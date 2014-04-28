#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

#define MAX_ARGS 3

struct lock *syslock;
int fd = 1;
struct list file_list;
list_init(file_list);

static void syscall_handler (struct intr_frame *f UNUSED)
{
  int arg[MAX_ARGS];
  int esp = getpage_prt((const void *)f->esp);
  switch (*(int *)esp)
  case SYS_HALT
  {
    halt();
    break;
  }
  case SYS_EXEC
  {
    get_args(f, arg, 1);
    check_string((const void *) argv[0]);
    f->eax = exec((const void *)argv[0]);
    break;
  }
}

pid_t exec(const char* cmd_line)
{
  pid_t pid = process_execute(cmd_line);
  struct child_process *cp = get_child_process(pid);
  if (!cp) {return ERROR;}
  if (cp->load == NOT_LOADED)
  {
    sema_down(&cp->load_sema);
  }
  if (cp->load == LOAD_FAIL)
  {
    remove_cp(cp);
    return ERROR;
  }
  return pid;
}

void halt(void)
{
  shutdown_power_off();
}

void exit(int status)
{
  struct thread *cur_thread = thread_current();
  if (thread_alive(cur_thread -> parent))
  {
    cur_thread -> parent -> status = status; //????????????????
  }
  thread_exit();
}

int wait(pid_t pid)
{
  return process_wait(pid);
}

bool create(const char* file, unsigned initial_size)
{
  lock_acquire(syslock);
  bool success = filesys_create(file, initial_size);
  lock_release(syslock);
  return success;
}

bool remove(const char* file)
{
  lock_acquire(syslock);
  bool success = filesys_remove(file);
  lock_release(syslock);
  return success;
}

int open(const char* file)
{
  lock_acquire(syslock);
  struct file* f = filesys_open(file);
  if(!f)
  {
    lock_release(syslock);
    return -1;
  }
  else
  {
    fd++;
    f->fd = fd;
    list_push_back(&file_list,&f->fd);
    lock_release(syslock);
    return fd;                            //???????????????????/
  }
}

int filesize(int fd)
{
  struct list_elem *e;
  lock_acquire(syslock);
  for (e = list_begin (&f->file_list); e != list_end (&f->file_list);
       e = list_next (e))
  {
    struct file *f = list_entry (e, struct file, elem);
    if (fd == f->fd)
	    {
	      return f->file;
	    }
  }
}

int read (int fd, void *buffer, unsigned size)
{
  if(fd == 0)
  {
    int i;
    int*    /////////////////////////////////////// to resume
  }
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}
