#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#define MAX_ARGS 3

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
  if (ep->load == LOAD_FAIL)
  {
    remove_cp(cp);
    return ERROR;
  }
  return pid;
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
