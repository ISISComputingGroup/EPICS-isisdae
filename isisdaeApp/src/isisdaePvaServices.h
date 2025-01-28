class isisdaeInterface;
class isisdaePvaServicesImpl;

class isisdaePvaServices
{
    isisdaePvaServicesImpl* m_services;
    public:
    isisdaePvaServices(isisdaeInterface* iface);    
    ~isisdaePvaServices();
};
