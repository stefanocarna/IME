#include <asm/apic.h>
#include <asm/apicdef.h>
#include <asm/nmi.h> 		// nmi stuff
#include <asm/cmpxchg.h>	// cmpxchg function
#include <linux/sched.h>	// current macro
#include <linux/percpu.h>	// this_cpu_ptr
#include <linux/slab.h>
#include <asm/fpu/internal.h>

#include "ime_handler.h"
#include "msr_config.h"
#include "ime_fops.h"
#include "ime_pebs.h"
#include "irq_facility.h"

extern volatile unsigned long audit_counter;
static DEFINE_PER_CPU(long, cpu_k);

static inline int handle_ime_event(struct pt_regs *regs)
{
	u64 msr;
	long current_nmi;
	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, msr);
	if(!(msr & (0xfULL))){
		pr_info("[CPU%d] irq NOT ok", smp_processor_id());
	}
	print_reg();

	current_nmi = __this_cpu_read(cpu_k) + 1;
	__this_cpu_write(cpu_k, current_nmi);
	pr_info("[CPU%d] current nmi counter: %d\n", smp_processor_id(), current_nmi);
	/*if(current_nmi > 2){
		wrmsrl(MSR_IA32_PEBS_ENABLE, 0ULL);
		wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
		wrmsrl(MSR_IA32_PERFEVTSEL(0), 0ULL);
		wrmsrl(MSR_IA32_PMC(0), 0ULL);
	}*/

	write_buffer();

	wrmsrl(MSR_IA32_PERFEVTSEL(0), 0ULL);
	wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, BIT(0));

	return 1;


	/*int pmc, i;
	u64 global_stat;
	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, global_stat);
	pmc = global_stat & 0xfULL;

	if(!(global_stat & (0xfULL))) return k;

	for(i = 0; i < MAX_PMC; i++){
		if((pmc >> i)%2){
			k++;
			int pmc_id = i;
			wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
			wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, BIT(pmc_id));
			//wrmsrl(MSR_IA32_PMC(pmc_id), ~(0xffULL));
			//write_buffer();
		}
	}
	//apic_write(APIC_LVTPC, BIT(10));
	//reset global registry
	//wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
	//return the number of interrupt 
	return k;*/

}// handle_ibs_event


int handle_ime_nmi(unsigned int cmd, struct pt_regs *regs)
{
	return handle_ime_event(regs);
}// handle_ime_nmi
