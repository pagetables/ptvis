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

uint32_t get_kernel_region_tfp(task_t kernel_task)
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

int main(int argc, char *argv[])
{
    uint32_t addr;
    task_t task;
    pid_t pid = 0;
    int error;

    error = task_for_pid(mach_task_self(), pid, &task);
    if (error != KERN_SUCCESS) {
        printf("task for pid failed for pid=%d error=%d\n", pid, error);
    	return 0;
    }

    addr = get_kernel_region_tfp(task);
    
    if (addr == 0)
    {
        printf("could not find base, giving up\n");
        return 0;
    }

    printf("addr= %x\n", addr);

    /*
     do dcache flust first
    */
    printf("ret=%d\n", syscall(0));
    fflush(0);

    syscall(8, addr - 0x80000000, addr + 0x1000);
    getchar();

    /*
        
    */
    syscall(8, addr - 0x80000000, strtoul(argv[1],0,0));

}
