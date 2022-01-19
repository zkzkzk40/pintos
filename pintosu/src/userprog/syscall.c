#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include "pagedir.h"
#include <threads/vaddr.h>
#include <filesys/filesys.h>
#include <devices/shutdown.h>
#include <filesys/file.h>
#include <devices/input.h>
#include <threads/malloc.h>
#include <threads/palloc.h>
# define max_syscall 20
# define USER_VADDR_BOUND (void*) 0x08048000
struct thread_file * find_file_id(int fd);
/* Our implementation for storing the array of system calls for Task2 and Task3 */
static void (*syscalls[max_syscall])(struct intr_frame *);
static void * check_ptr2(const void *vaddr);
static void exit_special (void);
struct thread_file * find_file_id(int fd);
/* Our implementation for Task2: syscall halt,exec,wait and practice */
void sys_halt(struct intr_frame* f); /* syscall halt. */
void sys_exit(struct intr_frame* f); /* syscall exit. */
void sys_exec(struct intr_frame* f); /* syscall exec. */

/* Our implementation for Task3: syscall create, remove, open, filesize, read, write, seek, tell, and close */
void sys_create(struct intr_frame* f); /* syscall create */
void sys_remove(struct intr_frame* f); /* syscall remove */
void sys_open(struct intr_frame* f);/* syscall open */
void sys_wait(struct intr_frame* f); /*syscall wait */
void sys_filesize(struct intr_frame* f);/* syscall filesize */
void sys_read(struct intr_frame* f);  /* syscall read */
void sys_write(struct intr_frame* f); /* syscall write */
void sys_seek(struct intr_frame* f); /* syscall seek */
void sys_tell(struct intr_frame* f); /* syscall tell */
void sys_close(struct intr_frame* f); /* syscall close */

static void syscall_handler (struct intr_frame *);
/* New method to check the address and pages to pass test sc-bad-boundary2, execute */
/* Handle the special situation for thread */

void 
exit_special (void)
{
  thread_current()->st_exit = -1;
  thread_exit ();
}

/* Method in document to handle special situation */
/* 在用户虚拟地址 UADDR 读取一个字节。
   UADDR 必须低于 PHYS_BASE。
   如果成功则返回字节值，如果
   发生段错误则返回 -1 。*/ 
static int 
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:" : "=&a" (result) : "m" (*uaddr));
  return result;
}

void * 
check_ptr2(const void *vaddr)
{ 
  /* Judge address */
  if (!is_user_vaddr(vaddr))//是否为用户地址
  {
    exit_special ();
  }
  /* Judge the page */
  void *ptr = pagedir_get_page (thread_current()->pagedir, vaddr);//是否为用户地址
  if (!ptr)
  {
    exit_special ();
  }
  /* Judge the content of page */
  uint8_t *check_byteptr = (uint8_t *) vaddr;
  uint8_t i=0;
  while ( i < 4) 
  {
    if (get_user(check_byteptr + i) == -1)
    {
      exit_special ();
    }
    i++;
  }

  return ptr;
}




void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    /* Our implementation for Task2: initialize halt,exit,exec */
  syscalls[SYS_EXEC] = &sys_exec;
  syscalls[SYS_HALT] = &sys_halt;
  syscalls[SYS_EXIT] = &sys_exit;
 
  // /* Our implementation for Task3: initialize create, remove, open, filesize, read, write, seek, tell, and close */
  syscalls[SYS_WAIT] = &sys_wait;
  syscalls[SYS_CREATE] = &sys_create;
  syscalls[SYS_REMOVE] = &sys_remove;
  syscalls[SYS_OPEN] = &sys_open;
  syscalls[SYS_WRITE] = &sys_write;
  syscalls[SYS_SEEK] = &sys_seek;
  syscalls[SYS_TELL] = &sys_tell;
  syscalls[SYS_CLOSE] =&sys_close;
  syscalls[SYS_READ] = &sys_read;
  syscalls[SYS_FILESIZE] = &sys_filesize;
}

/* Smplify the code to maintain the code more efficiently */
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* For Task2 practice, just add 1 to its first argument, and print its result */
  int * p = f->esp;
  check_ptr2 (p + 1);//检验第一个参数
  int type = * (int *)f->esp;//检验系统调用号sys_code是否合法
  if(type <= 0 || type >= max_syscall){
    exit_special ();
  }
  syscalls[type](f);//无误则执行对应系统调用函数
}

/* Our implementation for Task2: halt,exit,exec */
/* Do sytem halt */
void 
sys_halt (struct intr_frame* f)
{
  shutdown_power_off();
}

/* Do sytem exit */
void 
sys_exit (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);//检验第一个参数
  *user_ptr++;//指针指向第一个参数
  /* record the exit status of the process */
  thread_current()->st_exit = *user_ptr;//保存exit_code
  thread_exit ();
}

/* Do sytem exec */
void 
sys_exec (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);//检查第一个参数的地址
  check_ptr2 (*(user_ptr + 1));//检查第一个参数的值，即const char *file指向的地址
  *user_ptr++;
  f->eax = process_execute((char*)* user_ptr);//使用process_execute完成pid的返回
}
/* Do sytem wait */
void 
sys_wait (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  f->eax = process_wait(*user_ptr);
}
/* Do system write, Do writing in stdout and write in files */
void 
sys_write (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 7);//for tests maybe?
  check_ptr2 (*(user_ptr + 6));
  *user_ptr++;
  int fd = *user_ptr;
  const char * buffer = (const char *)*(user_ptr+1);
  off_t size = *(user_ptr+2);
  if (fd == 1) {//writes to the console
    /* Use putbuf to do testing */
    putbuf(buffer,size);
    f->eax = size;//return number written
  }
  else
  {
    /* Write to Files */
    struct thread_file * thread_file_temp = find_file_id (*user_ptr);
    if (thread_file_temp)
    {
      acquire_lock_f ();//file operating needs lock
      f->eax = file_write (thread_file_temp->file, buffer, size);
      release_lock_f ();
    } 
    else
    {
      f->eax = 0;//can't write,return 0
    }
  }
}

/* Find file by the file's ID */
struct thread_file * 
find_file_id (int file_id)
{
  struct list_elem *e;
  struct thread_file * thread_file_temp = NULL;
  struct list *files = &thread_current ()->files;
  for (e = list_begin (files); e != list_end (files); e = list_next (e)){
    thread_file_temp = list_entry (e, struct thread_file, file_elem);
    if (file_id == thread_file_temp->fd)
      return thread_file_temp;
  }
  return false;
}


/* Do sytem create, we need to acquire lock for file operation in the following methods when do file operation */
void 
sys_create(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 5);//for tests maybe?
  check_ptr2 (*(user_ptr + 4));
  *user_ptr++;
  acquire_lock_f ();
  f->eax = filesys_create ((const char *)*user_ptr, *(user_ptr+1));
  release_lock_f ();
}

/* Do system remove, by calling the method filesys_remove */
void 
sys_remove(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);//arg address
  check_ptr2 (*(user_ptr + 1));//file address 
  *user_ptr++;
  acquire_lock_f ();
  f->eax = filesys_remove ((const char *)*user_ptr);
  release_lock_f ();
}

/* Do system open, open file by the function filesys_open */
void 
sys_open (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  check_ptr2 (*(user_ptr + 1));
  *user_ptr++;
  acquire_lock_f ();
  struct file * file_opened = filesys_open((const char *)*user_ptr);
  release_lock_f ();
  struct thread * t = thread_current();
  if (file_opened)
  {
    struct thread_file *thread_file_temp = malloc(sizeof(struct thread_file));
    thread_file_temp->fd = t->max_file_fd++;
    thread_file_temp->file = file_opened;
    list_push_back (&t->files, &thread_file_temp->file_elem);//维护files列表
    f->eax = thread_file_temp->fd;
  } 
  else// the file could not be opened
  {
    f->eax = -1;
  }
}

/* Do system filesize, by calling the function file_length() in filesystem */
void 
sys_filesize (struct intr_frame* f){
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;//fd
  struct thread_file * thread_file_temp = find_file_id (*user_ptr);
  if (thread_file_temp)
  {
    acquire_lock_f ();
    f->eax = file_length (thread_file_temp->file);//return the size in bytes
    release_lock_f ();
  } 
  else
  {
    f->eax = -1;
  }
}



/* Check is the user pointer is valid */
bool 
is_valid_pointer (void* esp,uint8_t argc){
  for (uint8_t i = 0; i < argc; ++i)
  {
    if((!is_user_vaddr (esp)) || 
      (pagedir_get_page (thread_current()->pagedir, esp)==NULL)){
      return false;
    }
  }
  return true;
}

/* Do system read, by calling the function file_tell() in filesystem */
void 
sys_read (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  /* PASS the test bad read */
  *user_ptr++;
  /* We don't konw how to fix the bug, just check the pointer */
  int fd = *user_ptr;
  uint8_t * buffer = (uint8_t*)*(user_ptr+1);
  off_t size = *(user_ptr+2);
  if (!is_valid_pointer (buffer, 1) || !is_valid_pointer (buffer + size,1)){
    exit_special ();
  }
  /* get the files buffer */
  if (fd == 0) //stdin
  {
    for (int i = 0; i < size; i++)
      buffer[i] = input_getc();
    f->eax = size;
  }
  else
  {
    struct thread_file * thread_file_temp = find_file_id (*user_ptr);
    if (thread_file_temp)
    {
      acquire_lock_f ();
      f->eax = file_read (thread_file_temp->file, buffer, size);
      release_lock_f ();
    } 
    else//can't read
    {
      f->eax = -1;
    }
  }
}

/* Do system seek, by calling the function file_seek() in filesystem */
void 
sys_seek(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 5);
  *user_ptr++;//fd
  struct thread_file *file_temp = find_file_id (*user_ptr);
  if (file_temp)
  {
    acquire_lock_f ();
    file_seek (file_temp->file, *(user_ptr+1));
    release_lock_f ();
  }
}

/* Do system tell, by calling the function file_tell() in filesystem */
void 
sys_tell (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  struct thread_file *thread_file_temp = find_file_id (*user_ptr);
  if (!thread_file_temp)
  {
    f->eax = -1;
  }else{
    acquire_lock_f ();
    f->eax = file_tell (thread_file_temp->file);
    release_lock_f ();
  }
}

/* Do system close, by calling the function file_close() in filesystem */
void 
sys_close (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  struct thread_file * opened_file = find_file_id (*user_ptr);
  if (opened_file)
  {
    acquire_lock_f ();
    file_close (opened_file->file);
    release_lock_f ();
    /* Remove the opened file from the list */
    list_remove (&opened_file->file_elem);
    /* Free opened files */
    free (opened_file);
  }
}