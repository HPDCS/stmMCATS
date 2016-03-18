/*
 * File:
 *   stm.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   STM functions.
 *
 * Copyright (c) 2007-2012.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program has a dual license and can also be distributed
 * under the terms of the MIT license.
 */

#ifndef __USE_GNU
#define __USE_GNU
#endif
#define _GNU_SOURCE
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <pthread.h>


#include <sched.h>

#include "stm.h"
#include "stm_internal.h"

#include "utils.h"
#include "atomic.h"
#include "gc.h"

/* ################################################################### *
 * DEFINES
 * ################################################################### */


#ifdef PRINT_STATS_INFO
#define PRINT_STATS(...)			printf(__VA_ARGS__)
#else
#define PRINT_STATS(...)
#endif

/* Indexes are defined in stm_internal.h  */
static const char *design_names[] = {
  /* 0 */ "WRITE-BACK (ETL)",
  /* 1 */ "WRITE-BACK (CTL)",
  /* 2 */ "WRITE-THROUGH",
  /* 3 */ "WRITE-MODULAR"
};

static const char *cm_names[] = {
  /* 0 */ "SUICIDE",
  /* 1 */ "DELAY",
  /* 2 */ "BACKOFF",
  /* 3 */ "MODULAR"
};

/* Global variables */
global_t _tinystm =
    { .nb_specific = 0
    , .initialized = 0
#ifdef IRREVOCABLE_ENABLED
    , .irrevocable = 0
#endif /* IRREVOCABLE_ENABLED */
    };

#  ifdef STM_MCATS
	volatile stm_word_t running_transactions;
	volatile stm_word_t enqueued_transactions;
	volatile stm_word_t max_allowed_running_transactions;
	unsigned long max_concurrent_threads;
	int scaling;
	int min_queue_length_for_scaling;

#endif /* ! STM_MCATS */


/* ################################################################### *
 * TYPES
 * ################################################################### */


/*
 * Transaction nesting is supported in a minimalist way (flat nesting):
 * - When a transaction is started in the context of another
 *   transaction, we simply increment a nesting counter but do not
 *   actually start a new transaction.
 * - The environment to be used for setjmp/longjmp is only returned when
 *   no transaction is active so that it is not overwritten by nested
 *   transactions. This allows for composability as the caller does not
 *   need to know whether it executes inside another transaction.
 * - The commit of a nested transaction simply decrements the nesting
 *   counter. Only the commit of the top-level transaction will actually
 *   carry through updates to shared memory.
 * - An abort of a nested transaction will rollback the top-level
 *   transaction and reset the nesting counter. The call to longjmp will
 *   restart execution before the top-level transaction.
 * Using nested transactions without setjmp/longjmp is not recommended
 * as one would need to explicitly jump back outside of the top-level
 * transaction upon abort of a nested transaction. This breaks
 * composability.
 */

/*
 * Reading from the previous version of locked addresses is implemented
 * by peeking into the write set of the transaction that owns the
 * lock. Each transaction has a unique identifier, updated even upon
 * retry. A special "commit" bit of this identifier is set upon commit,
 * right before writing the values from the redo log to shared memory. A
 * transaction can read a locked address if the identifier of the owner
 * does not change between before and after reading the value and
 * version, and it does not have the commit bit set.
 */

/* ################################################################### *
 * THREAD-LOCAL
 * ################################################################### */

#if defined(TLS_POSIX) || defined(TLS_DARWIN)
/* TODO this may lead to false sharing. */
/* TODO this could be renamed with tinystm prefix */
pthread_key_t thread_tx;
pthread_key_t thread_gc;
#elif defined(TLS_COMPILER)
__thread stm_tx_t* thread_tx = NULL;
__thread long thread_gc = 0;
#endif /* defined(TLS_COMPILER) */

/* ################################################################### *
 * STATIC
 * ################################################################### */

#if CM == CM_MODULAR
/*
 * Kill other.
 */
static int
cm_aggressive(struct stm_tx *me, struct stm_tx *other, int conflict)
{
  return KILL_OTHER;
}

/*
 * Kill self.
 */
static int
cm_suicide(struct stm_tx *me, struct stm_tx *other, int conflict)
{
  return KILL_SELF;
}

/*
 * Kill self and wait before restart.
 */
static int
cm_delay(struct stm_tx *me, struct stm_tx *other, int conflict)
{
  return KILL_SELF | DELAY_RESTART;
}

/*
 * Oldest transaction has priority.
 */
static int
cm_timestamp(struct stm_tx *me, struct stm_tx *other, int conflict)
{
  if (me->timestamp < other->timestamp)
    return KILL_OTHER;
  if (me->timestamp == other->timestamp && (uintptr_t)me < (uintptr_t)other)
    return KILL_OTHER;
  return KILL_SELF | DELAY_RESTART;
}

/*
 * Transaction with more work done has priority.
 */
static int
cm_karma(struct stm_tx *me, struct stm_tx *other, int conflict)
{
  unsigned int me_work, other_work;

  me_work = (me->w_set.nb_entries << 1) + me->r_set.nb_entries;
  other_work = (other->w_set.nb_entries << 1) + other->r_set.nb_entries;

  if (me_work < other_work)
    return KILL_OTHER;
  if (me_work == other_work && (uintptr_t)me < (uintptr_t)other)
    return KILL_OTHER;
  return KILL_SELF;
}

struct {
  const char *name;
  int (*f)(stm_tx_t *, stm_tx_t *, int);
} cms[] = {
  { "aggressive", cm_aggressive },
  { "suicide", cm_suicide },
  { "delay", cm_delay },
  { "timestamp", cm_timestamp },
  { "karma", cm_karma },
  { NULL, NULL }
};
#endif /* CM == CM_MODULAR */

#ifdef SIGNAL_HANDLER
/*
 * Catch signal (to emulate non-faulting load).
 */
static void
signal_catcher(int sig)
{
  sigset_t block_signal;
  stm_tx_t *tx = tls_get_tx();

  /* A fault might only occur upon a load concurrent with a free (read-after-free) */
  PRINT_DEBUG("Caught signal: %d\n", sig);

  /* TODO: TX_KILLED should be also allowed */
  if (tx == NULL || tx->attr.no_retry || GET_STATUS(tx->status) != TX_ACTIVE) {
    /* There is not much we can do: execution will restart at faulty load */
    fprintf(stderr, "Error: invalid memory accessed and no longjmp destination\n");
    exit(1);
  }

  /* Unblock the signal since there is no return to signal handler */
  sigemptyset(&block_signal);
  sigaddset(&block_signal, sig);
  pthread_sigmask(SIG_UNBLOCK, &block_signal, NULL);

  /* Will cause a longjmp */
  stm_rollback(tx, STM_ABORT_SIGNAL);
}
#endif /* SIGNAL_HANDLER */


/* ################################################################### *
 * STM FUNCTIONS
 * ################################################################### */

/*
 * Called once (from main) to initialize STM infrastructure.
 */
#  ifdef STM_MCATS

void stm_init(int threads) {

	max_concurrent_threads = threads;

	FILE* fid;
	if ((fid = fopen("mcats_conf.txt", "r")) == NULL) {
		printf("\nError opening MCATS configuration file.\n");
		exit(1);
	}

	if (fscanf(fid, "SCALING=%d INITIAL_MAX_ADMITTED_TX=%d MIN_QUEUE_LENGTH_FOR_SCALING=%d", &scaling, &max_allowed_running_transactions, &min_queue_length_for_scaling)!=3) {
		printf("\nThe number of input parameters of the MCATS configuration file does not match the number of required parameters.\n");
		exit(1);
	}

	running_transactions=0;
	enqueued_transactions=0;


  	/* Set on conflict callback */

#else

void stm_init()
{

#endif /* ! STM_MCATS */

#if CM == CM_MODULAR
  char *s;
#endif /* CM == CM_MODULAR */
#ifdef SIGNAL_HANDLER
  struct sigaction act;
#endif /* SIGNAL_HANDLER */

  PRINT_DEBUG("==> stm_init()\n");

  if (_tinystm.initialized)
    return;

  PRINT_DEBUG("\tsizeof(word)=%d\n", (int)sizeof(stm_word_t));

  PRINT_DEBUG("\tVERSION_MAX=0x%lx\n", (unsigned long)VERSION_MAX);

  COMPILE_TIME_ASSERT(sizeof(stm_word_t) == sizeof(void *));
  COMPILE_TIME_ASSERT(sizeof(stm_word_t) == sizeof(atomic_t));

#ifdef EPOCH_GC
  gc_init(stm_get_clock);
#endif /* EPOCH_GC */

#if CM == CM_MODULAR
  s = getenv(VR_THRESHOLD);
  if (s != NULL)
    _tinystm.vr_threshold = (int)strtol(s, NULL, 10);
  else
    _tinystm.vr_threshold = VR_THRESHOLD_DEFAULT;
  PRINT_DEBUG("\tVR_THRESHOLD=%d\n", _tinystm.vr_threshold);
#endif /* CM == CM_MODULAR */

  /* Set locks and clock but should be already to 0 */
  memset((void *)_tinystm.locks, 0, LOCK_ARRAY_SIZE * sizeof(stm_word_t));
#ifdef INVISIBLE_TRACKING
  memset((void* )_tinystm.last_id_tx_class,-1,LOCK_ARRAY_SIZE * sizeof(stm_word_t));
#endif
  CLOCK = 0;

  stm_quiesce_init();

  tls_init();

#ifdef SIGNAL_HANDLER
  if (getenv(NO_SIGNAL_HANDLER) == NULL) {
    /* Catch signals for non-faulting load */
    act.sa_handler = signal_catcher;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    if (sigaction(SIGBUS, &act, NULL) < 0 || sigaction(SIGSEGV, &act, NULL) < 0) {
      perror("sigaction");
      exit(1);
    }
  }
#endif /* SIGNAL_HANDLER */
  _tinystm.initialized = 1;
}

/*
 * Called once (from main) to clean up STM infrastructure.
 */
_CALLCONV void
stm_exit(void)
{
  PRINT_DEBUG("==> stm_exit()\n");

  if (!_tinystm.initialized)
	  return;
  tls_exit();
  stm_quiesce_exit();

  char filename[512];
  int cpu_id=0, fd;
  for (cpu_id=0; cpu_id<sysconf(_SC_NPROCESSORS_CONF); cpu_id++) {
	  sprintf(filename, "/sys/devices/system/cpu/cpu%i/cpufreq/scaling_setspeed",cpu_id);
	  //printf("Filename: %s", filename);
	  fd=open(filename, O_WRONLY);
	  if(fd==-1){
		  printf("Error opening file %s \n", filename);
		  exit(1);
	  }
	  char target_freq[]="800000";
	  write(fd, &target_freq, sizeof(target_freq));
	  close(fd);
  }

#ifdef EPOCH_GC
  gc_exit();
#endif /* EPOCH_GC */
  _tinystm.initialized = 0;
}

/*
 * Called by the CURRENT thread to initialize thread-local STM data.
 */

_CALLCONV stm_tx_t *
stm_init_thread(void)
{
  return int_stm_init_thread();
}

/*
 * Called by the CURRENT thread to cleanup thread-local STM data.
 */
_CALLCONV void
stm_exit_thread(void)
{
	TX_GET;
	close(tx->scaling_setspeed_fd);
	int_stm_exit_thread(tx);

}

_CALLCONV void
stm_exit_thread_tx(stm_tx_t *tx)
{

  int_stm_exit_thread(tx);
}

/*
 * Called by the CURRENT thread to start a transaction.
 */
_CALLCONV sigjmp_buf *
stm_start(stm_tx_attr_t attr)
{
  TX_GET;
  sigjmp_buf * ret;
  stm_wait(attr.id);
  ret=int_stm_start(tx, attr);
  return ret;
}

#  ifdef STM_MCATS

int cpu_id[32]={0,4,8,12,16,20,24,28,1,5,9,13,17,21,25,29,2,6,10,14,18,22,26,30,3,7,11,15,19,23,27,31};


static inline void set_affinity(int thread_id) {
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	thread_id=thread_id%32;
	CPU_SET(cpu_id[thread_id], &cpuset);
	//CPU_SET(thread_id, &cpuset);
	// 0 is the current thread
	sched_setaffinity(0, sizeof(cpuset), &cpuset);
}

_CALLCONV stm_tx_t *stm_pre_init_thread(int id){
	stm_tx_t *tx;
	tx=stm_init_thread();
	tx->thread_identifier=id;
	tx->i_am_waiting=0;
	tx->scaled=0;


    //printf("\nSetting thread %i on cpu %i", id, id);
    //fflush(stdout);
    set_affinity(id);


	char filename[512];
	int cpu_id=sched_getcpu();
	sprintf(filename, "/sys/devices/system/cpu/cpu%i/cpufreq/scaling_setspeed",cpu_id);
	//printf("Filename: %s", filename);
	tx->scaling_setspeed_fd=open(filename, O_WRONLY);
    if(tx->scaling_setspeed_fd==-1){
        printf("\nError opening file %s \n", filename);
        exit(1);
    }
	char target_freq[]="2000000";
	write(tx->scaling_setspeed_fd, &target_freq, sizeof(target_freq));
	//printf("\nCore %i freq = %i", cpu_id, 2000000);
	//fflush(stdout);
	return tx;
}

//if (tx->thread_identifier>max_allowed_running_transactions) {
//char target_freq_1[] = "800000";
//write(tx->scaling_setspeed_fd, &target_freq_1, sizeof(target_freq_1));
//}


inline void stm_wait(int id) {

	TX_GET;

	int active_txs, max_txs;
	//tx->CAS_executed = 0;


	while (1) {
		active_txs = running_transactions;
		max_txs = max_allowed_running_transactions;
		if (active_txs < max_txs) {
			if (ATOMIC_CAS_FULL(&running_transactions, active_txs, active_txs + 1) != 0) {
				//tx->CAS_executed = 1;
				return;
			}
		} else
			break;
	}

	if (scaling) {
		int enqueued_txs;
		//transaction enqueued
		while (1) {
			enqueued_txs = enqueued_transactions;
			if (ATOMIC_CAS_FULL(&enqueued_transactions, enqueued_txs, enqueued_txs + 1) != 0) break;
		}
	}


	int i, max_cycles=500000;

	while(1){
		active_txs=running_transactions;
		max_txs=max_allowed_running_transactions;
		if(active_txs<max_txs)
			if (ATOMIC_CAS_FULL(&running_transactions, active_txs, active_txs+1)!= 0) {
				//tx->CAS_executed=1;
				ATOMIC_FETCH_DEC_FULL(&enqueued_transactions);
				break;
			}
		tx->i_am_waiting=1;

		//if (scaling && enqueued_txs>=min_queue_length_for_scaling) {
		//	char target_freq_1[] = "800000";
		//	write(tx->scaling_setspeed_fd, &target_freq_1, sizeof(target_freq_1));
		//	tx->scaled=1;
		//}

		for(i=0;i<max_cycles;i++) {
			//if (enqueued_txs>=min_queue_length_for_scaling && !tx->scaled && scaling) {
			//	char target_freq_1[] = "2000000";
			//	write(tx->scaling_setspeed_fd, &target_freq_1, sizeof(target_freq_1));
			//	tx->scaled=0;
			//}
			if(tx->i_am_waiting==0) {
			//	if (enqueued_txs>=min_queue_length_for_scaling && tx->scaled && scaling) {
			//		char target_freq_1[] = "2000000";
			//		write(tx->scaling_setspeed_fd, &target_freq_1, sizeof(target_freq_1));
			//		tx->scaled=0;
			//	}
				break;
			}
		}
		tx->i_am_waiting=0;
	}



}





#else
_CALLCONV stm_tx_t *stm_pre_init_thread(int id){
	return stm_init_thread();
}
void stm_wait(int id) {

}

#  endif /* STM_MCATS */


_CALLCONV sigjmp_buf *
stm_start_tx(stm_tx_t *tx, stm_tx_attr_t attr)
{
  return int_stm_start(tx, attr);
}

/*
 * Called by the CURRENT thread to commit a transaction.
 */
_CALLCONV int
stm_commit(void)
{
	TX_GET;
	int ret;

	ret=int_stm_commit(tx);
#ifdef STM_MCATS
	tx->committed_transactions++;
	//if (tx->CAS_executed) {
		ATOMIC_FETCH_DEC_FULL(&running_transactions);
		stm_tx_t *transaction=_tinystm.threads;
		int i;
		for (i=1;i< max_concurrent_threads-1;i++){
			if(transaction==NULL)
				return ret;
			if(transaction->i_am_waiting==1 && !transaction->scaled){
				transaction->i_am_waiting=0;
				//find transaction to unscale
				for (i=1;i< max_concurrent_threads-1;i++){
					if(transaction==NULL)
						return ret;
					if(transaction->scaled==1){
						transaction->scaled=0;
						return ret;
					}
					transaction=transaction->next;
				}

				return ret;
			}
			transaction=transaction->next;
		}


		for (i=1;i< max_concurrent_threads-1;i++){
			if(transaction==NULL)
				return ret;
			if(transaction->i_am_waiting==1){
				transaction->i_am_waiting=0;
				return ret;
			}
			transaction=transaction->next;
		}
	//}
#endif


  return ret;
}

_CALLCONV int
stm_commit_tx(stm_tx_t *tx)
{
  return int_stm_commit(tx);
}

/*
 * Called by the CURRENT thread to abort a transaction.
 */
_CALLCONV void
stm_abort(int reason)
{
  TX_GET;
  stm_rollback(tx, reason | STM_ABORT_EXPLICIT);
}

_CALLCONV void
stm_abort_tx(stm_tx_t *tx, int reason)
{
  stm_rollback(tx, reason | STM_ABORT_EXPLICIT);
}

/*
 * Called by the CURRENT thread to load a word-sized value.
 */
_CALLCONV ALIGNED stm_word_t
stm_load(volatile stm_word_t *addr)
{
  TX_GET;
  return int_stm_load(tx, addr);
}

_CALLCONV stm_word_t
stm_load_tx(stm_tx_t *tx, volatile stm_word_t *addr)
{
  return int_stm_load(tx, addr);
}

/*
 * Called by the CURRENT thread to store a word-sized value.
 */
_CALLCONV ALIGNED void
stm_store(volatile stm_word_t *addr, stm_word_t value)
{
  TX_GET;
  int_stm_store(tx, addr, value);
}

_CALLCONV void
stm_store_tx(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value)
{
  int_stm_store(tx, addr, value);
}

/*
 * Called by the CURRENT thread to store part of a word-sized value.
 */
_CALLCONV ALIGNED void
stm_store2(volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  TX_GET;
  int_stm_store2(tx, addr, value, mask);
}

_CALLCONV void
stm_store2_tx(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  int_stm_store2(tx, addr, value, mask);
}

/*
 * Called by the CURRENT thread to inquire about the status of a transaction.
 */
_CALLCONV int
stm_active(void)
{
  TX_GET;
  return int_stm_active(tx);
}

_CALLCONV int
stm_active_tx(stm_tx_t *tx)
{
  return int_stm_active(tx);
}

/*
 * Called by the CURRENT thread to inquire about the status of a transaction.
 */
_CALLCONV int
stm_aborted(void)
{
  TX_GET;
  return int_stm_aborted(tx);
}

_CALLCONV int
stm_aborted_tx(stm_tx_t *tx)
{
  return int_stm_aborted(tx);
}

/*
 * Called by the CURRENT thread to inquire about the status of a transaction.
 */
_CALLCONV int
stm_irrevocable(void)
{
  TX_GET;
  return int_stm_irrevocable(tx);
}

_CALLCONV int
stm_irrevocable_tx(stm_tx_t *tx)
{
  return int_stm_irrevocable(tx);
}

/*
 * Called by the CURRENT thread to obtain an environment for setjmp/longjmp.
 */
_CALLCONV sigjmp_buf *
stm_get_env(void)
{
  TX_GET;
  return int_stm_get_env(tx);
}

_CALLCONV sigjmp_buf *
stm_get_env_tx(stm_tx_t *tx)
{
  return int_stm_get_env(tx);
}

/*
 * Get transaction attributes.
 */
_CALLCONV stm_tx_attr_t
stm_get_attributes(void)
{
  TX_GET;
  assert (tx != NULL);
  return tx->attr;
}

/*
 * Get transaction attributes from a specifc transaction.
 */
_CALLCONV stm_tx_attr_t
stm_get_attributes_tx(struct stm_tx *tx)
{
  assert (tx != NULL);
  return tx->attr;
}

/*
 * Return statistics about a thread/transaction.
 */
_CALLCONV int
stm_get_stats(const char *name, void *val)
{
  TX_GET;
  return int_stm_get_stats(tx, name, val);
}

_CALLCONV int
stm_get_stats_tx(stm_tx_t *tx, const char *name, void *val)
{
  return int_stm_get_stats(tx, name, val);
}

/*
 * Return STM parameters.
 */
_CALLCONV int
stm_get_parameter(const char *name, void *val)
{
  if (strcmp("contention_manager", name) == 0) {
    *(const char **)val = cm_names[CM];
    return 1;
  }
  if (strcmp("design", name) == 0) {
    *(const char **)val = design_names[DESIGN];
    return 1;
  }
  if (strcmp("initial_rw_set_size", name) == 0) {
    *(int *)val = RW_SET_SIZE;
    return 1;
  }
#if CM == CM_BACKOFF
  if (strcmp("min_backoff", name) == 0) {
    *(unsigned long *)val = MIN_BACKOFF;
    return 1;
  }
  if (strcmp("max_backoff", name) == 0) {
    *(unsigned long *)val = MAX_BACKOFF;
    return 1;
  }
#endif /* CM == CM_BACKOFF */
#if CM == CM_MODULAR
  if (strcmp("vr_threshold", name) == 0) {
    *(int *)val = _tinystm.vr_threshold;
    return 1;
  }
#endif /* CM == CM_MODULAR */
#ifdef COMPILE_FLAGS
  if (strcmp("compile_flags", name) == 0) {
    *(const char **)val = XSTR(COMPILE_FLAGS);
    return 1;
  }
#endif /* COMPILE_FLAGS */
  return 0;
}

/*
 * Set STM parameters.
 */
_CALLCONV int
stm_set_parameter(const char *name, void *val)
{
#if CM == CM_MODULAR
  int i;

  if (strcmp("cm_policy", name) == 0) {
    for (i = 0; cms[i].name != NULL; i++) {
      if (strcasecmp(cms[i].name, (const char *)val) == 0) {
        _tinystm.contention_manager = cms[i].f;
        return 1;
      }
    }
    return 0;
  }
  if (strcmp("cm_function", name) == 0) {
    _tinystm.contention_manager = (int (*)(stm_tx_t *, stm_tx_t *, int))val;
    return 1;
  }
  if (strcmp("vr_threshold", name) == 0) {
    _tinystm.vr_threshold = *(int *)val;
    return 1;
  }
#endif /* CM == CM_MODULAR */
  return 0;
}

/*
 * Create transaction-specific data (return -1 on error).
 */
_CALLCONV int
stm_create_specific(void)
{
  if (_tinystm.nb_specific >= MAX_SPECIFIC) {
    fprintf(stderr, "Error: maximum number of specific slots reached\n");
    return -1;
  }
  return _tinystm.nb_specific++;
}

/*
 * Store transaction-specific data.
 */
_CALLCONV void
stm_set_specific(int key, void *data)
{
  int_stm_set_specific(key, data);
}

/*
 * Fetch transaction-specific data.
 */
_CALLCONV void *
stm_get_specific(int key)
{
  return int_stm_get_specific(key);
}

/*
 * Register callbacks for an external module (must be called before creating transactions).
 */
_CALLCONV int
stm_register(void (*on_thread_init)(void *arg),
             void (*on_thread_exit)(void *arg),
             void (*on_start)(void *arg),
             void (*on_precommit)(void *arg),
             void (*on_commit)(void *arg),
             void (*on_abort)(void *arg),
             void *arg)
{
  if ((on_thread_init != NULL && _tinystm.nb_init_cb >= MAX_CB) ||
      (on_thread_exit != NULL && _tinystm.nb_exit_cb >= MAX_CB) ||
      (on_start != NULL && _tinystm.nb_start_cb >= MAX_CB) ||
      (on_precommit != NULL && _tinystm.nb_precommit_cb >= MAX_CB) ||
      (on_commit != NULL && _tinystm.nb_commit_cb >= MAX_CB) ||
      (on_abort != NULL && _tinystm.nb_abort_cb >= MAX_CB)) {
    fprintf(stderr, "Error: maximum number of modules reached\n");
    return 0;
  }
  /* New callback */
  if (on_thread_init != NULL) {
    _tinystm.init_cb[_tinystm.nb_init_cb].f = on_thread_init;
    _tinystm.init_cb[_tinystm.nb_init_cb++].arg = arg;
  }
  /* Delete callback */
  if (on_thread_exit != NULL) {
    _tinystm.exit_cb[_tinystm.nb_exit_cb].f = on_thread_exit;
    _tinystm.exit_cb[_tinystm.nb_exit_cb++].arg = arg;
  }
  /* Start callback */
  if (on_start != NULL) {
    _tinystm.start_cb[_tinystm.nb_start_cb].f = on_start;
    _tinystm.start_cb[_tinystm.nb_start_cb++].arg = arg;
  }
  /* Pre-commit callback */
  if (on_precommit != NULL) {
    _tinystm.precommit_cb[_tinystm.nb_precommit_cb].f = on_precommit;
    _tinystm.precommit_cb[_tinystm.nb_precommit_cb++].arg = arg;
  }
  /* Commit callback */
  if (on_commit != NULL) {
    _tinystm.commit_cb[_tinystm.nb_commit_cb].f = on_commit;
    _tinystm.commit_cb[_tinystm.nb_commit_cb++].arg = arg;
  }
  /* Abort callback */
  if (on_abort != NULL) {
    _tinystm.abort_cb[_tinystm.nb_abort_cb].f = on_abort;
    _tinystm.abort_cb[_tinystm.nb_abort_cb++].arg = arg;
  }

  return 1;
}

/*
 * Called by the CURRENT thread to load a word-sized value in a unit transaction.
 */
_CALLCONV stm_word_t
stm_unit_load(volatile stm_word_t *addr, stm_word_t *timestamp)
{
#ifdef UNIT_TX
  volatile stm_word_t *lock;
  stm_word_t l, l2, value;

  PRINT_DEBUG2("==> stm_unit_load(a=%p)\n", addr);

  /* Get reference to lock */
  lock = GET_LOCK(addr);

  /* Read lock, value, lock */
 restart:
  l = ATOMIC_LOAD_ACQ(lock);
 restart_no_load:
  if (LOCK_GET_OWNED(l)) {
    /* Locked: wait until lock is free */
#ifdef WAIT_YIELD
    sched_yield();
#endif /* WAIT_YIELD */
    goto restart;
  }
  /* Not locked */
  value = ATOMIC_LOAD_ACQ(addr);
  l2 = ATOMIC_LOAD_ACQ(lock);
  if (l != l2) {
    l = l2;
    goto restart_no_load;
  }

  if (timestamp != NULL)
    *timestamp = LOCK_GET_TIMESTAMP(l);

  return value;
#else /* ! UNIT_TX */
  fprintf(stderr, "Unit transaction is not enabled\n");
  exit(-1);
  return 1;
#endif /* ! UNIT_TX */
}

/*
 * Store a word-sized value in a unit transaction.
 */
static INLINE int
stm_unit_write(volatile stm_word_t *addr, stm_word_t value, stm_word_t mask, stm_word_t *timestamp)
{
#ifdef UNIT_TX
  volatile stm_word_t *lock;
  stm_word_t l;

  PRINT_DEBUG2("==> stm_unit_write(a=%p,d=%p-%lu,m=0x%lx)\n",
               addr, (void *)value, (unsigned long)value, (unsigned long)mask);

  /* Get reference to lock */
  lock = GET_LOCK(addr);

  /* Try to acquire lock */
 restart:
  l = ATOMIC_LOAD_ACQ(lock);
  if (LOCK_GET_OWNED(l)) {
    /* Locked: wait until lock is free */
#ifdef WAIT_YIELD
    sched_yield();
#endif /* WAIT_YIELD */
    goto restart;
  }
  /* Not locked */
  if (timestamp != NULL && LOCK_GET_TIMESTAMP(l) > *timestamp) {
    /* Return current timestamp */
    *timestamp = LOCK_GET_TIMESTAMP(l);
    return 0;
  }
  /* TODO: would need to store thread ID to be able to kill it (for wait freedom) */
  if (ATOMIC_CAS_FULL(lock, l, LOCK_UNIT) == 0)
    goto restart;
  ATOMIC_STORE(addr, value);
  /* Update timestamp with newer value (may exceed VERSION_MAX by up to MAX_THREADS) */
  l = FETCH_INC_CLOCK + 1;
  if (timestamp != NULL)
    *timestamp = l;
  /* Make sure that lock release becomes visible */
  ATOMIC_STORE_REL(lock, LOCK_SET_TIMESTAMP(l));
  if (unlikely(l >= VERSION_MAX)) {
    /* Block all transactions and reset clock (current thread is not in active transaction) */
    stm_quiesce_barrier(NULL, rollover_clock, NULL);
  }
  return 1;
#else /* ! UNIT_TX */
  fprintf(stderr, "Unit transaction is not enabled\n");
  exit(-1);
  return 1;
#endif /* ! UNIT_TX */
}

/*
 * Called by the CURRENT thread to store a word-sized value in a unit transaction.
 */
_CALLCONV int
stm_unit_store(volatile stm_word_t *addr, stm_word_t value, stm_word_t *timestamp)
{
  return stm_unit_write(addr, value, ~(stm_word_t)0, timestamp);
}

/*
 * Called by the CURRENT thread to store part of a word-sized value in a unit transaction.
 */
_CALLCONV int
stm_unit_store2(volatile stm_word_t *addr, stm_word_t value, stm_word_t mask, stm_word_t *timestamp)
{
  return stm_unit_write(addr, value, mask, timestamp);
}

/*
 * Enable or disable extensions and set upper bound on snapshot.
 */
static INLINE void
int_stm_set_extension(stm_tx_t *tx, int enable, stm_word_t *timestamp)
{
#ifdef UNIT_TX
  tx->attr.no_extend = !enable;
  if (timestamp != NULL && *timestamp < tx->end)
    tx->end = *timestamp;
#else /* ! UNIT_TX */
  fprintf(stderr, "Unit transaction is not enabled\n");
  exit(-1);
#endif /* ! UNIT_TX */
}

_CALLCONV void
stm_set_extension(int enable, stm_word_t *timestamp)
{
  TX_GET;
  int_stm_set_extension(tx, enable, timestamp);
}

_CALLCONV void
stm_set_extension_tx(stm_tx_t *tx, int enable, stm_word_t *timestamp)
{
  int_stm_set_extension(tx, enable, timestamp);
}

/*
 * Get curent value of global clock.
 */
_CALLCONV stm_word_t
stm_get_clock(void)
{
  return GET_CLOCK;
}

/*
 * Get current transaction descriptor.
 */
_CALLCONV stm_tx_t *
stm_current_tx(void)
{
  return tls_get_tx();
}

/* ################################################################### *
 * UNDOCUMENTED STM FUNCTIONS (USE WITH CARE!)
 * ################################################################### */

#ifdef CONFLICT_TRACKING
/*
 * Get thread identifier of other transaction.
 */
_CALLCONV int
stm_get_thread_id(stm_tx_t *tx, pthread_t *id)
{
  *id = tx->thread_id;
  return 1;
}

/*
 * Set global conflict callback.
 */
_CALLCONV int
stm_set_conflict_cb(void (*on_conflict)(int id1, int id2))
{
  _tinystm.conflict_cb = on_conflict;
  return 1;
}
#endif /* CONFLICT_TRACKING */

/*
 * Set the CURRENT transaction as irrevocable.
 */
static INLINE int
int_stm_set_irrevocable(stm_tx_t *tx, int serial)
{
#ifdef IRREVOCABLE_ENABLED
# if CM == CM_MODULAR
  stm_word_t t;
# endif /* CM == CM_MODULAR */

  if (!IS_ACTIVE(tx->status) && serial != -1) {
    /* Request irrevocability outside of a transaction or in abort handler (for next execution) */
    tx->irrevocable = 1 + (serial ? 0x08 : 0);
    return 0;
  }

  /* Are we already in irrevocable mode? */
  if ((tx->irrevocable & 0x07) == 3) {
    return 1;
  }

  if (tx->irrevocable == 0) {
    /* Acquire irrevocability for the first time */
    tx->irrevocable = 1 + (serial ? 0x08 : 0);
    /* Try acquiring global lock */
    if (_tinystm.irrevocable == 1 || ATOMIC_CAS_FULL(&_tinystm.irrevocable, 0, 1) == 0) {
      /* Transaction will acquire irrevocability after rollback */
      stm_rollback(tx, STM_ABORT_IRREVOCABLE);
      return 0;
    }
    /* Success: remember we have the lock */
    tx->irrevocable++;
    /* Try validating transaction */
#if DESIGN == WRITE_BACK_ETL
    if (!stm_wbetl_validate(tx,0)) {
      stm_rollback(tx, STM_ABORT_VALIDATE);
      return 0;
    }
#elif DESIGN == WRITE_BACK_CTL
    if (!stm_wbctl_validate(tx)) {
      stm_rollback(tx, STM_ABORT_VALIDATE);
      return 0;
    }
#elif DESIGN == WRITE_THROUGH
    if (!stm_wt_validate(tx)) {
      stm_rollback(tx, STM_ABORT_VALIDATE);
      return 0;
    }
#elif DESIGN == MODULAR
    if ((tx->attr.id == WRITE_BACK_CTL && stm_wbctl_validate(tx))
       || (tx->attr.id == WRITE_THROUGH && stm_wt_validate(tx))
       || (tx->attr.id != WRITE_BACK_CTL && tx->attr.id != WRITE_THROUGH && stm_wbetl_validate(tx))) {
      stm_rollback(tx, STM_ABORT_VALIDATE);
      return 0;
    }
#endif /* DESIGN == MODULAR */

# if CM == CM_MODULAR
   /* We might still abort if we cannot set status (e.g., we are being killed) */
    t = tx->status;
    if (GET_STATUS(t) != TX_ACTIVE || ATOMIC_CAS_FULL(&tx->status, t, t + (TX_IRREVOCABLE - TX_ACTIVE)) == 0) {
      stm_rollback(tx, STM_ABORT_KILLED);
      return 0;
    }
# endif /* CM == CM_MODULAR */
    if (serial && tx->w_set.nb_entries != 0) {
      /* TODO: or commit the transaction when we have the irrevocability. */
      /* Don't mix transactional and direct accesses => restart with direct accesses */
      stm_rollback(tx, STM_ABORT_IRREVOCABLE);
      return 0;
    }
  } else if ((tx->irrevocable & 0x07) == 1) {
    /* Acquire irrevocability after restart (no need to validate) */
    while (_tinystm.irrevocable == 1 || ATOMIC_CAS_FULL(&_tinystm.irrevocable, 0, 1) == 0)
      ;
    /* Success: remember we have the lock */
    tx->irrevocable++;
  }
  assert((tx->irrevocable & 0x07) == 2);

  /* Are we in serial irrevocable mode? */
  if ((tx->irrevocable & 0x08) != 0) {
    /* Stop all other threads */
    if (stm_quiesce(tx, 1) != 0) {
      /* Another thread is quiescing and we are active (trying to acquire irrevocability) */
      assert(serial != -1);
      stm_rollback(tx, STM_ABORT_IRREVOCABLE);
      return 0;
    }
  }

  /* We are in irrevocable mode */
  tx->irrevocable++;

#else /* ! IRREVOCABLE_ENABLED */
  fprintf(stderr, "Irrevocability is not supported in this configuration\n");
  exit(-1);
#endif /* ! IRREVOCABLE_ENABLED */
  return 1;
}

_CALLCONV NOINLINE int
stm_set_irrevocable(int serial)
{
  TX_GET;
  return int_stm_set_irrevocable(tx, serial);
}

_CALLCONV NOINLINE int
stm_set_irrevocable_tx(stm_tx_t *tx, int serial)
{
  return int_stm_set_irrevocable(tx, serial);
}

/*
 * Increment the value of global clock (Only for TinySTM developers).
 */
void
stm_inc_clock(void)
{
  FETCH_INC_CLOCK;
}


