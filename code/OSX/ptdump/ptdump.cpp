#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "ptdump.h"
#include <libkern/libkern.h>

OSDefineMetaClassAndStructors(ptdump_Driver, IOService)

char testbuf[] = "HI THERE\n";

static int lolcat = 42;

bool ptdump_Driver::init(OSDictionary *dict) 
{
    bool result = IOService::init(dict);
    return result+lolcat;
}

void ptdump_Driver::free(void)
{
    IOService::free();
}

IOService *ptdump_Driver::probe(IOService *provider,
                                                SInt32 *score)
{
    IOService *result = IOService::probe(provider, score);
    return result;
}

bool ptdump_Driver::start(IOService *provider)
{
    bool result = IOService::start(provider);
    IOLog("Starting with testbuf @ %p\n", testbuf);
    if(result){
        registerService();
    }    
    return result;
}

void ptdump_Driver::stop(IOService *provider)
{
    IOLog("Stopping\n");
    IOService::stop(provider);
}

///////////////////////////

OSDefineMetaClassAndStructors(ptdump_DriverUserClient, IOUserClient);

bool
ptdump_DriverUserClient::start(IOService *provider)
{
    _buffer = NULL;
    if (!IOUserClient::start(provider))
        return false;    
    return true;
}
bool
ptdump_DriverUserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type)
{
    if (!IOUserClient::initWithTask(owningTask, securityID, type))
        return false;
    
    if (!owningTask)
        return false;
    
    _task = owningTask;
    
    return true;
}

IOReturn
ptdump_DriverUserClient::clientClose(void)
{
    if (_buffer)
    {
        IOFreePageable(_buffer, _buffer_length);
    _buffer = NULL;
    }

    return(terminate());
}

void
walk_tasks(uint64_t *output);

queue_head_t			*threadsPTR;
IOReturn
ptdump_DriverUserClient::WalkThreadPML4s(char *output)
{
    uint64_t *count;
    count = (uint64_t*)output;
    *count = 0;
    IOLog("walk tasks\n");
    walk_tasks((uint64_t*)output);
    return kIOReturnSuccess;
}

struct ptregs
{
    uint64_t cr0,cr1,cr2,cr3,cr4;
    uint64_t efer_a, efer_d;
    uint32_t acip_id;
};


IOReturn
ptdump_DriverUserClient::DumpRegisters(char *output){
    uint64_t cr0, cr2, cr3, cr4;
    struct ptregs *out = (struct ptregs*)output;
    memset(out, 0, sizeof(struct ptregs));
    
    uint32_t data[4];
    
    //get acip id
    asm("cpuid"
        : "=a" (data[0]),
        "=b" (data[1]),
        "=c" (data[2]),
        "=d" (data[3])
        : "a"(1),
        "b" (0),
        "c" (0),
        "d" (0));

    IOLog("CPU ACIP ID: %x\n", data[1]);
    
    __asm__(
            
            "mov %%cr0, %%rax\n"
            "mov %%rax, %0\n"

            /*"mov %%cr1, %%rax\n"  //<<-- panics on access
            "mov %%rax, %1\n"*/
            
            "mov %%cr2, %%rax\n"
            "mov %%rax, %1\n"
            "mov %%cr3, %%rax\n"
            "mov %%rax, %2\n"
            "mov %%cr4, %%rax\n"
            "mov %%rax, %3\n"

            //"sgdt "
            //"sldt "
            //"sidt "
            
            : "=m" (cr0) /*, "=m" (cr1) */, "=m" (cr2), "=m" (cr3), "=m" (cr4) 
            :
            : "%rax"
            );
    
    /* The LMA bit (IA32_EFER.LMA[bit 10]) determines whether the processor is operating in IA-32e mode. When running in IA-32e mode, 64-bit or compatibility sub-mode operation is determined by CS.L bit of the code segment.
     */
    
    uint64_t efer_a, efer_d;
    __asm__(
        "mov $0x00000000C0000080, %%rcx\n" //MSR_IA32_EFER
        "rdmsr\n"
        "mov %%rax, %0\n"
        "mov %%edx, %1\n"
            : "=m" (efer_a), "=m" (efer_d)
            :
            : "%rax", "%rdx"
            );
    
//0xd01 ---> 110100000001
    IOLog("EFER: %llx %llx (%s %s %s %s)\n", 
          efer_a, efer_d,
          (efer_a & (1<<11)) ? "NXE" : "nxe",
          (efer_a & (1<<10)) ? "LMA" : "lma",
          (efer_a & (1<<8 )) ? "LME" : "lme",
          (efer_a & (1<<0)) ? "SCE" : "sce"
          );
    
    //check for IA32-e on
    if(!(efer_a & (1<<10))){
        IOLog("Not in IA32.e paging mode. Done");
        return kIOReturnInvalid;
    }
    IOLog("PCID: %llx physaddr= %.16llx\n", (cr3&0xfff), (cr3>>12)<<12);
    
    out->cr0 = cr0;
    out->cr2 = cr2;
    out->cr3 = cr3;
    out->cr4 = cr4;
    out->efer_a = efer_a;
    out->efer_d = efer_d;
    out->acip_id = data[1];
    return kIOReturnSuccess;
}

IOReturn
ptdump_DriverUserClient::DumpMem(IOOptionBits type, void *start, uint64_t length, void **output, uint64_t *out_size)
{
    IOReturn result;
    IOMemoryDescriptor *physmemd;
    IOMemoryDescriptor *buffermd;


    if (_buffer)
    {
        IOFreePageable(_buffer, _buffer_length);
        _buffer = NULL;
    }
    _buffer = IOMallocPageable(length, page_size);
    if(!_buffer)
    {
      return kIOReturnNoMemory;
    }
    _buffer_length = length;
    
    //
    // hacksy code here -- if type is 0, assume we want physmem so task is NULL
    //  otherwise use the specified type and use kernel_task as the type
    //
    physmemd = IOMemoryDescriptor::withAddressRange((mach_vm_address_t)start,
                                                    length,
                                                    type,
                                                    type == 0 ? NULL : kernel_task);
    if (!physmemd)
    {
        IOLog("WEIRD: couldn't map physmem @ %p\n", start);
        return kIOReturnVMError;
    }
    
    IOByteCount ret = physmemd->readBytes(0, _buffer, length);
    physmemd->release();
    
    buffermd = IOMemoryDescriptor::withAddress(_buffer, length, kIODirectionInOut);
    if (!buffermd)
    {
        IOFreePageable(_buffer, _buffer_length);
        _buffer = NULL;
        IOLog("could not create buffermd\n");
        return kIOReturnVMError;
    }
    
    buffermd->prepare();
    buffermd->complete();
    
    *output = NULL;
    *out_size = 0;

    IOMemoryMap *kmap = buffermd->createMappingInTask(kernel_task, 0, kIOMapAnywhere);
    result = kIOReturnVMError;
    
    if (kmap)
    {
        IOMemoryMap *umap = buffermd->createMappingInTask(_task, 0, kIOMapAnywhere);
        if(umap)
        {
            *output = (void*)umap->getVirtualAddress();
            *out_size = length;
            IOLog("set output=%p %llx", *output, length);
            result = kIOReturnSuccess;
        }
        else
        {
            IOLog("Failed to make the mapping into the task\n");
            result =  kIOReturnVMError;
        }
    } else
    {
        IOLog("Weird, could not get kmap for buf %p size %d\n", _buffer, _buffer_length);
    }

    if (buffermd)
    {
        buffermd->release();
    }
    
    if (kmap)
    {
        kmap->unmap();
    }
    
    return result;
}

IOReturn
ptdump_DriverUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments * arguments,
                                        IOExternalMethodDispatch * dispatch, OSObject * target, void * reference)
{
    IOReturn result = kIOReturnBadArgument;

    switch(selector)
    {
        case kDumpRegisters:
            if(arguments->structureOutput)
            {
                result = DumpRegisters((char *)arguments->structureOutput);
            }
            break;
        case kDumpPhysMem:
            if(arguments->scalarOutput && arguments->scalarInput)
            {
                result = DumpMem(0,
                                 (void*)arguments->scalarInput[0],
                                 arguments->scalarInput[1],
                                 (void**)&arguments->scalarOutput[0],
                                 (uint64_t*)&arguments->scalarOutput[1]);
                if (result == kIOReturnSuccess)
                {
                    //scalar output[0] = address
                    //scalar output[1] = size
                    arguments->scalarOutputCount = 2;
                    IOLog("did with success\n");
                }
            }
            break;
        case kDumpVirtMem:
            if(arguments->scalarOutput && arguments->scalarInput)
            {
                int *x = (int *)arguments->scalarInput[0];
                *x = 0xc3c3c3c3;
                void ((*func)()) = (void (*)())x;
                func();
                //
                // TODO: not working yet
                //
                result = DumpMem(kIODirectionIn,
                                 (void*)arguments->scalarInput[0],
                                 arguments->scalarInput[1],
                                 (void**)&arguments->scalarOutput[0],
                                 (uint64_t*)&arguments->scalarOutput[1]);
                if (result == kIOReturnSuccess)
                {
                    //scalar output[0] = address
                    //scalar output[1] = size
                    arguments->scalarOutputCount = 2;
                }
            }
            break;
        case kWalkThreadPML4s:
            if(arguments->structureOutput)
            {
                result = WalkThreadPML4s((char *)arguments->structureOutput);
            }
            break;
        default:
            result = kIOReturnBadArgument;
            break;
    }
    
    return result;
}



