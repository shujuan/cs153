#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <user/syscall.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

#define MAX_ARGS 3
#define USER_VADDR_BOTTOM ((void *) 0x08048000)

struct lock *syslock;
int fd = 1;

struct process_file
{
	struct file *file;
	int fd;
	struct list_elem elem;
};

int add_file (struct file *f);
struct file* get_file (int fd);

static void syscall_handler (struct intr_frame *);
int user_to_kernel(const void *vaddr);
void get_arg (struct intr_frame *f, int *arg, int n);
void check_ptr (const void *vaddr);
void check_buffer (void* buffer, unsigned size);

void
syscall_init(void)
{
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f UNUSED)
{
	int arg[MAX_ARGS];
	int esp = getpage_ptr((const void *)f->esp);
	switch (*(int *)esp)
	{
		case SYS_HALT:
	{
		halt();
		break;
	}
	case SYS_EXEC:
	{
		get_arg(f, arg, 1);
		arg[0] = user_to_kernel((const void *) arg[0]);
		f->eax = exec((const char *) arg[0]); 
		break;
	}
	 case SYS_WAIT:
      {
		get_arg(f, arg, 1);
		f->eax = wait(arg[0]);
		break;
      }
    case SYS_CREATE:
      {
	get_arg(f, arg, 2);
	arg[0] = user_to_kernel((const void *) arg[0]);
	f->eax = create((const char *)arg[0], (unsigned) arg[1]);
	break;
      }
    case SYS_REMOVE:
      {
	get_arg(f, arg, 1);
	arg[0] = user_to_kernel((const void *) arg[0]);
	f->eax = remove((const char *) arg[0]);
	break;
      }
    case SYS_OPEN:
      {
	get_arg(f, arg, 1);
	arg[0] = user_to_kernel((const void *) arg[0]);
	f->eax = open((const char *) arg[0]);
	break; 		
      }
    case SYS_FILESIZE:
      {
	get_arg(f, arg, 1);
	f->eax = filesize(arg[0]);
	break;
      }
    case SYS_READ:
      {
	get_arg(f, arg, 3);
	check_buffer((void *) arg[1], (unsigned) arg[2]);
	arg[1] = user_to_kernel((const void *) arg[1]);
	f->eax = read(arg[0], (void *) arg[1], (unsigned) arg[2]);
	break;
      }
    case SYS_WRITE:
      { 
	get_arg(f, arg, 3);
	check_buffer((void *) arg[1], (unsigned) arg[2]);
	arg[1] = user_to_kernel((const void *) arg[1]);
	f->eax = write(arg[0], (const void *) arg[1],
		       (unsigned) arg[2]);
	break;
      }
    case SYS_SEEK:
      {
	get_arg(f, arg, 2);
	seek(arg[0], (unsigned) arg[1]);
	break;
      } 
    case SYS_TELL:
      { 
	get_arg(f, arg, 1);
	f->eax = tell(arg[0]);
	break;
      }
    case SYS_CLOSE:
      { 
	get_arg(f, arg, 1);
	close(arg[0]);
	break;
      }
    }

}

pid_t exec(const char* cmd_line)
{
  pid_t pid = process_execute(cmd_line);
  struct child_process* cp = get_child(pid);
  ASSERT(cp);
  while (cp->load == NOT_LOADED)
    {
      barrier();
    }
  if (cp->load == LOAD_FAIL)
    {
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
	if (thread_alive(cur_thread->parent))
	{
		cur_thread->cp->status = status; //????????????????
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

/*struct file *find_file(int fd)
{
struct
}*/

int open(const char* file) 
{
	lock_acquire(syslock);
	struct process_file *f = filesys_open(file); 
	if (!f)
	{
		lock_release(syslock);
		return -1;
	}
	else
	{
		fd++;
		f->fd = fd;
		list_push_back(&thread_current()->file_list, &f->fd);
		lock_release(syslock);
		return fd;                         
	}
}

int filesize(int fd)
{
	lock_acquire(syslock);
	struct file* f = process_get_file(fd);
	if (!f)								//return -1 if can't the file has not been opened
	{
		lock_release(syslock);
		return ERROR;
	}
	int fs = file_length(f);			//return filesize if the file can be found
	lock_release(syslock);
	return fs;
}

int read(int fd, void *buffer, unsigned size)
{
  if (fd == STDIN_FILENO)
    {
      unsigned i;
      uint8_t* local_buffer = (uint8_t *) buffer;
      for (i = 0; i < size; i++)
	{
	  local_buffer[i] = input_getc();
	}
      return size;
    }


	lock_acquire(syslock);				// read from the file
	struct file *f = get_file(fd);
	int bytes_read = file_read(f, buffer, size);
	lock_release(syslock);
	return bytes_read;
}

int write(int fd, const void* buffer, unsigned size)
{
	if (fd == STDOUT_FILENO)			// write buffer to console if fd = STDOUT_FILENO
	{
		putbuf(buffer, size);
		return size;
	}

	lock_acquire(syslock);				// write buffer to file otherwise
	struct file *f = get_file(fd);
	int bytes_written = file_write(f, buffer, size);
	lock_release(syslock);
	return bytes_written;
}

void seek(int fd, unsigned position)
{
	lock_acquire(syslock);
	struct file *f = get_file(fd);
	file_seek(f, position);
	lock_release(syslock);
}

unsigned tell(int fd)
{
	unsigned position;
	lock_acquire(syslock);
	struct file *f = get_file(fd);
	position = (unsigned)file_tell(f);
	lock_release(syslock);
	return position;
}

void close(int fd)
{
	lock_acquire(syslock);
	struct file *f = get_file(fd);
	file_close(f);
	lock_release(syslock);
}

void check_ptr (const void *vaddr)
{
  if (!is_user_vaddr(vaddr) || vaddr < USER_VADDR_BOTTOM)
    {
      exit(ERROR);
    }
}

int user_to_kernel(const void *vaddr)
{
  // TO DO: Need to check if all bytes within range are correct
  // for strings + buffers
  check_ptr(vaddr);
  void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
  if (!ptr)
    {
      exit(ERROR);
    }
  return (int) ptr;
}

int add_file (struct file *f)
{
  struct process_file *pf = malloc(sizeof(struct process_file));
  pf->file = f;
  pf->fd = thread_current()->fd;
  thread_current()->fd++;
  list_push_back(&thread_current()->file_list, &pf->elem);
  return pf->fd;
}


struct file* get_file(int fd)
{
	struct thread *t = thread_current();
	struct list_elem *e;
	for (e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e))
	{
		struct process_file *pf = list_entry(e, struct process_file, elem);
		if (fd == pf->fd)
			return pf->file;
	}
	return NULL;
}

void close_file (int fd)
{
  struct thread *t = thread_current();
  struct list_elem *next, *e = list_begin(&t->file_list);

  while (e != list_end (&t->file_list))
    {
      next = list_next(e);
      struct process_file *pf = list_entry (e, struct process_file, elem);
      if (fd == pf->fd || fd == CLOSE_ALL)
	{
	  file_close(pf->file);
	  list_remove(&pf->elem);
	  free(pf);
	  if (fd != CLOSE_ALL)
	    {
	      return;
	    }
	}
      e = next;
    }
}

struct child_process* add_child (int pid)
{
  struct child_process* cp = malloc(sizeof(struct child_process));
  cp->pid = pid;
  cp->load = NOT_LOADED;
  cp->wait = false;
  cp->exit = false;
  lock_init(&cp->wait_lock);
  list_push_back(&thread_current()->child_list,
		 &cp->elem);
  return cp;
}

struct child_process* get_child (int pid)
{
  struct thread *t = thread_current();
  struct list_elem *e;

  for (e = list_begin (&t->child_list); e != list_end (&t->child_list);
       e = list_next (e))
        {
          struct child_process *cp = list_entry (e, struct child_process, elem);
          if (pid == cp->pid)
	    {
	      return cp;
	    }
        }
  return NULL;
}

/*void remove_child (struct child_process *cp)
{
  list_remove(&cp->elem);
  free(cp);
}

void remove_child (void)
{
  struct thread *t = thread_current();
  struct list_elem *next, *e = list_begin(&t->child_list);

  while (e != list_end (&t->child_list))
    {
      next = list_next(e);
      struct child_process *cp = list_entry (e, struct child_process,
					     elem);
      list_remove(&cp->elem);
      free(cp);
      e = next;
    }
}*/

void get_arg(struct intr_frame *f, int *arg, int n)
{
  int i;
  int *ptr;
  for (i = 0; i < n; i++)
    {
      ptr = (int *) f->esp + i + 1;
      check_ptr((const void *) ptr);
      arg[i] = *ptr;
    }
}

void check_buffer (void* buffer, unsigned size)
{
  unsigned i;
  char* local_buffer = (char *) buffer;
  for (i = 0; i < size; i++)
    {
      check_ptr((const void*) local_buffer);
      local_buffer++;
    }
}
