#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

#define MAX_ARGS 3

struct lock *syslock;
int fd = 1;

struct process_file
{
	struct file *file;
	int fd;
	struct list_elem elem;
};

void
syscall_init(void)
{
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f UNUSED)
{
	int arg[MAX_ARGS];
	int esp = getpage_prt((const void *)f->esp);
	switch (*(int *)esp)
	{
		case SYS_HALT:
	{
		halt();
		break;
	}
	case SYS_EXEC:
	{
		get_args(f, arg, 1);
		check_string((const void *)arg[0]);
		f->eax = exec((const void *)arg[0]);
		break;
	}
/*	case SYS_EXEC:
	case SYS_WAIT:
	case SYS_CREATE:
	case SYS_REMOVE:
	case SYS_OPEN:
	case SYS_FILESIZE:
	case SYS_READ:
	case SYS_WRITE:
	case SYS_SEEK:
	case SYS_TELL:
	case SYS_CLOSE:*/
	}
}

pid_t exec(const char* cmd_line)
{
	pid_t pid = process_execute(cmd_line);
	struct child_process *cp = get_child_process(pid);
	if (!cp) { return ERROR; }
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
	if (thread_alive(cur_thread->parent))
	{
		cur_thread->parent->status = status; //????????????????
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
	if (fd == STDIN_FILENO)				// read from keyboard if fd = 0
	{
		int i = 0;
		char c;
		while (i < size && (c = input_getc()) != "\n")
		{
			
			&buffer += c; // ??? problematic
			i++;
		}
		return i;
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


static void
syscall_handler(struct intr_frame *f UNUSED)
{
	printf("system call!\n");
	thread_exit();
}
