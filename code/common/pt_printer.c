#include <stdint.h>
#include <stdio.h>
#include <string.h>

//
// Read total_size bytes from physical address @ start into virt buffer @ dest
// (Implementation specific)
//
int physread(uint32_t phys_start, void* dest, uint32_t total_size);

//
// mem allocation, again impl specific
//
void *allocate(uint32_t size);
void deallocate(void * p, uint32_t size);

#define errprint(...) printf(__VA_ARGS__)

//
// This is a control register that needs to be read and set
//
extern uint32_t sctlr;

static const char *cacheable(uint32_t flags) {
    const char *descs[] = {
        "Non-cacheable",
        "Write-Back, Write-Allocate",
        "Write-Through, no Write-Allocate",
        "Write-Back, no Write-Allocate"
    };
    return descs[flags];
}

static const char *tex(uint32_t tex, uint32_t c, uint32_t b) {
    if(tex & 4) {
        static char buf[1024];
        sprintf(buf, "Cacheable, Outer:%s, Inner:%s", cacheable(tex & 3), cacheable(((c & 1) << 1) | (b & 1)));
        return buf;
    }

    const char *descs[] = {
        "Strongly-ordered",
        "Shareable Device",
        "Outer and Inner Write-Through, no Write-Allocate",
        "Outer and Inner Write-Back, no Write-Allocate",
        "Outer and Inner Non-cacheable",
        "Reserved! 00101",
        "Implementation defined! 00110",
        "Outer and Inner Write-Back, Write-Allocate",
        "Non-shareable Device",
        "Reserved! 01001",
        "Reserved! 01010",
        "Reserved! 01011",
    };
    uint32_t thing = ((tex & 7) << 2) |
                     ((c & 1) << 1) |
                     (b & 1);
    if (thing > 11)
    {
        printf("thing=%d\n", thing);
        return "tex error";
    }
    return descs[thing];
}

static const char *ap(uint32_t ap) {
    const char *descs[] = {
        "X/X",
        "RW/X",
        "RW/RO",
        "RW/RW",
        "100!",
        "RO/X",
        "RO/RO [deprec.]",
        "RO/RO",
    };
    const char *simpledesc[] = {
        "RW/X",
        "RW/RW",
        "RO/X",
        "RO/RO"
    };

    int simple_mode = sctlr & (1<<29);
    
    if (simple_mode)
    {
        return simpledesc[(ap>>1) & 3];
    }
    
    return descs[ap & 7];
}

void armPtPrintPT(uint32_t l1desc, uint32_t virt_offset)
{
    int i, ret;
    uint32_t l2desc;
    //lower 10 bits are attributes
    uint32_t phys_base = (l1desc >> 10) << 10;
    uint32_t *buffer;
    const size_t size = 256 * sizeof(l2desc);

    printf("page table base=%08x P=%d domain=%d (%08x)\n", l1desc & ~0x3ff, (l1desc & (1 << 9)) & 1, (l1desc >> 5) & 0xf, l1desc);

    buffer = allocate(size);    
    ret = physread(phys_base, buffer, size);
    if (ret != 0)
    {
        errprint("could not read phys\n");
        goto out;
    }

    for (i = 0; i < 256; i++)
    {
        l2desc = buffer[i];
//        if (l2desc & 3)
//            printf("%d: %x\n", i, l2desc);
        switch(l2desc & 3) {
            case 0:
                //printf("fault\n");
                break;
            case 1:
                //
                // Note on usage of aps::  >> 7 & 4... this checks the 9th bit and returns 4 if it is set
                //  to preserve the ap[2] ap[1] ap[0] bit ordering
                //
                
                //large page (64kb)
                printf("virt=%08x large base=%08x [%s] XN=%x nG=%x S=%x AP=%s (%08x)\n",
                    virt_offset + ((i&0xf0)<<16),
                    l2desc & 0xffff0000,
                    tex(l2desc >> 12, l2desc >> 3, l2desc >> 2),
                    (l2desc >> 15) & 1,
                    (l2desc >> 11) & 1,
                    (l2desc >> 10) & 1,
                    ap(((l2desc >> 7) & 4) | ((l2desc >> 4) & 3)),
                    l2desc);
                break;
            case 2:
            case 3:
                //small page (4kb)
                printf("virt=%08x small base=%08x [%s] XN=%x nG=%x S=%x AP=%s (%08x)\n",
                    virt_offset + (i<<12),
                    l2desc & 0xfffff000,
                    tex(l2desc >> 6, l2desc >> 3, l2desc >> 2),
                    l2desc & 1,
                    (l2desc >> 11) & 1,
                    (l2desc >> 10) & 1,
                    ap(((l2desc >> 7) & 4) | ((l2desc >> 4) & 3)),
                    l2desc);
                break;
            }
    }
       
out:
    deallocate(buffer, size);
}

//
// size - the size of the level 1 descriptor array to read
// N - the ttbcr.N offset
// ttbr - the base register (can be ttbr0 or ttbr1)
// N2 - When ttbr1 is being passed in, N will be 0, but N2 will be set to ttbcr.N 
//      to let this routine know how to display virtual addresses
//
void armPtPrintTT(uint32_t size, uint32_t N, uint32_t ttbr, uint32_t N2)
{
    uint32_t phys_base;
    uint32_t *buffer;
    int ret, i;
    uint32_t level1;
    uint32_t virt_offset;
    
    phys_base = (ttbr >> (14-N)) << (14-N);
    
    virt_offset = 0;
    if (N2 != 0)
    {
        //
        // This is ttbr1, so set up virt_offset for the first
        // address translated from ttbr1
        //
        virt_offset = 0x100000000LL >> N2;
        printf("virt_offset = %x\n", virt_offset);
        virt_offset = 0;
    }
    
    buffer = allocate(size);    
    ret = physread(phys_base, buffer, size);
    if (ret != 0)
    {
        errprint("could not read phys\n");
        goto out;
    }
    
    for(i = 0; i < size/sizeof(level1); i++)
    {
        level1 = buffer[i];
        if ((level1 & 3) == 0){
            //skip faulting entry
            continue;
        }

        if ((level1 & 3) == 1)
        {
            //page table
            armPtPrintPT(level1, virt_offset + (i<<20));
        } else if (level1 & 2)
        {
            if (level1 & (1<<18))
            {
                //
                // super sections not supported, virt address is also i<<20 though
                //
                //super section -> 16MB memory region
                printf("virt=%08x supersection (!!! unexpected LPAE) base=%08x extbase=%x NS=%d nG=%d [%s] AP=%s extbase2=%x XN=%x (%08x)\n",
                    (i<<20),
                    level1 & 0xff000000,
                    (level1 >> 20) & 0xf,
                    (level1 >> 19) & 1,
                    (level1 >> 17) & 1,
                    tex(level1 >> 12, level1 >> 3, level1 >> 2),
                    ap((((level1 >> 15)&1) ? 4 : 0) | ((level1 >> 10) & 3)),
                    (level1 >> 5) & 0xf,
                    (level1 >> 4) & 1,
                    level1);
            }
            else
            {
                //
                // 31-N...20 are the virtual address bits (N only passed in for ttbr0)
                //  index is the virtual address
                //
                
                //section -> 1MB memory region
                printf("virt=%08x section base=%08x NS=%d nG=%d [%s] AP=%s domain=%x XN=%x (%08x)\n",
                    (i<<20) ,
                    level1 & 0xfff00000,
                    (level1 >> 19) & 1,
                    (level1 >> 17) & 1,
                    tex(level1 >> 12, level1 >> 3, level1 >> 2),
                    ap( (((level1 >> 15)&1) ? 4 : 0) | ((level1 >> 10) & 3)),
                    (level1 >> 5) & 0xf,
                    (level1 >> 4) & 1,
                    level1);
            }
        }
        
    }
    
out:
    deallocate(buffer, size);
}

void armPtPrint(uint32_t ttbcr, uint32_t ttbr0, uint32_t ttbr1)
{
    int N;

    if (ttbcr & (1<<31))
    {
        errprint("LPAE not supported");
        return;
    }

    N = ttbcr & 7;
    
    if (ttbr0 != 0)
    {
        printf("---- TTBR0 N= %d----\n", N);
        
    switch(N)
    {
        case 0:
            //
            // dump ttbr0, which is 16k
            //
            armPtPrintTT(16*1024, N, ttbr0, 0);
            break;
        case 1:
            armPtPrintTT(8*1024, N, ttbr0, 0);
            break;
        case 2:
            armPtPrintTT(4*1024, N, ttbr0, 0);
            break;
        case 3:
            armPtPrintTT(2*1024, N, ttbr0, 0);
            break;
        case 4:
            armPtPrintTT(1*1024, N, ttbr0, 0);
            break;
        case 5:
            armPtPrintTT(512, N, ttbr0, 0);
            break;
        case 6:
            armPtPrintTT(256, N, ttbr0, 0);
            break;
        case 7:
            armPtPrintTT(128, N, ttbr0, 0);
            break;
        default:
            break;
    }

    }

    if ((N != 0) && (ttbr1))
    {
        printf("---- TTBR1 ----\n");
        armPtPrintTT(16*1024, 0, ttbr1, N);
    }
}

#ifndef __NO_PTPRINTER_MAIN

int main(int argc, char *argv[])
{
    uint32_t ttbcr, ttbr0, ttbr1;

    ttbcr = GetTTBCR();
    ttbr0 = getTTBR0();
    ttbr1 = getTTBR1();
    
    //
    // XXX bit 9 of DFSR indicates if LPAE is on
    // if it is on, then this won't make sense, TTBR0 may be 64-bits, etc
    //
    
    armPtPrint(ttbcr, ttbr0, ttbr1);
}

#endif /* __NO_PTPRINTER_MAIN */
