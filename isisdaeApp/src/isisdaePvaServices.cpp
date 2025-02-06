#include <pvxs/sharedpv.h>
#include <pvxs/server.h>
#include <pvxs/nt.h>
#include <pvxs/log.h>
#include <functional>

#include "isisdaeInterface.h" 
#include "isisdaePvaServices.h" 

using namespace pvxs;


// s1 starts with s2 check
static bool starts_with(const std::string& s1, const std::string& s2) {
    return s2.size() <= s1.size() && s1.compare(0, s2.size(), s2) == 0;
}

#if 0
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

#endif


class isisdaePvaServicesImpl
{
    isisdaeInterface* m_iface;
    server::Server m_server;
    server::SharedPV m_pv;
    public:
    isisdaePvaServicesImpl(isisdaeInterface* iface, const char* pvprefix) : m_iface(iface), m_server(server::Server::fromEnv()), m_pv(server::SharedPV::buildReadonly())
    {
        std::string service_name = std::string(pvprefix != NULL ? pvprefix : "") + "RPC:QXReadArray";
        try {
//            m_pv.onRPC(std::move(std::bind(&isisdaePvaServicesImpl::onRPC, this, std::ref(std::placeholders::_1), std::move(std::placeholders::_2), std::move(std::placeholders::_3))));
            m_pv.onRPC(std::bind(&isisdaePvaServicesImpl::onRPC, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            m_server.addPV(service_name, m_pv);
            epicsThreadCreate("rpc", epicsThreadPriorityMedium, epicsThreadStackMedium, &isisdaePvaServicesImpl::runRPC, this);
        }
        catch(const std::exception& ex)
        {
            std::cerr << "isisdaePvaServicesImpl failed: " << ex.what() << std::endl;
        }
    }
    
    static void runRPC(void* arg)
    {
        isisdaePvaServicesImpl* impl = (isisdaePvaServicesImpl*)arg;
        impl->runRPC();
    }
    
    void runRPC()
    {
        m_server.run();
    }

    void onRPC(server::SharedPV& pv, std::unique_ptr<server::ExecOp>&& op, Value&& top)
    {
        auto reply(nt::NTScalar{TypeCode::Float64}.create());
        try {
            // assume arguments encoded NTURI
 //           auto rhs = top["query.rhs"].as<double>();
  //          auto lhs = top["query.lhs"].as<double>();

            reply["value"] = 1;

        } catch(std::exception& e){
            reply["alarm.severity"] = 0; //INVALID_ALARM;
            reply["alarm.message"] = e.what();
        }

        op->reply(reply);
        // Scale-able applications may reply outside of this callback,
        // and from another thread.
    }
};

isisdaePvaServices::isisdaePvaServices(isisdaeInterface* iface, const char* pvprefix) : m_services(new isisdaePvaServicesImpl(iface, pvprefix))
{
}

isisdaePvaServices::~isisdaePvaServices()
{
    delete m_services;
}
