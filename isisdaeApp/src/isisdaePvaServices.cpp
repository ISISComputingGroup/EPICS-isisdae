#include <pv/pvData.h>
#include <pv/rpcServer.h>

#include "isisdaeInterface.h" 
#include "isisdaePvaServices.h" 

using namespace epics::pvData;
using namespace epics::pvAccess;

static Structure::const_shared_pointer resultStructure =
    getFieldCreate()->createFieldBuilder()->
    addArray("values", pvInt)->
    createStructure();

// s1 starts with s2 check
static bool starts_with(const std::string& s1, const std::string& s2) {
    return s2.size() <= s1.size() && s1.compare(0, s2.size(), s2) == 0;
}

class QXReadArrayServiceImpl :
    public RPCServiceAsync
{
    isisdaeInterface* m_iface;
    public:
    QXReadArrayServiceImpl(isisdaeInterface* iface) : RPCServiceAsync(), m_iface(iface) { }
    private:
    void request(PVStructure::shared_pointer const & pvArguments,
                 RPCResponseCallback::shared_pointer const & callback)
    {
        
               
        // NTURI support
        PVStructure::shared_pointer args(
            (starts_with(pvArguments->getStructure()->getID(), "epics:nt/NTURI:1.")) ?
            pvArguments->getSubField<PVStructure>("query") :
            pvArguments
        );

        // get fields and check their existence
        PVScalar::shared_pointer card_id_f = args->getSubField<PVScalar>("card_id");
        PVScalar::shared_pointer card_address_f = args->getSubField<PVScalar>("card_address");
        PVScalar::shared_pointer trans_type_f = args->getSubField<PVScalar>("trans_type");
        PVScalar::shared_pointer num_values_f = args->getSubField<PVScalar>("num_values");
        if (!card_id_f || !card_address_f || !trans_type_f || !num_values_f)
        {
            callback->requestDone(
                Status(
                    Status::STATUSTYPE_ERROR,
                    "card_id, card_address, num_values and trans_type are required"
                ),
                PVStructure::shared_pointer());
            return;
        }

        unsigned card_id, card_address, trans_type, num_values;
        try
        {
            card_id = card_id_f->getAs<unsigned>();
            card_address = card_address_f->getAs<unsigned>();
            trans_type = trans_type_f->getAs<unsigned>();
            num_values = num_values_f->getAs<unsigned>();
        }
        catch (std::exception &e)
        {
            callback->requestDone(
                Status(
                    Status::STATUSTYPE_ERROR,
                    std::string("failed to convert arguments to unsigned long: ") + e.what()
                ),
                PVStructure::shared_pointer());
            return;
        }

        // create return structure and set data
        std::vector<long> values;
        try {
            m_iface->QXReadArray(card_id, card_address, values, num_values, trans_type);
        }
        catch (std::exception &e)
        {
            callback->requestDone(
                Status(
                    Status::STATUSTYPE_ERROR,
                    std::string("failed to call QXReadArray: ") + e.what()
                ),
                PVStructure::shared_pointer());
            return;
        }
        std::tr1::shared_ptr<int> values_v(new int[num_values]);
        std::copy(values.begin(), values.end(), values_v.get());
        PVIntArray::const_svector values_sv(values_v, 0, num_values);

        PVStructure::shared_pointer result = getPVDataCreate()->createPVStructure(resultStructure);
        
        result->getSubField<PVIntArray>("values")->replace(values_sv);

        callback->requestDone(Status::Ok, result);
    }
};

class isisdaePvaServicesImpl
{
    isisdaeInterface* m_iface;
    std::tr1::shared_ptr<RPCServer> m_server;
    public:
    isisdaePvaServicesImpl(isisdaeInterface* iface, const char* pvprefix) : m_iface(iface), m_server(new RPCServer)
    {
        std::string service_name = std::string(pvprefix != NULL ? pvprefix : "") + "RPC:QXReadArray";
        try {
            m_server->registerService(service_name, RPCServiceAsync::shared_pointer(new QXReadArrayServiceImpl(m_iface)));
            m_server->printInfo();
            m_server->runInNewThread();
        }
        catch(const std::exception& ex)
        {
            std::cerr << "isisdaePvaServicesImpl failed: " << ex.what() << std::endl;
        }
    }
};

isisdaePvaServices::isisdaePvaServices(isisdaeInterface* iface, const char* pvprefix) : m_services(new isisdaePvaServicesImpl(iface, pvprefix))
{
}

isisdaePvaServices::~isisdaePvaServices()
{
    delete m_services;
}
