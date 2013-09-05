#include <kern/kern_types.h>
#include <mach/mach_types.h>
#include <sys/cdefs.h>
#include <mach/boolean.h>
#include <mach/port.h>
#include <mach/time_value.h>
#include <mach/message.h>
#include <mach/mach_param.h>
#include <mach/task_info.h>
#include <mach/exception_types.h>
#include <mach/vm_statistics.h>
#include <kern/queue.h>
#include <security/_label.h>
#include <libkern/libkern.h>

typedef struct _lck_mtx_ {
	union {
		struct {
			volatile uintptr_t		lck_mtxd_owner;
			union {
				struct {
					volatile uint32_t
						lck_mtxd_waiters:16,
						lck_mtxd_pri:8,
						lck_mtxd_ilocked:1,
						lck_mtxd_mlocked:1,
						lck_mtxd_promoted:1,
						lck_mtxd_spin:1,
						lck_mtxd_is_ext:1,
						lck_mtxd_pad3:3;
				};
					uint32_t	lck_mtxd_state;
			};
#if	defined(__x86_64__)
			/* Pad field used as a canary, initialized to ~0 */
			uint32_t			lck_mtxd_pad32;
#endif			
		} lck_mtxd;
		struct {
			struct _lck_mtx_ext_		*lck_mtxi_ptr;
			uint32_t			lck_mtxi_tag;
#if	defined(__x86_64__)				
			uint32_t			lck_mtxi_pad32;
#endif			
		} lck_mtxi;
	} lck_mtx_sw;
} lck_mtx_t;

#define decl_lck_mtx_data(class,name)     class lck_mtx_t name;

extern "C" void				lck_mtx_lock(
									lck_mtx_t		*lck);

extern "C" void				lck_mtx_unlock(
									lck_mtx_t		*lck);

struct task {
	/* Synchronization/destruction information */
	decl_lck_mtx_data(,lock)		/* Task's lock */
	uint32_t	ref_count;	/* Number of references to me */
	boolean_t	active;		/* Task has not been terminated */
	boolean_t	halting;	/* Task is being halted */

	/* Miscellaneous */
	vm_map_t	map;		/* Address space description */
	queue_chain_t	tasks;	/* global list of tasks */
	void		*user_data;	/* Arbitrary data settable via IPC */
};

//
// 10.8.2 offsets vs 10.8.3 offsets
//

vm_offset_t
get_slide()
{
    return ((char *)&lck_mtx_lock) - ((char *)(0xffffff80002ace00));
}

extern
"C"
void*
get_bsdtask_info (
     task_t task
);

extern
"C"
int
proc_pid(void*);

void
walk_tasks(uint64_t *output)
{
    task_t	task;
    lck_mtx_t *tasks_threads_lockPTR;
    queue_head_t*			tasksPTR;
    uint64_t *outcount = output;
    int count = 0;
    
    tasksPTR = (queue_head_t*)(((char *)0xffffff80008b9b78) + get_slide());
    tasks_threads_lockPTR = (lck_mtx_t*)(((char *)0xffffff80008b9bd0) + get_slide());        
    
    lck_mtx_lock(tasks_threads_lockPTR);
    
    output++;

    for (task = (task_t)queue_first(tasksPTR);
         !queue_end(tasksPTR, (queue_entry_t)task);
         task = (task_t)queue_next(&task->tasks))
    {
        if(task == NULL) break;
        
        void *vm_map = *(((void**)task)+4);
        void *pmap = *(((void**)vm_map)+9);
        uint64_t cr3 = ((uint64_t*)pmap)[1];
        
        *output = cr3;
        output++;
       // *output = proc_pid(get_bsdtask_info(task));
        output++;
        count++;
        count++;
    }
    lck_mtx_unlock(tasks_threads_lockPTR);
    
    *outcount = count;
}


/*

from task what is offset to vm_map?  -- 32 bytes -- 
from vm_map what is offset to pmap?  -- 72 bytes --
from pmap what is offset to cr3?     -- 8 bytes --
from pmap what is offset to pml4?    -- 56 bytes -- 

task                vm_map              ipc_space          #acts   pid  process             io_policy    wq_state   command

0xffffff8030873c40  0xffffff802ea4fbc8  0xffffff80307ede70     1   206  0xffffff802f786bc0                          bash


task->map = 0xffffff802ea4fbc8, 
/gx 0xffffff8030873c40+32
0xffffff8030873c60:	0xffffff802ea4fbc8


  pmap = 0xffffff8030874f00, 
(gdb) x/gx 0xffffff802ea4fbc8+72
0xffffff802ea4fc10:	0xffffff8030874f00


  pm_cr3 = 957120512, 
  pm_pml4 = 0xffffff8030875000, 

*/
