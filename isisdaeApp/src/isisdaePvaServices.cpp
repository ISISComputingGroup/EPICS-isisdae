#include <pvxs/sharedpv.h>
#include <pvxs/server.h>
#include <pvxs/nt.h>
#include <pvxs/log.h>
#include <functional>

#include <alarm.h>

#include "isisdaeInterface.h" 
#include "isisdaePvaServices.h" 

using namespace pvxs;


// s1 starts with s2 check
static bool starts_with(const std::string& s1, const std::string& s2) {
    return s2.size() <= s1.size() && s1.compare(0, s2.size(), s2) == 0;
}

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
            Value initial = nt::NTScalar{TypeCode::String}.create();
            initial["value"] = "RPC only";
            m_pv.open(initial);
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
        auto reply(nt::NTScalar{TypeCode::UInt32A}.create());
        try {
            // assume arguments encoded NTURI
            auto card_id = top["query.card_id"].as<unsigned>();
            auto card_address = top["query.card_address"].as<unsigned>();
            auto trans_type = top["query.trans_type"].as<unsigned>();
            auto num_values = top["query.num_values"].as<unsigned>();
            std::vector<long> values;
            m_iface->QXReadArray(card_id, card_address, values, num_values, trans_type);            
            shared_array<unsigned> arr(values.begin(), values.end());
            reply["value"] =  arr.freeze();
        } catch(std::exception& e){
            reply["alarm.severity"] = INVALID_ALARM;
            reply["alarm.message"] = e.what();
        }
        op->reply(reply);
    }
};

isisdaePvaServices::isisdaePvaServices(isisdaeInterface* iface, const char* pvprefix) : m_services(new isisdaePvaServicesImpl(iface, pvprefix))
{
}

isisdaePvaServices::~isisdaePvaServices()
{
    delete m_services;
}
