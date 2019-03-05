#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kprobes.h>

#include "control.h"
#include "irq_facility.h"
#include "ime_device.h"
#include "ime_pebs.h"
#include "msr_config.h"

unsigned long audit_counter = 0;
module_param(audit_counter, ulong, S_IRUSR | S_IRGRP | S_IROTH);

extern long unsigned int system_vectors[];

static __init int hop_init(void)
{
	int err = 0;
	u64 msr;
	pr_info("Module Init\n");
	rdmsrl(MSR_IA32_MISC_ENABLE, msr);
	//Performance Monitoring Available
	pr_info("MSR_IA32_MISC_ENABLE[7]: %llx\n", msr & BIT(7));
	//Processor Event Based Sampling Unavailable
	pr_info("MSR_IA32_MISC_ENABLE[12]: %llx\n", msr & BIT(12));
	//check_for_pebs_support();
	on_each_cpu(pebs_init, NULL, 0);
	setup_resources();
	enable_nmi();
	//on_each_cpu(enablePMC0, NULL, 1);
	//enable_on_apic();
	return 0;
}// hop_init

void __exit hop_exit(void)
{
	disable_nmi();
	cleanup_resources();
	on_each_cpu(pebs_exit, NULL, 0);
	//cleanup_pmc();
	//on_each_cpu(disablePMC0, NULL, 1);
	//disable_on_apic();
	pr_info("Module Exit -- %lu\n", audit_counter);
}// hop_exit

// Register these functions
module_init(hop_init);
module_exit(hop_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna'");
