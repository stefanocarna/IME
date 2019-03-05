#include <asm/apicdef.h> 	// Intel apic constants
#include <asm/desc.h>
#include <linux/kallsyms.h>
#include <linux/smp.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>		/* current macro */
#include <linux/percpu.h>		/* this_cpu_ptr */
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <asm/apic.h>
#include <asm/nmi.h>

#include "msr_config.h"
#include "irq_facility.h"
#include "intel_pmc_events.h"
#include "ime_fops.h"
#include "ime_handler.h"

#define MSR_IBS_CONTROL					0xc001103a
#define 	IBS_LVT_OFFSET_VAL				(1ULL<<8)	
#define 	IBS_LVT_OFFSET					0xfULL 	

unsigned ime_vector = 0xffU;

static void debugPMU(u64 pmu)
{
	u64 msr;
	rdmsrl(pmu, msr);
	pr_info("PMU %llx: %llx\n", pmu, msr);
}// debugPMU


/**
 * On my machine system_irqs is a global symbol but it is not exported (EXPORT_SYMBOL).
 * This is a problem for the system whne trying to mount the module because it cannot find
 * the symbol and ends up aborting the operation. To bypass this flaw, we use the 
 * kallsyms_lookup_name utility function which does the grunt work in place of us.
 */
static long unsigned int *system_irqs;

static unsigned irq_vector = 0;

static DEFINE_PER_CPU(u64, lvtpc_bkp);
static gate_desc entry_bkp;

extern unsigned long audit_counter;

extern void pebs_entry(void);

static void setup_ime_lvt(void *err)
{
	apic_write(APIC_LVTPC, BIT(10));
	return;
} // setup_ime_lvt

int setup_ime_nmi(int (*handler) (unsigned int, struct pt_regs*))
{
	int err = 0;
	static struct nmiaction handler_na;

	on_each_cpu(setup_ime_lvt, &err, 1);
	if (err) goto out;

	// register_nmi_handler(NMI_LOCAL, handler, NMI_FLAG_FIRST, NMI_NAME);
	handler_na.handler = handler;
	handler_na.name = NMI_NAME;
	/* this is a Local CPU NMI, thus must be processed before others */
	handler_na.flags = NMI_FLAG_FIRST;
	err = __register_nmi_handler(NMI_LOCAL, &handler_na);

out:
	return err;
}// setup_ime_nmi

void cleanup_ime_nmi(void)
{
	unregister_nmi_handler(NMI_LOCAL, NMI_NAME);
}// cleanup_ime_nmi


int enable_nmi(void)
{
	pr_info("Enable pebs\n");
	setup_ime_nmi(handle_ime_nmi);
	return 0;
}// enable_pebs_on_systemz



void disable_nmi(void)
{
	cleanup_ime_nmi();
}// disable_pebs_on_system



void disableAllPMC(void* arg)
{
	int pmc_id; 
	preempt_disable();
	for(pmc_id = 0; pmc_id < MAX_ID_PMC; pmc_id++){
		wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
		wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
		wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
		pr_info("[CPU %u] disablePMC%d\n", smp_processor_id(), pmc_id);
	}
	preempt_enable();
}

void cleanup_pmc(void){
	pr_info("audit_counter: %lu\n", audit_counter);
	on_each_cpu(disableAllPMC, NULL, 1);
}

void disablePMC0(void* arg)
{
	preempt_disable();
	//ret = debugPMU(MSR_IA32_PMC(pmc_id));
	if(smp_processor_id() == 3){
		int pmc_id = 0;
		wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, 0ULL);
		wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), 0ULL);
		wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
		pr_info("[CPU %u] disablePMC%d\n", smp_processor_id(), pmc_id);
		preempt_enable();
	}
}

void enablePMC0(void* arg)
{
	if(smp_processor_id() == 3){
		int pmc_id = 0;
		u64 msr;
		preempt_disable();
		wrmsrl(MSR_IA32_PMC(pmc_id), 0ULL);
		wrmsrl(MSR_IA32_PERF_GLOBAL_CTRL, BIT(pmc_id));
		wrmsrl(MSR_IA32_PERFEVTSEL(pmc_id), BIT(22) | BIT(20) | BIT(16) | 0xC0);
		wrmsrl(MSR_IA32_PERF_GLOBAL_OVF_CTRL, 1ULL << 62);
		wrmsrl(MSR_IA32_PMC(pmc_id), ~(0xffULL));
		pr_info("[CPU %u] enabledPMC%d\n", smp_processor_id(), pmc_id);
		rdmsrl(MSR_IA32_PMC(pmc_id), msr);
		pr_info("PMU%llx: %llx\n", MSR_IA32_PMC(pmc_id) - 0xC1, msr);
		preempt_enable();
	}
}

