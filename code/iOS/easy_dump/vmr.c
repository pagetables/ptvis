#include <sys/types.h>
#include <sys/sysctl.h>
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
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct vmtotal vmtotal_t;

int dump_kernel_task(int argc, char *argv[])
{
    task_t task;
    kern_return_t error;
    pid_t pid = 0;
    int fd;
    
    error = task_for_pid(mach_task_self(), pid, &task);
    if (error != KERN_SUCCESS) {
        printf("task for pid failed for pid=%d error=%d\n", pid, error);
    	return 0;
    }

    vm_address_t address=0;
    char *buf = malloc(4096);
    vm_address_t start = strtoul(argv[1],0,0);
    vm_address_t end = strtoul(argv[2], 0, 0);
    int ret;

    printf("Reading from %x to %x\n", start, end);
    fd = -1;
    for (address = start; address < end; address += 0x1000)
    {
        memset(buf, 0, 4096);
        mach_msg_type_number_t count = 0;
        
        //
        // Region is mapped 0,0. but there's a bug where
        // reading < 4096 will work :-) 
        //
        ret = vm_read_overwrite
                (task,
                 address,
                 PAGE_SIZE/2,
                 buf,
                 &count);
        if (ret == 0)
        {
            ret = vm_read_overwrite
                    (task,
                     address + PAGE_SIZE/2,
                     PAGE_SIZE/2,
                     buf + PAGE_SIZE/2,
                     &count);
        }
             
        if (ret == 0)
        {
            if (fd == -1)
            {
                char namebuf[40];
                sprintf(namebuf, "kmem/%.8x.mem", address);
                fd = open(namebuf, O_CREAT|O_WRONLY);
                if (fd == -1){
                    printf("unable to open file %s for writing\n", namebuf);
                    break;
                }
            }
            write(fd, buf, 4096);
        } else
        {
            printf("Failed to read: ret=%d address= %x", ret, address);
            break;
        }
    }

    close(fd);
   
    printf("Finished\n");
    mach_port_deallocate(mach_task_self(), task);
    return 0;
}

int main(int argc, char *argv[])
{
    mkdir("kmem",0755);
    dump_kernel_task(argc, argv);
}