#ifndef JOB_MANAGER_H
#define JOB_MANAGER_H

#include <stdint.h>

typedef int32_t jid_t;
typedef void * (* job_func_t)(void *, void *, uint32_t);
typedef void (* job_free_func_t)(void *, void *,  uint32_t);
typedef void (* job_callback)(void *);

enum {
  READY,
  RUNNING,
  TERMINATED
} job_state;

typedef struct {
  job_func_t run;
  job_free_func_t free;
} job_impl;

typedef struct {
  job_callback on_termination;
  job_callback on_expiration;
} job_callbacks;

extern void job_manager_start(void);

extern void job_manager_stop(void);

extern jid_t job_add(job_impl implementation, void * args, job_callbacks callbacks, uint32_t memsize, uint32_t timeout);

extern void * job_get_return_value(jid_t jid);

extern job_state job_get_status(jid_t jid);

extern int job_cancel(jid_t jid);

#endif
