
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

/*
void get_regs(void *proc, void **destp, void *ret) {
    void *dest;
    uint32_t slide;
    slide = *(destp+1);
    dest = *(destp+2);

    void (*IOLog)(char *fmt, ...);
    
    IOLog = slide + 0x8023efc8|1;

    struct regs r;
    uint32_t *ttbr_ptrs; 
    asm("mrs %0, cpsr" :"=r"(r.cpsr));
    asm("mrc p15, 0, %0, c2, c0, 0" :"=r"(r.ttbr0));
    asm("mrc p15, 0, %0, c2, c0, 1" :"=r"(r.ttbr1));
    asm("mrc p15, 0, %0, c2, c0, 2" :"=r"(r.ttbcr));
    asm("mrc p15, 0, %0, c13, c0, 1" :"=r"(r.contextidr));
    asm("mrc p15, 0, %0, c1, c0, 0" :"=r"(r.sctlr));
    //asm("mcr p15, 0, %0, c1, c1, 0" :: "r"(1 << 6));
    asm("mrc p15, 0, %0, c1, c1, 0" :"=r"(r.scr));
    asm("mrc p14, 0, %0, c0, c0, 0" :"=r"(r.dbgdidr));
    asm("mrc p14, 0, %0, c1, c0, 0" :"=r"(r.dbgdrar));
    asm("mrc p14, 0, %0, c2, c0, 0" :"=r"(r.dbgdsar));
    asm("mrc p15, 0, %0, c0, c1, 2" :"=r"(r.id_dfr0));
    asm("mrc p14, 0, %0, c0, c1, 0" : "=r"(r.dbgdscr));
    asm("mrc p15, 0, %0, c13, c0, 4" : "=r"(r.tpidrprw));
    asm("mrc p15, 0, %0, c3, c0, 0" : "=r"(r.dacr));
    asm("mrc p15, 0, %0, c13, c0, 4" : "=r"(ttbr_ptrs));

    IOLog("cpsr = %x\n", r.cpsr);
    IOLog("ttbr0 = %x\n", r.ttbr0);
    IOLog("ttbr1 = %x\n", r.ttbr1);
    IOLog("ttbcr = %x\n", r.ttbcr);
    IOLog("dacr = %x\n", r.dacr);


//80019de2        ee1d0f90        mrc     p15, #0, r0, c13, c0, #4
//0x4b0 - user mapping
//0x4b4 -- restored ttbr0 (before copyout)
//0x4b8 - saved ttbr1 mapping
    IOLog("user ttbr0 mapping @ %x\n", ttbr_ptrs[0x4b0/4]);
    IOLog("saved ttbr0 mapping @ %x\n", ttbr_ptrs[0x4b4/4]);
    IOLog("copyout ttbr1 mapping @ %x\n", ttbr_ptrs[0x4b8/4]);

    //
    //now read out stored ttbr pointers
    //
    
    IOLog("hello, will copy data to %x\n", dest);
}
*/

inline
void Zmemcpy(char *dst, char *src, uint32_t len);

static const char *ap(uint32_t ap);

static const char *tex(uint32_t tex, uint32_t c, uint32_t b);

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