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
#define FALLOUT_PAGE_SIZE_BYTES 12
#define FALLOUT_REINFORCEMENT_WRITES 45 % 300
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
    int i;
    for (i = 0; i < 256; i++) {
        flush(mem + FALLOUT_PAGE_SIZE * i);
    }
}

static int fallout_write_access(struct seq_file *m, void *v) {
    unsigned int offset = 0, secret = 0, i = 0, r = 0x88121;
    char pr[4] = {0x88, 0x1, 0x21, 0x0};
    /*if (wbuffer != NULL) {
        free_page((unsigned long) wbuffer);
        wbuffer = NULL;
    }*/
    r = 0x88021;
    offset = 0x021;// r & (FALLOUT_PAGE_SIZE - 1);
    secret = (r >> FALLOUT_PAGE_SIZE_BYTES) % 256;
    seq_printf(m, "%s\n", pr);
    //wbuffer = (uint8_t*) __get_free_page(GFP_KERNEL);
    if (wbuffer == NULL) {
        wbuffer = kmalloc(FALLOUT_PAGE_SIZE * 300, GFP_KERNEL);
        wbuffer = (uint8_t *)(((uint64_t) wbuffer + FALLOUT_PAGE_SIZE) & 0xFFFFFFFFFFFFF000ull);
        unrelated_page = kmalloc(FALLOUT_PAGE_SIZE * 300, GFP_KERNEL);
        mem = kmalloc(FALLOUT_PAGE_SIZE * 300, GFP_KERNEL);
        mem = (uint8_t *)(((uint64_t) mem + FALLOUT_PAGE_SIZE) & 0xFFFFFFFFFFFFF000ull);
    }
    flush_mem(mem);
    for (i = 0; i < FALLOUT_REINFORCEMENT_WRITES; i++) {
        unrelated_page[(i << 12) | i] = i;
    }
    wbuffer[offset] = secret;
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
        kfree(wbuffer);
        kfree(unrelated_page);
        kfree(mem);
        unrelated_page = wbuffer = NULL;
    }
    printk(KERN_INFO
    "[FALLOUT] Fallout Kernel Module unloaded. Goodbye!\n");
}

module_init(fallout_init);
module_exit(fallout_exit);

