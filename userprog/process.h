#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
static void start_process(void *file_name_);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool load(const char *file_name, void(**eip) (void), void **esp, char **saveptr);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
static bool setup_stack(void **esp, const char* file_name, char** save_ptr);


#endif /* userprog/process.h */
