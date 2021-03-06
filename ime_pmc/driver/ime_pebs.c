#include <linux/percpu.h>	/* Macro per_cpu */
#include <linux/slab.h>
#include <asm/smp.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/hashtable.h>

#include "msr_config.h" 
#include "ime_pebs.h"
#include "../main/ime-ioctl.h"

#define PEBS_STRUCT_SIZE	sizeof(pebs_arg_t)
#define MAX_BUFFER_PEBS		(int)65536/PEBS_STRUCT_SIZE;
#define MAX_POOL_BUFFER 	60
#define MAX_POOL_ALLOC		20



int nRecords_pebs = MAX_BUFFER_PEBS;
int nBuffer_array = MAX_POOL_BUFFER;
int write_index = 0;
int read_index = 0;
unsigned long write_cycle = 0;
unsigned long read_cycle = 0;
debug_store_t* percpu_ds;
spinlock_t lock_buffer;
static DEFINE_PER_CPU(task_array_t*, percpu_task);
static DEFINE_PER_CPU(pebs_array_t*, percpu_buffer);
static DEFINE_PER_CPU(unsigned long, percpu_old_ds);
static DEFINE_PER_CPU(unsigned long, percpu_index);
extern u64 reset_value_pmc[MAX_ID_PMC];
u64 collected_samples;
DECLARE_HASHTABLE(hash_samples, 10);

void allocate_buffer(void* arg)
{
	int index_array;
	pebs_arg_t *ppebs;
	task_array_t* task;
	int i;
	debug_store_t *ds = this_cpu_ptr(percpu_ds);
	pebs_array_t* buffer_bh = (pebs_array_t*)kzalloc(sizeof(pebs_array_t)*nBuffer_array, GFP_KERNEL);
	if (!buffer_bh) {
		pr_err("Cannot allocate buffer POOL buffer\n");
		return;
	}
	task = (task_array_t*)kzalloc(sizeof(task_array_t)*nBuffer_array, GFP_KERNEL);
	if (!task) {
		pr_err("Cannot allocate buffer POOL buffer\n");
		return;
	}
	__this_cpu_write(percpu_index, 0);
	index_array = __this_cpu_read(percpu_index);

	for(i = 0; i < MAX_POOL_ALLOC; i++){
		buffer_bh[i].buffer = (pebs_arg_t*)kzalloc(PEBS_STRUCT_SIZE*nRecords_pebs, GFP_KERNEL);
		if (!buffer_bh[i].buffer) {
			pr_err("Cannot allocate buffer %d buffer\n", i);
			return;
		}
		buffer_bh[i].allocated = 1;
		task[i].task = (struct tasklet_struct*)kzalloc(sizeof(struct tasklet_struct), GFP_KERNEL);
		if (!task[i].task) {
			pr_err("Cannot allocate buffer %d buffer\n", i);
			return;
		}
		task[i].allocated = 1;

	}
	//pr_info("[CPU%d]ds: %llx -- buffer: %llx\n", smp_processor_id(), ds, buffer_bh);
	ppebs = buffer_bh[index_array].buffer;
	buffer_bh[index_array].usage = 1;

	ds->bts_buffer_base 			= 0;
	ds->bts_index					= 0;
	ds->bts_absolute_maximum		= 0;
	ds->bts_interrupt_threshold		= 0;
	ds->pebs_buffer_base			= ppebs;
	ds->pebs_index					= ppebs;
	ds->pebs_absolute_maximum		= ppebs + (nRecords_pebs-1);
	ds->pebs_interrupt_threshold	= ppebs + (nRecords_pebs-1);
	ds->reserved					= 0;

	__this_cpu_write(percpu_buffer, buffer_bh);
	__this_cpu_write(percpu_task, task);
}

void set_reset_value(void){
	debug_store_t *ds = this_cpu_ptr(percpu_ds);
	ds->pebs_counter0_reset			= ~reset_value_pmc[0];
	ds->pebs_counter1_reset			= ~reset_value_pmc[1];
	ds->pebs_counter2_reset			= ~reset_value_pmc[2];
	ds->pebs_counter3_reset			= ~reset_value_pmc[3];
}

void schedule_task(int index_array, unsigned long start, unsigned long end){
	int i;
	struct tasklet_struct *t;
	bh_data_t *data;
	task_array_t* task;
	atomic_t count = ATOMIC_INIT(0);
	task = __this_cpu_read(percpu_task);
	for(i = 0; i < nBuffer_array; i++){
		if(!task[i].usage){
			if(!task[i].allocated){
				t = (struct tasklet_struct*)kzalloc(sizeof(struct tasklet_struct), GFP_KERNEL);
				if (!t) {
					pr_err("Cannot allocate buffer %d buffer\n", i);
					return;
				}
				task[i].task = t;
				task[i].allocated = 1;
			}
			else{
				t = task[i].task;
			}
			data = (bh_data_t*) kzalloc (sizeof(bh_data_t), GFP_ATOMIC);
			if (!data) {
				pr_err("Cannot allocate DATA buffer\n");
				return;
			}
			data->start = start;
			data->end = end;
			data->index = index_array;
			data->index_task = i;

			task[i].usage = 1;
			t->next = NULL;
			t->state = 0;
			t->count = count;
			t->func = tasklet_handler;
			t->data = (unsigned long) data;
			tasklet_schedule(t);
			break;
		}
	}
}

void write_buffer(void){
	int i, index_array;
	debug_store_t *ds;
	pebs_array_t* buffer_bh;
	task_array_t* task;
	pebs_arg_t *ppebs;
	bh_data_t *data;
	struct tasklet_struct *t;
	atomic_t count = ATOMIC_INIT(0);
	ds = this_cpu_ptr(percpu_ds);
	buffer_bh = __this_cpu_read(percpu_buffer);
	task = __this_cpu_read(percpu_task);
	index_array = __this_cpu_read(percpu_index);

	for(i = 0; i < nBuffer_array; i++){
		if(!buffer_bh[i].usage){
			if(!buffer_bh[i].allocated){
				ppebs = (pebs_arg_t *) kzalloc (PEBS_STRUCT_SIZE*nRecords_pebs, GFP_KERNEL);
				if (!ppebs) {
					pr_err("Cannot allocate PEBS buffer\n");
					return -1;
				}
				buffer_bh[i].buffer = ppebs;
				buffer_bh[i].allocated = 1;
			}
			else{
				ppebs = buffer_bh[i].buffer;
			}
			schedule_task(index_array, (unsigned long) ds->pebs_buffer_base, (unsigned long) ds->pebs_index);
			break;
		}
	}

	if(i == nBuffer_array){
		pr_info("Override\n");
		i = index_array;
		ppebs = buffer_bh[i].buffer;
	}
	else index_array = i;

	if (!ppebs) {
		pr_err("Cannot allocate PEBS buffer\n");
		return -1;
	}
	buffer_bh[index_array].usage = 1;
	__this_cpu_write(percpu_index, index_array);
	//pr_info("[CPU%d]Switch to index: %d\n",smp_processor_id(), i);
	ds->pebs_buffer_base			= ppebs;
	ds->pebs_index					= ppebs;
	ds->pebs_absolute_maximum		= ppebs + (nRecords_pebs-1);
	ds->pebs_interrupt_threshold	= ppebs + (nRecords_pebs-1);
	//ds->pebs_index = ds->pebs_buffer_base;
	++collected_samples;
}

int init_pebs_struct(void){
	int i;
	percpu_ds = alloc_percpu(debug_store_t);
	if(!percpu_ds) return -1;
	on_each_cpu(allocate_buffer, NULL, 1);
	hash_init(hash_samples);
	return 0;
}

void release_buffer(void* arg){
	int i;
	task_array_t* task;
	pebs_array_t* buffer_bh = __this_cpu_read(percpu_buffer);
	for(i = 0; i < nBuffer_array; i++){
		if(buffer_bh[i].allocated) {
			kfree(buffer_bh[i].buffer);
		}
	}
	kfree(buffer_bh);
	task = __this_cpu_read(percpu_task);
	for(i = 0; i < nBuffer_array; i++){
		if(task[i].allocated) {
			kfree(task[i].task);
		}
	}
	kfree(task);
}

void exit_pebs_struct(void){
	on_each_cpu(release_buffer, NULL, 1);
	free_percpu(percpu_ds);
}

void pebs_init(void *arg)
{
	u64 pebs;
	int pmc_id;
	unsigned long old_ds;
	struct sampling_spec* args = (struct sampling_spec*) arg;
	if(args->enable_PEBS[smp_processor_id()] == 0) return;
	pmc_id = args->pmc_id;
	if(args->buffer_pebs_length > 0) nRecords_pebs = args->buffer_pebs_length;
	if(args->buffer_module_length >= MAX_POOL_ALLOC && args->buffer_module_length <= MAX_POOL_BUFFER){ 
		nBuffer_array = args->buffer_module_length;
	}
	collected_samples = 0;

	set_reset_value();

	rdmsrl(MSR_IA32_DS_AREA, old_ds);
	__this_cpu_write(percpu_old_ds, old_ds);
	wrmsrl(MSR_IA32_DS_AREA, this_cpu_ptr(percpu_ds));

	rdmsrl(MSR_IA32_PEBS_ENABLE, pebs);
	wrmsrl(MSR_IA32_PEBS_ENABLE, pebs | BIT(pmc_id));
}

void pebs_exit(void *arg)
{
	u64 pebs;
	int pmc_id, i;
	struct sampling_spec* args = (struct sampling_spec*) arg;
	if(args->enable_PEBS[smp_processor_id()] == 0) return;
	pmc_id = args->pmc_id;
	//scrivi i samples che non hanno causato un overflow del buffer di PEBs nel buffer del modulo
	rdmsrl(MSR_IA32_PEBS_ENABLE, pebs);
	wrmsrl(MSR_IA32_PEBS_ENABLE, pebs & ~(BIT(pmc_id)));
	wrmsrl(MSR_IA32_DS_AREA, __this_cpu_read(percpu_old_ds));
}

void tasklet_handler(unsigned long data){
	pebs_arg_t * pebs, *end;
	pebs_array_t* buffer_bh;
	unsigned long flags;
	bh_data_t *bh_data = (bh_data_t*)data;
	sample_arg_t *current_sample, *new_sample;
	//pr_info("[TASK] Start%d\n", bh_data->index);
	int written_samples = 0;
	task_array_t* task = __this_cpu_read(percpu_task);
	buffer_bh = __this_cpu_read(percpu_buffer);
	pebs = (pebs_arg_t *)bh_data->start;
	end = (pebs_arg_t *)bh_data->end;
	spin_lock_irqsave(&(lock_buffer), flags);
	for (; pebs < end; pebs = (pebs_arg_t *)((char *)pebs + PEBS_STRUCT_SIZE)) {
		int found = 0;
		hash_for_each_possible(hash_samples, current_sample, sample_node, (pebs->add >> 12)){
			if(current_sample->address == (pebs->add >> 12)) {
				++found;
				++current_sample->times;
			}
		}
		if(found == 0 && ((pebs->add >> 32) == 0x4000)){
			new_sample = (sample_arg_t*) kzalloc (sizeof(sample_arg_t), GFP_KERNEL);
			new_sample->address = pebs->add >> 12;
			new_sample->times = 1;
			hash_add(hash_samples, &new_sample->sample_node, new_sample->address);
		}
		++written_samples;
	}
	spin_unlock_irqrestore(&(lock_buffer), flags);
	buffer_bh[bh_data->index].usage = 0;
	task[bh_data->index_task].usage = 0;
	kfree(bh_data);
	//pr_info("[TASK] End%d\n", bh_data->index);
}

void read_buffer_samples(struct buffer_struct* args){
	int c;
	u64 l = 0;
	sample_arg_t *cursor;
	hash_for_each(hash_samples, c, cursor, sample_node){
		//pr_info("Address: %llx -- times: %llx\n", cursor->address, cursor->times);
		args[l].address = cursor->address;
		args[l].times = cursor->times;
		++l;
	}
	//pr_info("samples: %llx\n", l);
}

u64 retrieve_buffer_size(void){
	int c;
	u64 n_samples = 0;
	sample_arg_t *cursor;
	hash_for_each(hash_samples, c, cursor, sample_node){
		//pr_info("Address: %llx -- times: %llx\n", cursor->address, cursor->times);
		++n_samples;
	}
	return n_samples;
}

void reset_hashtable(void){
	int c;
	struct hlist_node *next;
	sample_arg_t *cursor;
	hash_for_each_safe(hash_samples, c, next, cursor, sample_node){
		//pr_info("remove node\n");
		hash_del(&(cursor->sample_node));
		kfree(cursor);
	}
}

void empty_pebs_buffer(void){
	int index_array;
	debug_store_t *ds;
	bh_data_t *data;
	ds = this_cpu_ptr(percpu_ds);
	index_array = __this_cpu_read(percpu_index);

	data = (bh_data_t*) kzalloc (sizeof(bh_data_t), GFP_ATOMIC);
	if (!data) {
		pr_err("Cannot allocate DATA buffer\n");
		return;
	}
	data->start = (unsigned long) ds->pebs_buffer_base;
	data->end = (unsigned long) ds->pebs_index;
	data->index = index_array;
	tasklet_handler(data);

	ds->pebs_index = ds->pebs_buffer_base;
}