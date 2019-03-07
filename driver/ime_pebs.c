#include <asm/types.h>
#include <linux/percpu.h>	/* Macro per_cpu */
#include <linux/ioctl.h>
// #include <asm/uaccess.h>
#include <linux/hashtable.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sched.h>	/* task_struct */
#include <linux/cpu.h>
#include <asm/cmpxchg.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/threads.h>
#include <asm/smp.h>

#include "msr_config.h" 
#include "ime_pebs.h"
#include "irq_facility.h"

#define BUFFER_SIZE		(64 * 1024) /* must be multiple of 4k */

typedef struct{
	u64 eflags;	// 0x00
	u64 eip;	// 0x08
	u64 eax;	// 0x10
	u64 ebx;	// 0x18
	u64 ecx;	// 0x20
	u64 edx;	// 0x28
	u64 esi;	// 0x30
	u64 edi;	// 0x38
	u64 ebp;	// 0x40
	u64 esp;	// 0x48
	u64 r8;		// 0x50
	u64 r9;		// 0x58
	u64 r10;	// 0x60
	u64 r11;	// 0x68
	u64 r12;	// 0x70
	u64 r13;	// 0x78
	u64 r14;	// 0x80
	u64 r15;	// 0x88
	u64 stat;	// 0x90 IA32_PERF_GLOBAL_STATUS
	u64 add;	// 0x98 Data Linear Address
	u64 enc;	// 0xa0 Data Source Encoding
	u64 lat;	// 0xa8 Latency value (core cycles)
				// 0xb0
}pebs_arg_t;

typedef struct{
	u64 bts_buffer_base;					// 0x00 
	u64 bts_index;							// 0x08
	u64 bts_absolute_maximum;				// 0x10
	u64 bts_interrupt_threshold;			// 0x18
	pebs_arg_t *pebs_buffer_base;			// 0x20
	pebs_arg_t *pebs_index;					// 0x28
	pebs_arg_t *pebs_absolute_maximum;		// 0x30
	pebs_arg_t *pebs_interrupt_threshold;	// 0x38
	u64 pebs_counter0_reset;				// 0x40
	u64 pebs_counter1_reset;				// 0x48
	u64 pebs_counter2_reset;				// 0x50
	u64 pebs_counter3_reset;				// 0x58
	u64 reserved;							// 0x60
}debug_store_t;


static int pebs_record_size = sizeof(pebs_arg_t);

debug_store_t* percpu_ds;

static DEFINE_PER_CPU(unsigned long, percpu_old_ds);
static DEFINE_PER_CPU(void *, buffer_base);
static DEFINE_PER_CPU(void *, buffer_index);
static DEFINE_PER_CPU(void *, buffer_absolute_maximum);


static int allocate_buffer(void)
{
	pebs_arg_t *ppebs;
	int nRecords = 32;
	debug_store_t *ds = this_cpu_ptr(percpu_ds);	

	ppebs = (pebs_arg_t *)kzalloc(sizeof(pebs_arg_t)*nRecords, GFP_KERNEL);
	if (!ppebs) {
		pr_err("Cannot allocate PEBS buffer\n");
		return -1;
	}

	//__this_cpu_write(last_read, ppebs);

	ds->bts_buffer_base 			= 0;
	ds->bts_index					= 0;
	ds->bts_absolute_maximum		= 0;
	ds->bts_interrupt_threshold		= 0;
	ds->pebs_buffer_base			= ppebs;
	ds->pebs_index					= ppebs;
	ds->pebs_absolute_maximum		= ppebs + (nRecords-1) * sizeof(pebs_arg_t);
	ds->pebs_interrupt_threshold	= ppebs + (nRecords/2) * sizeof(pebs_arg_t);
	ds->pebs_counter0_reset			= ~(0xfffffULL);
	ds->reserved					= 0;

	pr_info("Buffer allocated\n");
	return 0;
}

//chiamato quando c'è un interrupt
void write_buffer(void){
	debug_store_t *ds;
	pebs_arg_t *pebs, *end;

	ds = this_cpu_ptr(percpu_ds);
	pebs = (pebs_arg_t *) ds->pebs_buffer_base;
	end = (pebs_arg_t *)ds->pebs_index;
	pr_info("CPU[%d] start: %llx -- end: %llx\n", smp_processor_id(), pebs, end);
	for (; pebs < end; pebs = (pebs_arg_t *)((char *)pebs + pebs_record_size)) {
		pr_info("CPU[%d] latency: %llu\n", smp_processor_id(), pebs->lat);
	}
}

//alloca un buffer più grande per salvare i pebs records e fare spazio nella ds area
static int allocate_out_buf(void)
{
	void *outbu_base = kmalloc(BUFFER_SIZE, GFP_KERNEL);
	if (!outbu_base) {
		pr_err("Cannot allocate out buffer\n");
		return -1;
	}
	__this_cpu_write(buffer_base, outbu_base);
	__this_cpu_write(buffer_index, outbu_base);
	__this_cpu_write(buffer_absolute_maximum, outbu_base + BUFFER_SIZE);
	return 0;
}

int init_pebs_struct(void){
	percpu_ds = alloc_percpu(debug_store_t);
	if(!percpu_ds) return -1;
	return 0;
}

void exit_pebs_struct(void){
	free_percpu(percpu_ds);
}

void pebs_init(void *arg)
{
	unsigned long old_ds;
	//init_pebs_struct();
	allocate_buffer();
	//allocate_out_buf(); 

	rdmsrl(MSR_IA32_DS_AREA, old_ds);
	__this_cpu_write(percpu_old_ds, old_ds);
	wrmsrl(MSR_IA32_DS_AREA, this_cpu_ptr(percpu_ds));

	//BIT(0) enable PMC0
	wrmsrl(MSR_IA32_PEBS_ENABLE, BIT(32) | BIT(0));
}

void pebs_exit(void *arg)
{
	wrmsrl(MSR_IA32_PEBS_ENABLE, 0ULL);
	wrmsrl(MSR_IA32_DS_AREA, __this_cpu_read(percpu_old_ds));
	//exit_pebs_struct();
	/*if (__this_cpu_read(buffer_base)) {
		kfree(__this_cpu_read(buffer_base));
		__this_cpu_write(buffer_base, 0);
	}*/
}