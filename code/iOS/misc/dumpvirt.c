
#include <stdio.h>
#include <stdint.h>
//8025df30 T IOMemoryDescriptor::withPhysicalAddress(unsigned long, unsigned long, IODirection)
//80261364 T IOMemoryDescriptor::map(unsigned long)

/// xxx -> might need fixed metacall shit for this one
//80261008 T IOMemoryMap::getAddress()

//80088060 T _copyin
//80088144 T _copyout
//8023efc8 T _IOLog



struct regs {
    uint32_t cpsr;
    uint32_t ttbr0;
    uint32_t ttbr1;
    uint32_t ttbcr;
    uint32_t contextidr;
    uint32_t sctlr;
    uint32_t scr;
    uint32_t dbgdidr;
    uint32_t dbgdrar;
    uint32_t dbgdsar;
    uint32_t id_dfr0;
    uint32_t dbgdscr;
    uint32_t tpidrprw;
    uint32_t dacr;
};

typedef enum IODirection { 
    kIODirectionNone = 0, 
    kIODirectionIn = 1, // User land 'read' 
    kIODirectionOut = 2, // User land 'write' 
    kIODirectionOutIn = 3 
} IODirection; 

#define FIXED_METACALL(typ, num, object, args...) ({ \
    void *_o = (object); \
    ((typ (***)(void *, ...)) _o)[0][(num)](_o, ##args); \
})

static inline void OSObject_release(void *object) {
    return FIXED_METACALL(void, 2, object);
}

static inline void *OSObject_retain(void *object) {
    return FIXED_METACALL(void *, 4, object);
}

static inline void *IOMemoryMap_getAddress(void *map) {
    return (void *) (uint32_t) FIXED_METACALL(uint64_t, 24, map);
}


void get_regs(void *proc, void **destp, void *ret) {
    void *dest;
    uint32_t slide;
    slide = *(destp+1);
    dest = *(destp+2);
    uint32_t *ptr;
    int i;

    void (*IOLog)(char *fmt, ...);
    
    IOLog = slide + 0x8023efc8|1;

    //
    // read and print some virt mem addresses
    //
    ptr = dest;
    IOLog("Dumping 8 words from %x (proc @ %p)\n", ptr, proc);
    for(i = 0; i < 8; i++)
    {
        IOLog("%d: %08x\n", i, ptr[i]);
    }
    IOLog("Done\n", dest);
}

/*
inline
void Zmemcpy(char *dst, char *src, uint32_t len);

int poke_mem(void *proc, void **destp, void *ret)
//void *kaddr, uint32_t uaddr, uint32_t size, int write, int phys, int slide) 
{
    void *kaddr = *(destp+1);
    uint32_t uaddr = *(destp+2);
    uint32_t size = *(destp+3);
    int slide = *(destp+4);
    int write = 0;
    void *pkaddr;
    
    void *(*IOMemoryDescriptor_withAddressRange)(unsigned long long, unsigned long long, unsigned long, void*);
    void *(*IOMemoryDescriptor_readbytes)(void*, unsigned long, void*, unsigned long);
    void *(*copyin)(uint32_t, uint32_t, uint32_t);
    void *(*copyout)(uint32_t, uint32_t, uint32_t);
    void (*IOLog)(char *fmt, ...);
    IOMemoryDescriptor_withAddressRange = slide + 0x8025de98|1;
    IOMemoryDescriptor_readbytes = slide + 0x8025EA30|1;
    copyin = slide + 0x80088060|1;
    copyout = slide + 0x80088144|1;
    IOLog = slide + 0x8023efc8|1;

    void *descriptor = 0, *map = 0;
    uint32_t retval;
    descriptor = IOMemoryDescriptor_withAddressRange((uint32_t) kaddr, size, 0, NULL);

    if(!descriptor) {
        IOLog("couldn't create descriptor\n");
        return -1;
    }
    retval = IOMemoryDescriptor_readbytes(descriptor, 0, (void*)uaddr, size);
    IOLog("got ret= %x for readbytes from %x", retval, kaddr);

    if (descriptor != NULL)
    {
        OSObject_release(descriptor);
    }
    return 0;
}

inline
void Zmemcpy(char *dst, char *src, uint32_t len)
{
    uint32_t i;
    for(i = 0; i < len; i++) dst[i] = src[i];
}
int main(){}
*/