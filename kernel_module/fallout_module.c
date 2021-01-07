#include <asm/io.h>
#include <linux/stddef.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/mm.h>

#define FALLOUT_PAGE_SIZE 4096
#define FALLOUT_PAGE_SIZE_BITS 12
#define FALLOUT_REINFORCEMENT_WRITES 40
#define FALLOUT_PROC_FILE_NAME "fallout_interface"

MODULE_LICENSE("GPL");

uint8_t *wbuffer = NULL, *unrelated_page, *mem;

static inline void flush(void *p) {
#ifdef __x86_64__
    asm volatile("clflush (%0)\n" : : "r"(p) : "rax");
#else
    asm volatile("clflush (%0)\n" : : "r"(p) : "eax");
#endif
}

static inline void flush_mem(void *mem) {
    int i = 0;
    for (; i < 256; i++) {
        flush(mem + (i<<FALLOUT_PAGE_SIZE_BITS));
    }
}


/**
 * Perform a write access to kernel memory at a specified page offset
 * @return Always 0
 */
static int fallout_write_access(struct seq_file *m, void *v) {
    int i;
    if (wbuffer == NULL) {
        wbuffer = alloc_pages_exact(FALLOUT_PAGE_SIZE * 300, GFP_KERNEL);
        unrelated_page = kmalloc(FALLOUT_PAGE_SIZE * 300, GFP_KERNEL);
        mem = alloc_pages_exact(FALLOUT_PAGE_SIZE * 300, GFP_KERNEL);
    }
    // Check if allocated memory is page-aligned
    seq_printf(m, "%c\n", (char)(((uint64_t)wbuffer & 0xFF) | ((uint64_t)mem & 0xFF) | (((uint64_t)wbuffer & 0xF00) >> 8) | (((uint64_t)mem & 0xF00) >> 8)));
    flush_mem(mem);
    for (i = 0; i < FALLOUT_REINFORCEMENT_WRITES; i++) {
        unrelated_page[(i << FALLOUT_PAGE_SIZE_BITS) | i] = i;
    }
    wbuffer[0xABC] = 42;
    return 0;
}

static int open_file(struct inode *inode, struct file *file) {
    return single_open(file, fallout_write_access, NULL);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
static struct file_operations file_ops = {
        .owner = THIS_MODULE,
        .open = open_file,
        .release = single_release,
        .read = seq_read,
        .llseek = seq_lseek,
};
#else
static struct proc_ops file_ops = {
        .proc_open = open_file,
        .proc_release = single_release,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
};
#endif

static int __init

fallout_init(void) {
    struct proc_dir_entry *entry;
    printk(KERN_INFO
    "[FALLOUT] Hello World!\n");
    entry = proc_create(FALLOUT_PROC_FILE_NAME, 777, NULL, &file_ops);
    printk(KERN_INFO
    "[FALLOUT] Fallout Kernel Module loaded!\n");
    return 0;
}

static void __exit

fallout_exit(void) {
    remove_proc_entry(FALLOUT_PROC_FILE_NAME, NULL);
    if (wbuffer != NULL) {
        free_pages_exact(wbuffer, FALLOUT_PAGE_SIZE * 300);
        kfree(unrelated_page);
        free_pages_exact(mem, FALLOUT_PAGE_SIZE * 300);
        unrelated_page = wbuffer = mem = NULL;
    }
    printk(KERN_INFO
    "[FALLOUT] Fallout Kernel Module unloaded. Goodbye!\n");
}

module_init(fallout_init);
module_exit(fallout_exit);

