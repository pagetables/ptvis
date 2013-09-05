/* add your code here */
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>

enum {
    kDumpRegisters,
    kDumpPhysMem,
    kDumpVirtMem,
    kWalkThreadPML4s
};


class ptdump_Driver : public IOService
{
    OSDeclareDefaultStructors(ptdump_Driver)
public:
    virtual bool init(OSDictionary *dictionary = 0);
    virtual void free(void);
    virtual IOService *probe(IOService *provider, SInt32 *score);
    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);
};



class ptdump_DriverUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(ptdump_DriverUserClient);
    
public:
    bool                   initWithTask(task_t owningTask, void *securityID, UInt32 type);
    virtual bool           start(IOService *provider);
    IOReturn               externalMethod(uint32_t selector,
                                           IOExternalMethodArguments * arguments,
                                           IOExternalMethodDispatch * dispatch = 0,
                                           OSObject * target = 0,
                                           void * reference = 0);
    IOReturn               clientClose(void);
private:
    task_t                 _task;
    void*                  _buffer;
    vm_size_t              _buffer_length;
    IOReturn               DumpRegisters(char *output);
    IOReturn               WalkThreadPML4s(char *output);
    IOReturn               DumpMem(IOOptionBits type, void *start, uint64_t length, void **output, uint64_t *out_size);
    /*
    IOReturn               DumpTaskVirtMem(uint64_t pid, void *start, uint64_t length, void *output);
    */
    
};

