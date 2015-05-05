#include "job_manager.h"

#include <unistd.h>
#include <sys/times.h>
#include <signal.h>
#include <pthread.h>

#define MAX_JOBS 128

typedef struct job_struct {
  pthread_t thread;
  job_state status;
  uint32_t expired_tick;
  void * return_value;
  void * args;
  void * memory;
  uint32_t memsize;
  job_callbacks callbacks;
  job_impl implementation;
  struct job_struct * next;
  struct job_struct * prev;
} job_struct;

static job_struct * job_array[MAX_JOBS] = {NULL};
static jid_t id_counter = 0;
static pid_t job_manager_pid = 0;
static job_struct * job_list = NULL;
static lock * job_list_lock = NULL;

static void _job_manager_termination_handler(int signum);

static void _job_manager_run(void);

static int _job_start(job_struct * job);

static void * _job_run_routine(void * args);

static void _job_terminate(job_struct * job);

static void _job_free(job_struct * job);

static job_struct * _job_by_jid(jid_t jid);

void _job_manager_run(void) {
  while (1) {
    while (job_list != NULL && job_list->expired_tick > times(NULL)) {
      usleep(1);
    }
    
    if (job_list != NULL) {
      job_list_lock->acquire();
      _job_free(job_list)
      job_list_lock->release();
    }
    
    usleep(1);
  }
}

void _job_manager_termination_handler(int signum) {
  job_struct * job;
  for (job = job_list; job != NULL; job = job->next) {
    _job_terminate(job);
    _job_free(job);
  }
  raise(SIGTERM);
}

void job_manager_start(void) {
  // TODO
}

void job_manager_stop(void) {
  kill(SIGINT, job_manager_pid);
}

job_struct * _job_by_jid(jid_t jid) {
  if (jid > id_counter)
    return NULL;
  return job_array[jid % MAX_JOBS];
}

void * _job_run_routine(void * args) {
  job_struct * job = (job_struct *) args;
  void * return_value = job->implementation.run(job->args, job->memory, job->memsize);
  job->return_value = return_value;
  _job_terminate(job);
  return NULL;
}

int _job_start(job_struct * job) {
  job->status = RUNNING;
  return pthead_create(&job->thread, NULL, _job_run_routine, (void *) job);
}

void _job_terminate(job_struct * job) {
  if (job->status != TERMINATED) {
    if (job->implementation.free != NULL) {
      job->implementation.free(job->args, job->memory, job->memsize);
    }
    if (job->callbacks.on_termination != NULL) {
      job->callbacks.on_termination(job->return_value);
    }
    job->status = TERMINATED;
  }
}

void _job_free(job_struct * job) {
  job_list = _job_list_remove(job);
  
  pthread_cancel(job->thread);
   _job_terminate(job);
  if (job->callbacks.on_expiration != NULL) {
    job->callbacks.on_expiration(job->return_value);
  }
  free(job->memory);
  free(job->return_value);
  free(job);
}

jid_t job_add(job_impl implementation, void * args, job_callbacks callbacks, uint32_t memsize, uint32_t timeout) {
  job_struct * job = malloc(sizeof(job_struct));
  if (job == NULL)
    return -1;
  
  job->status = READY;
  if (timeout == 0) {
    job->expired_tick = 0;
  }
  else {
    job->expired_tick = timeout + times(NULL);
  }
  job->return_value = NULL;
  job->args = args;
  job->memory = calloc(memsize, sizeof(void *));
  if (job->memory == NULL) {
    free(job);
    return -1;
  }
  job->memsize = memsize;
  job->callbacks = callbacks;
  job->implementation = implementation;
  job->prev = NULL;
  job->next = NULL;
  
  _job_list_insert(job);
  jid_t id = _job_array_insert(job);
  _job_start(job);
  return id;
}

void * job_get_return_value(jid_t jid) {
  job_struct * job = _job_by_jid(jid);
  if (job == NULL) {
    return NULL;
  }
  return job->return_value;
}

job_state job_get_status(jid_t jid); {
  job_struct * job = _job_by_jid(jid);
  if (job == NULL) {
    return TERMINATED;
  }
  return job->status;
}
int job_cancel(jid_t jid) {
  job_struct * job = _job_by_jid(jid);
  if (job == NULL) {
    return -1;
  }
  _job_free(job);
}
