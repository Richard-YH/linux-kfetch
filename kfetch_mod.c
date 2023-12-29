/* 
 *  kfetch_mod.c: Creates a char device that would fetch OS information
 *  you have read from the dev file 
 */ 

#include <linux/atomic.h> 
#include <linux/cdev.h> 
#include <linux/delay.h> 
#include <linux/device.h>  
#include <linux/fs.h> 
#include <linux/init.h> 
#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/printk.h> 
#include <linux/types.h> 
#include <linux/uaccess.h>
#include <linux/version.h> 
#include <linux/utsname.h>
#include <linux/sched.h>
#include <asm/errno.h> 
#include "kfetch.h"


#define SUCCESS 0 
#define DEVICE_NAME "kfetch" /* Dev name as it appears in /proc/devices   */ 
#define BUF_LEN 1024 /* Max length of the message from the device */ 
#define ROWS 7 /* for graph rows*/
#define COLS 20 /* for graph cols*/


enum { 
    CDEV_NOT_USED = 0, 
    CDEV_EXCLUSIVE_OPEN = 1, 
}; 
 
/* Function prototypes */
static int device_open(struct inode *, struct file *); 
static int device_release(struct inode *, struct file *); 
static ssize_t device_read(struct file *, char __user *, size_t len, loff_t *); 
static ssize_t device_write(struct file *, const char __user *, size_t len, loff_t *); 
 

/* Global variables */
static int major; /* major number assigned to our device driver */ 
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED); /* Is device open? Used to prevent multiple access to device */ 
static char msg[BUF_LEN + 1]; /* The msg the device will give when asked */ 
static struct class *cls; 
 
/* File operations structure */
static struct file_operations chardev_fops = { 
    .owner = THIS_MODULE,
    .read = device_read, 
    .write = device_write, 
    .open = device_open, 
    .release = device_release, 
}; 

static int __init chardev_init(void) { 
    major = register_chrdev(0, DEVICE_NAME, &chardev_fops); 
    if (major < 0) { 
        pr_alert("Registering char device failed with %d\n", major); 
        return major; 
    } 
    pr_info("I was assigned major number %d.\n", major); 
 
    // Create a class; use different methods based on kernel version
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0) 
        cls = class_create(DEVICE_NAME); 
    #else 
        cls = class_create(THIS_MODULE, DEVICE_NAME); 
    #endif 

    if (IS_ERR(cls)) {
        unregister_chrdev(major, DEVICE_NAME);
        pr_alert("Failed to create class.\n");
        return PTR_ERR(cls);
    }

    if (IS_ERR(device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME))) {
        class_destroy(cls);
        unregister_chrdev(major, DEVICE_NAME);
        pr_alert("Failed to create device.\n");
        return -1; // Or an appropriate error number
    }
 
    pr_info("Device created on /dev/%s\n", DEVICE_NAME); 
    return SUCCESS; 
} 
 
static void __exit chardev_exit(void)  { 
    /* Destroy the device - only if it was created */
    if (cls) {
        device_destroy(cls, MKDEV(major, 0)); 
    }

    /* Destroy the class - only if it was created */
    if (cls) {
        class_destroy(cls); 
    }

    /* Unregister the device - only if it was registered */
    if (major >= 0) {
        unregister_chrdev(major, DEVICE_NAME); 
    }

    pr_info("Char device unregistered and resources freed.\n");
} 
 
void get_hostname(char *msg) {
    struct new_utsname *uts; 
    uts = utsname(); 

    sprintf(msg, "                   \033[33m%s\033[0m\n", uts->nodename);
    // Printing the seperate line based on the length
    sprintf(msg + strlen(msg), "                   ");
    for (size_t i = 0; i < strlen(uts->nodename); i++) {
        sprintf(msg + strlen(msg), "-");
    }
    strcat(msg + strlen(msg), "\n");
}

void get_kernel_info(char *msg) {
    struct new_utsname *uts; 
    uts = utsname(); 
    sprintf(msg + strlen(msg), "\033[33mKernel:\033[0m\t%s\n", uts->release);
}

void get_CPU_info(char *msg) {
    struct file *cpuinfo_file;
    char cpuinfo_buffer[512]; 
    char *model_name;

    cpuinfo_file = filp_open("/proc/cpuinfo", O_RDONLY, 0);
    if (IS_ERR(cpuinfo_file)) {
        pr_err("Failed to open /proc/cpuinfo file\n");
        return;
    }

    memset(cpuinfo_buffer, 0, sizeof(cpuinfo_buffer));
    kernel_read(cpuinfo_file, cpuinfo_buffer, sizeof(cpuinfo_buffer), &cpuinfo_file->f_pos);
    filp_close(cpuinfo_file, NULL);

    model_name = strstr(cpuinfo_buffer, "model name");
    if (model_name) {
        model_name = strchr(model_name, ':');
        sprintf(msg + strlen(msg), "\033[33mCPU:\033[0m\t\t");

        if (model_name) {
            model_name++;
            while (*model_name && (*model_name == ' ' || *model_name == '\t')) {
                model_name++;
            }

            while (*model_name && *model_name != '\n') {
                sprintf(msg + strlen(msg), "%c", *model_name);
                model_name++;
            }
            sprintf(msg + strlen(msg), "\n");
        }
    }
}

void get_CPU_counts(char *msg) {
    struct file *cpuinfo_file;
    int online_cpus = 0;
    int total_cpus = 0;
    char cpuinfo_buffer[512]; 
    char *cpu_block;
    char *core_id;

    cpuinfo_file = filp_open("/proc/cpuinfo", O_RDONLY, 0);
    if (IS_ERR(cpuinfo_file)) {
        pr_err("Failed to open /proc/cpuinfo file\n");
        return;
    }

    memset(cpuinfo_buffer, 0, sizeof(cpuinfo_buffer));
    kernel_read(cpuinfo_file, cpuinfo_buffer, sizeof(cpuinfo_buffer), &cpuinfo_file->f_pos);
    filp_close(cpuinfo_file, NULL);

    cpu_block = cpuinfo_buffer;
    while ((cpu_block = strstr(cpu_block, "processor"))) {
        total_cpus++; 
        cpu_block = strchr(cpu_block, '\n'); 
        if (cpu_block) {
            cpu_block++; 
        }

        core_id = strstr(cpu_block, "core id");
        if (core_id) {
            online_cpus++;
        }
    }

    sprintf(msg + strlen(msg), "\033[33mCPUs:\033[0m\t%d / %d\n", online_cpus, total_cpus);
}

void getMemoryInfo(char *msg) {
    struct file *meminfo_file;
    char meminfo_buffer[512]; 
    unsigned long long total_memory = 0;
    unsigned long long free_memory = 0;

    meminfo_file = filp_open("/proc/meminfo", O_RDONLY, 0);
    if (!IS_ERR(meminfo_file)) {
        memset(meminfo_buffer, 0, sizeof(meminfo_buffer));
        kernel_read(meminfo_file, meminfo_buffer, sizeof(meminfo_buffer), &meminfo_file->f_pos);
        filp_close(meminfo_file, NULL);

        sscanf(meminfo_buffer, "MemTotal: %llu kB\nMemFree: %llu kB", &total_memory, &free_memory);
        total_memory /= 1024; 
        free_memory /= 1024; 

        sprintf(msg + strlen(msg), "\033[33mMem:\033[0m\t\t%llu MB / %llu MB\n", free_memory, total_memory);
    } else {
        pr_err("Failed to open /proc/meminfo file\n");
    }
}

void get_process_info(char *msg) {
    struct task_struct *task;
    int process_count = 0;

    for_each_process(task) {
        process_count++;
    }
    
    if (process_count) {
        sprintf(msg + strlen(msg), "\033[33mProcs:\033[0m\t%d\n", process_count);
    } else {
        pr_err("Failed to fetch the number of process\n");
        sprintf(msg + strlen(msg), "Failed to fetch the number of process\n");
    }
}

void get_uptime_info(char *msg) {
    struct file *uptime_file;
    char uptime_buffer[512]; 
    unsigned long uptime_seconds = 0;
    unsigned int uptime_minutes;

    uptime_file = filp_open("/proc/uptime", O_RDONLY, 0);
    if (IS_ERR(uptime_file)) {
        pr_err("Failed to open /proc/uptime file\n");
        return;
    }

    memset(uptime_buffer, 0, sizeof(uptime_buffer));
    kernel_read(uptime_file, uptime_buffer, sizeof(uptime_buffer), &uptime_file->f_pos);
    filp_close(uptime_file, NULL);


    sscanf(uptime_buffer, "%lu", &uptime_seconds);
    uptime_minutes = uptime_seconds / 60;

    sprintf(msg + strlen(msg), "\033[33mUptime:\033[0m\t%u mins\n", uptime_minutes);
}
 
/* Called when a process tries to open the device file, like 
 * "sudo cat /dev/chardev" 
 */ 
static int device_open(struct inode *inode, struct file *file) { 
    if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN)) 
        return -EBUSY; 
        
    try_module_get(THIS_MODULE); 
 
    return SUCCESS; 
} 
 
/* Called when a process closes the device file. */ 
static int device_release(struct inode *inode, struct file *file) 
{ 
    // Indicate that the device is no longer in use
    atomic_set(&already_open, CDEV_NOT_USED); 
 
    // Decrement the module's use count
    module_put(THIS_MODULE); 
 
    return SUCCESS; 
} 
 
/* Called when a process, which already opened the dev file, attempts to 
 * read from it. 
 */ 
static ssize_t device_read(struct file *filp, char __user *buffer, size_t len, loff_t *offset) { 
    /* Number of bytes actually written to the buffer */ 
    int bytes_read = 0; 
    const char *msg_ptr = msg; 
 
    if (!*(msg_ptr + *offset)) { /* we are at the end of message */ 
        *offset = 0; /* reset the offset */ 
        return 0; /* signify end of file */ 
    } 
 
    msg_ptr += *offset; 
    len = strlen(msg_ptr);
    if (copy_to_user(buffer, msg_ptr, len)) {
        pr_alert("Failed to copy data to user");
        return 0;
    }

    bytes_read = len;
    *offset += bytes_read; 

    memset(msg, 0, sizeof(msg));
    /* Most read functions return the number of bytes put into the buffer. */ 
    return bytes_read; 

} 

void write_banner(char msg[], char graphic[][COLS + 1], int* currentRow) {
    if (*currentRow < ROWS) {
        sprintf(msg + strlen(msg), "%s", graphic[*currentRow]);
        (*currentRow)++;
    } 
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello */ 
static ssize_t device_write(struct file *filp, const char __user *buff, size_t len, loff_t *off) 
{ 
    int mask_info = 0; // Initialize mask_info
    int currentRow = 0; // config_record
    char graphic[ROWS][COLS + 1] = {
        "        .-.        ",
        "       (.. |       ",
        "       <>  |       ",
        "      / --- \\      ",
        "     ( |   | |     ",
        "   |\\\\_)___/\\)/\\   ",
        "  <__)------(__/   "
    };
    
    // Copy data from user space to kernel space
    if (copy_from_user(&mask_info, buff, sizeof(int))) {
        pr_alert("Failed to copy data from user");
        return -EFAULT; // Return an error code indicating copy failure
    }

    get_hostname(msg);
    // Set the information mask 
    if (mask_info & KFETCH_RELEASE) {
        write_banner(msg, graphic, &currentRow);
        get_kernel_info(msg);
    }

    if (mask_info & KFETCH_CPU_MODEL) {
        write_banner(msg, graphic, &currentRow);
        get_CPU_info(msg);
    }

    if (mask_info & KFETCH_NUM_CPUS) {
        write_banner(msg, graphic, &currentRow);
        get_CPU_counts(msg);
    }

    if (mask_info & KFETCH_MEM) {
        write_banner(msg, graphic, &currentRow);
        getMemoryInfo(msg);
    }

    if (mask_info & KFETCH_NUM_PROCS) {
        write_banner(msg, graphic, &currentRow);
        get_process_info(msg);
    }

    if (mask_info & KFETCH_UPTIME) {
        write_banner(msg, graphic, &currentRow);
        get_uptime_info(msg);
    }
    
    while ( currentRow < ROWS) {
        write_banner(msg, graphic, &currentRow);
        sprintf(msg + strlen(msg), "\n");
    }
    
    return len; // Return the number of bytes written
} 


module_init(chardev_init); 
module_exit(chardev_exit); 
 
MODULE_LICENSE("GPL");
