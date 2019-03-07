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
	pr_info("Init\n");
	int err = 0;
	print_reg();
	//check_for_pebs_support();
	enable_nmi();
	setup_resources();
	init_pebs_struct();
	on_each_cpu(pebs_init, NULL, 1);
	//on_each_cpu(enablePMC0, NULL, 1);
	//enable_on_apic();
	return 0;
}// hop_init

void __exit hop_exit(void)
{
	pr_info("Exit\n");
	print_reg();
	on_each_cpu(pebs_exit, NULL, 1);
	exit_pebs_struct();
	cleanup_resources();
	disable_nmi();
	//cleanup_pmc();
	//on_each_cpu(disablePMC0, NULL, 1);
	//disable_on_apic();
	pr_info("Module Exit -- %lu\n", audit_counter);
}// hop_exit

// Register these functions
module_init(hop_init);
module_exit(hop_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Serena Ferracci");
