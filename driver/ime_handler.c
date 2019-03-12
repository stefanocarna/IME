#include <asm/apic.h>

#include "ime_handler.h"
#include "msr_config.h"
#include "ime_pebs.h"
#include "ime_fops.h"


static inline int handle_ime_event(struct pt_regs *regs)
{
	u64 msr[MAX_PMC];
	u64 global; 
	rdmsrl(MSR_IA32_PERF_GLOBAL_STATUS, global);
	if(global & BIT(62)){ 
		int i;
		for(i = 0; i < MAX_PMC; i++){
			rdmsrl(MSR_IA32_PERFEVTSEL(i), msr[i]);
			wrmsrl(MSR_IA32_PERFEVTSEL(i), 0ULL);
		}
		
		write_buffer();

		for(i = 0; i < MAX_PMC; i++){
			wrmsrl(MSR_IA32_PERFEVTSEL(i), msr[i]);
		}
		wrmsrl(MSR_IA32_PERF_GLOBAL_STATUS_RESET, BIT(62));
		return 1;
	}
	return 0;
}// handle_ibs_event


int handle_ime_nmi(unsigned int cmd, struct pt_regs *regs)
{
	return handle_ime_event(regs);
}// handle_ime_nmi
