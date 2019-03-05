#include <asm/apic.h>
#include <asm/apicdef.h>
#include <asm/nmi.h> 		// nmi stuff
#include <asm/cmpxchg.h>	// cmpxchg function
#include <linux/sched.h>	// current macro
#include <linux/percpu.h>	// this_cpu_ptr
#include <linux/slab.h>
#include <asm/fpu/internal.h>
#include <linux/sched/task_stack.h>

#include "ime_handler.h"
#include "msr_config.h"
#include "ime_fops.h"
#include "ime_pebs.h"

extern volatile unsigned long audit_counter;
spinlock_t counter_lock;

static inline int handle_ime_event(struct pt_regs *regs)
{
	unsigned long flags;
	int pmc, i;
	u64 global_stat;
	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, global_stat);
	pmc = global_stat & 0xfULL;

	if(!(global_stat & (0xfULL))) return 0;

	for(i = 0; i < MAX_PMC; i++){
		if((pmc >> i)%2){
			pmc_sample_t sample;
			int pmc_id = i;
			sample.pmc_id = i;
			memcpy(&(sample.regs), task_pt_regs(current), sizeof(struct pt_regs));
			memset((char *)&(sample.fpu_reg), 0, sizeof(struct fpu));
			copy_fxregs_to_kernel(&(sample.fpu_reg));
			//wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
			wrmsrl(MSR_IA32_PMC(pmc_id), ~(0xffULL));
			pr_info("write_buffer");
			write_buffer();
		}
	}

	spin_lock_irqsave(&(counter_lock), flags);
	audit_counter++;
	spin_unlock_irqrestore(&(counter_lock), flags);

	apic_write(APIC_LVTPC, BIT(10));
	//reset global registry
	wrmsrl(MSR_IA32_PERF_GLOBAL_OVF_CTRL, 0ULL | BIT(62));
	//wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
	//return the number of interrupt 
	return pmc;

}// handle_ibs_event


int handle_ime_nmi(unsigned int cmd, struct pt_regs *regs)
{
	return handle_ime_event(regs);
}// handle_ime_nmi