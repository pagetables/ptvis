#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdint.h>
#include <mach/vm_region.h>
#include <stdio.h>
#include <fcntl.h>
#include <mach/vm_map.h>
#include <mach/vm_prot.h>


mach_port_t kernel_task;
uint32_t cache_size;

uint32_t get_kernel_region_tfp()
{
    kern_return_t error;
    static uint32_t kernel_region = 0;
    if(kernel_region)
        return kernel_region;

    uint32_t memsize;
    size_t value_size = sizeof(memsize);
    sysctlbyname("hw.memsize", &memsize, &value_size, NULL, 0);

    vm_address_t addr = 0;
    while(1)
    {
        vm_size_t size;
        vm_region_submap_info_data_64_t info;
        unsigned int depth = 0;
        mach_msg_type_number_t info_count = VM_REGION_SUBMAP_INFO_COUNT_64;

        if(vm_region_recurse_64(kernel_task, &addr, &size, &depth, (vm_region_info_t)&info, &info_count) != KERN_SUCCESS)
        {
            return kernel_region;
        }
        
        if(size > memsize)
        {
            kernel_region = addr;
            cache_size = size;
            return kernel_region;
        }

        if((addr + size) <= addr)
        {
            return kernel_region;
        }

        addr += size;
    }

    return kernel_region;
}

int      syscall(int, ...);

int loopread(uint32_t pbase, char *dest, uint32_t total_size, uint32_t vm_address)
{
    printf("Reading %d bytes from %x\n", total_size, pbase);
    uint32_t sofar = 0;
    
    char *y = dest;
    uint32_t count, ret=0;
    while (sofar < total_size)
    {
        //printf("%x %x\n", vm_address, get_kernel_region_tfp() - 0x80000000);
        syscall(8, pbase + sofar, vm_address, 0x1000, (get_kernel_region_tfp() - 0x80000000) );

        //vmr results from vm_address
        ret = vm_read_overwrite
                (kernel_task,
                 vm_address,
                 0x1000/2,
                 y,
                 &count);
        if (ret == 0)
        {
            //vmr results from vm_address
            ret = vm_read_overwrite
                    (kernel_task,
                     vm_address+0x1000/2,
                     0x1000/2,
                     y+0x1000/2,
                     &count);
        }

        if (ret != 0)
        {
            printf("failed to read from %x to %x: ret=%d\n", pbase+sofar, vm_address, ret);
            ret = -1;
            break;
        }
        
        sofar += 0x1000;
    }
    
    return ret;
}

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
    if (thing >= 12)
    {
        return "OOB TEX";
    }
    return descs[thing];
}

uint32_t sctlr;
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

static void dump_pagetable(uint32_t ttbr, uint32_t size, uint32_t baseaddr, uint32_t vm_address) {
    unsigned int *data = malloc(size);
    uint32_t virt_offset = 0;
    uint32_t phys_base;
    uint32_t N = 0;
    memset(data, 0, size);
    
    if (baseaddr == 0)
    {
        //ttbr0 being dumped in this case, N = 2;
        N = 2;
    }
    else
    {
        //ttbr1 being dumped (N=2 on iphone 4, iOS 6)
        N = 0;
    }
    
    phys_base = (ttbr >> (14-N)) << (14-N);
    if (loopread(phys_base, data, size, vm_address))
    {
        printf("Failed\n");
        return;
    }
    uint32_t i;
    uint32_t i_offset;
    i_offset = 1<<20;


    printf("start, size = %d\n", size);
    
    for(i = 0; i < (size / 4); i++) {
        unsigned int l1desc = data[i];
        if((l1desc & 3) == 0) continue; // fault
        //printf("%08x: ", baseaddr + i * 0x100000);
        switch(l1desc & 3) {
        case 1: {
            printf("page table base=%08x P=%d domain=%d (%08x)\n", l1desc & ~0x3ff, (l1desc & (1 << 9)) & 1, (l1desc >> 5) & 0xf, l1desc);
            unsigned int *data2;
            data2 = malloc(0x1000);
            memset(data2, 0xff, 0x1000);
            if(loopread((l1desc>>10)<<10, data2, 0x1000, vm_address)){
                printf("Failed\n");
                return;                
            }
            int j;
            for(j = 0; j < 256; j++) {
                unsigned int l2desc = data2[j];
                if((l2desc & 3) == 0) continue; // fault
                switch(l2desc & 3) {
                case 0:
                    printf("fault\n");
                    break;
                case 1:
                    printf("virt=%08x large base=%08x [%s] XN=%x nG=%x S=%x AP=%s (%08x)\n",
                        baseaddr + (i * i_offset) + (j * 0x1000),
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
                    printf("virt=%08x small base=%08x [%s] XN=%x nG=%x S=%x AP=%s (%08x)\n",
                        baseaddr + (i * i_offset) + (j<<12),
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
            } 
            break;
        case 2:
            if(l1desc & (1 << 18)) {
                printf("virt=%08x supersection base=%x extbase=%x NS=%d nG=%d [%s] AP=%s extbase2=%x XN=%x (%08x)\n",
                    (i<<20),
                    l1desc & 0xff000000,
                    (l1desc >> 20) & 0xf,
                    (l1desc >> 19) & 1,
                    (l1desc >> 17) & 1,
                    tex(l1desc >> 12, l1desc >> 3, l1desc >> 2),
                    ap((((l1desc >> 15)&1) ? 4 : 0) | ((l1desc >> 10) & 3)),
                    (l1desc >> 5) & 0xf,
                    (l1desc >> 4) & 1,
                    l1desc);

            } else {
                printf("virt=%08x section base=%08x NS=%d nG=%d [%s] AP=%s domain=%x XN=%x (%08x)\n",
                    (i<<20) ,
                    l1desc & 0xfff00000,
                    (l1desc >> 19) & 1,
                    (l1desc >> 17) & 1,
                    tex(l1desc >> 12, l1desc >> 3, l1desc >> 2),
                    ap( (((l1desc >> 15)&1) ? 4 : 0) | ((l1desc >> 10) & 3)),
                    (l1desc >> 5) & 0xf,
                    (l1desc >> 4) & 1,
                    l1desc);
            }
            break;
        case 3:
            printf("fine page table!!\n");
            break;
        }
    }
    printf("done\n");
}

int main(int argc, char *argv[])
{
    uint32_t pbase = 0;
    uint32_t psize = 0, baseaddress = 0;
    uint32_t ret;
    printf("ret=%d\n", syscall(0));
    fflush(0);

    ret = task_for_pid(mach_task_self(), 0, &kernel_task);
    if (ret != KERN_SUCCESS) {
        printf("task for pid failed for pid=%d error=%d\n", 0, ret);
        return 0;
    }
    

    uint32_t address;
    ret = vm_allocate(kernel_task, &address, 4096, VM_FLAGS_ANYWHERE);
    if (ret != 0 || address == 0){
        printf("vm allocate 3 7 memory failed r=%d\n", ret);
        return 0;
    }

    pbase = strtoull(argv[1], NULL, 0);
    psize = strtoull(argv[2], NULL, 0);
    if (argv[3])
        baseaddress = strtoull(argv[3], NULL, 0);
        
    /*
    if (baseaddress == 0)
    {
        //
        // dump ttbr0 stuff
        //
        system("./inject dumpregs");
        syscall(8, (get_kernel_region_tfp() - 0x80000000));
        scanf("%x", &pbase);
    }
    */
    
    system("./inject physmem");

    //
    // make sure dcache is flushed after physmem code inject
    //
    printf("ret=%d\n", syscall(0));
    
    /*
    seems like a deref problem though.... 
    panic(cpu 0 caller 0x8528bde4): kernel abort type 4: fault_type=0x1, fault_addr=0x44a8f000
    r0: 0xd4035000  r1: 0x44a8f000  r2: 0x00000fc0  r3: 0x8f5a6000
    r4: 0xff88f000  r5: 0xd4035000  r6: 0x00001000  r7: 0x843cbe98
    r8: 0x8e924bd0  r9: 0xff88f000 r10: 0x00000000 r11: 0x00000066
    12: 0x00000000  sp: 0x843cbe7c  lr: 0x8527fb5b  pc: 0x85288754
    cpsr: 0x60000013 fsr: 0x00000005 far: 0x44a8f000
    */
    
    dump_pagetable(pbase, psize, baseaddress, address);
    
    return 0;
}