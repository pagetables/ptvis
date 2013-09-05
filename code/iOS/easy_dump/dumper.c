#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <mach/vm_region.h>
#include <mach/vm_map.h>
#include <mach/vm_prot.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/task_info.h>
#include <mach/thread_info.h>
#include <mach/thread_act.h>
#include <mach/vm_region.h>
#include <mach/vm_map.h>
#include <mach/vm_prot.h>
#include <mach/task.h>
#include <sys/sysctl.h>

void armPtPrint(uint32_t ttbcr, uint32_t ttbr0, uint32_t ttbr1);

uint32_t sctlr = 0;

mach_port_t kernel_task;

uint32_t SHIFTPHYS;
//
// Read total_size bytes from physical address @ start into virt buffer @ dest
// (Implementation specific)
//
int
physread(uint32_t phys_start, char* dest, uint32_t total_size)
{
    char *cmd;
    int ret;

    ret = asprintf(&cmd, "./vmr 0x%x 0x%x >/dev/null", phys_start + SHIFTPHYS, phys_start + SHIFTPHYS + total_size);
    system(cmd);
    free(cmd);
    
    ret = asprintf(&cmd, "kmem/%.8x.mem", phys_start + SHIFTPHYS);
    int fd;
    fd = open(cmd, O_RDONLY);
    free(cmd);
    if (fd == -1)
        return fd;

    ret = read(fd, dest, total_size);
    if (ret != total_size)
    {
        printf("bad read: %d\n", ret);
        close(fd);
        return 1;
    }
    
    close(fd);
   
    return 0;
}

uint32_t get_kernel_region_tfp(task_t kernel_task)
{
    static uint32_t kernel_region = 0;
    //kernel_region = 0x8f400000;
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

//
// mem allocation, again impl specific
//
void *allocate(uint32_t size)
{
    return malloc(size);
}
void deallocate(void * p, uint32_t size)
{
    free(p);
}


int get_kernel_map(int kernel_base, int kernel_pmap_offset, uint32_t *virt, uint32_t *phys)
{
    int ret;
    char *dest;
    uint32_t *ptr;
    vm_address_t addr;
    *virt = 0;
    *phys = 0;
    mach_msg_type_number_t count;

    dest = allocate(4096);
    
    addr = (kernel_base + kernel_pmap_offset - 8);
    
    count = 12;
    ret = vm_read_overwrite
            (kernel_task,
             addr,
             4*3,
             dest,
             &count);

    if (ret == 0)
    {
        ptr = dest;
        if (ptr[0] != 0xa)
        {
            //failed, should match 0xa. and ptr[2] is the next thing
            printf("MISMATCH SIG for physmap ptr: %x %x %x\n", ptr[0], ptr[1], ptr[2]);
            ret = 3;
            goto end;
        }
        
        //ptr[2] is kernel_pmap. read those bytes now
        addr = ptr[2];
        count = 8;
        ret = vm_read_overwrite
                (kernel_task,
                 addr,
                 count,
                 dest,
                 &count);
        if (ret != 0)
        {
            goto end;
        }
        
        ptr = dest;
        *virt = ptr[0];
        *phys = ptr[1];
        ret = 0;
    }
    
    

end:    
    deallocate(dest, 4096);
    
    return ret;
}
int main(int argc, char *argv[])
{    
    int ret;
    uint32_t addr, virtbase, physbase;
    
    ret = task_for_pid(mach_task_self(), 0, &kernel_task);
    if (ret != KERN_SUCCESS) {
        printf("task for pid failed for pid=%d error=%d\n", 0, ret);
        return 0;
    }

    addr = get_kernel_region_tfp(kernel_task);
    printf("[+] Found kernel region @ %x\n", addr);
    
    ret = get_kernel_map(addr, 0x802d27e8 - 0x80000000, &virtbase, &physbase);
    if (ret != KERN_SUCCESS)
    {
        printf("Failed to get kernel map addresses\n");
        return 0;
    }
    
    SHIFTPHYS = virtbase - physbase;
    printf("Got virtbase = %x physbase = %x shift = %x\n", virtbase, physbase, SHIFTPHYS);

    armPtPrint(2, physbase, physbase);

    mach_port_deallocate(mach_task_self(), kernel_task);
    return 0;
}
