#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x3dbdfe51, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xc869666f, __VMLINUX_SYMBOL_STR(cdev_del) },
	{ 0xd235538, __VMLINUX_SYMBOL_STR(cdev_init) },
	{ 0xd6ee688f, __VMLINUX_SYMBOL_STR(vmalloc) },
	{ 0x4d1850e4, __VMLINUX_SYMBOL_STR(register_kretprobe) },
	{ 0x8069e8bd, __VMLINUX_SYMBOL_STR(device_destroy) },
	{ 0x949f7342, __VMLINUX_SYMBOL_STR(__alloc_percpu) },
	{ 0x7485e15e, __VMLINUX_SYMBOL_STR(unregister_chrdev_region) },
	{ 0x7a2af7b4, __VMLINUX_SYMBOL_STR(cpu_number) },
	{ 0xc9ec4e21, __VMLINUX_SYMBOL_STR(free_percpu) },
	{ 0x412ff19e, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x1530bb65, __VMLINUX_SYMBOL_STR(unregister_kretprobe) },
	{ 0xc0f3ee82, __VMLINUX_SYMBOL_STR(device_create) },
	{ 0xa5967b99, __VMLINUX_SYMBOL_STR(cdev_add) },
	{ 0xd9431824, __VMLINUX_SYMBOL_STR(pv_cpu_ops) },
	{        0, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0x1226bdb6, __VMLINUX_SYMBOL_STR(ex_handler_default) },
	{ 0x18b4c1b5, __VMLINUX_SYMBOL_STR(find_get_pid) },
	{ 0x35ef420c, __VMLINUX_SYMBOL_STR(__put_task_struct) },
	{ 0x281eeb7e, __VMLINUX_SYMBOL_STR(class_destroy) },
	{ 0x18ced3ab, __VMLINUX_SYMBOL_STR(get_pid_task) },
	{ 0x7ea74792, __VMLINUX_SYMBOL_STR(__class_create) },
	{ 0x29537c9e, __VMLINUX_SYMBOL_STR(alloc_chrdev_region) },
	{ 0x6c07d933, __VMLINUX_SYMBOL_STR(add_uevent_var) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "0B156D8AC666296162F9AF7");
