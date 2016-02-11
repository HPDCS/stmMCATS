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

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

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
	volatile int queued_transactions;
	volatile stm_word_t max_allowed_running_transactions;
	unsigned long max_concurrent_threads;
	int transactions_per_tuning_cycle;
	double average_spin_time_per_waiting_transacton;
	int main_thread;
	int current_collector_thread;
	stm_time_t last_tuning_time;
	stm_time_t *wasted_time_k;
	stm_time_t *useful_time_k;
	long * conflicts_per_active_transactions;
	long * committed_per_active_transactions;

	long busy_waiting_time_threashold;

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

#ifdef STM_MCATS

inline void update_conflict_table(int id1, int id2) {
		TX_GET;
		tx->aborted_transactions++;
		tx->last_k=running_transactions;
		tx->total_conflict_per_active_transactions[tx->last_k]++;
}

void reset_local_stats(stm_tx_t *tx){
	  tx->total_useful_time=0;
	  tx->total_no_tx_time=0;
	  tx->total_wasted_time=0;
	  tx->total_spin_time=0;
	  tx->start_no_tx_time=0;
	  tx->committed_transactions_as_a_collector_thread=0;
	  tx->committed_transactions=0;
	  tx->aborted_transactions=0;
	  tx->queued_transactions=0;
	  tx->sleepy_transactions=0;
	  memset(tx->total_tx_wasted_per_active_transactions,0,(max_concurrent_threads+1)*sizeof(stm_time_t));
	  memset(tx->total_tx_committed_per_active_transactions,0,(max_concurrent_threads+1)*sizeof(long));
	  memset(tx->total_conflict_per_active_transactions,0,(max_concurrent_threads+1)*sizeof(long));
	  memset(tx->total_tx_useful_per_active_transactions,0,(max_concurrent_threads+1)*sizeof(stm_time_t));
}
#endif /* ! STM_MCATS */

/* ################################################################### *
 * STM FUNCTIONS
 * ################################################################### */

/*
 * Called once (from main) to initialize STM infrastructure.
 */
#  ifdef STM_MCATS

void stm_init(int threads) {

	int max_transactions_per_tuning_cycle;

	max_concurrent_threads = threads;

	FILE* fid;
	if ((fid = fopen("mcats_conf.txt", "r")) == NULL) {
		printf("\nError opening MCATS configuration file.\n");
		exit(1);
	}

	if (fscanf(fid, "TX_PER_CYCLE=%d INITIAL_MAX_ADMITTED_TX=%d BUSY_WAITING_TIME_THRESHOLD=%d", &max_transactions_per_tuning_cycle, &max_allowed_running_transactions, &busy_waiting_time_threashold)!=3) {
		printf("\nThe number of input parameters of the MCATS configuration file does not match the number of required parameters.\n");
		exit(1);
	}

	transactions_per_tuning_cycle = max_transactions_per_tuning_cycle / max_concurrent_threads;
	main_thread = current_collector_thread = 0;
	running_transactions = 0;
	queued_transactions=0;
	average_spin_time_per_waiting_transacton=0;
	wasted_time_k=(stm_time_t *)malloc((max_concurrent_threads+1)*sizeof(stm_time_t));
	useful_time_k=(stm_time_t *)malloc((max_concurrent_threads+1)*sizeof(stm_time_t));
	conflicts_per_active_transactions=(long *)malloc((max_concurrent_threads + 1) * sizeof(long));
	committed_per_active_transactions=(long *)malloc((max_concurrent_threads + 1) * sizeof(long));

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
_CALLCONV stm_tx_t *stm_pre_init_thread(int id){
	stm_tx_t *tx;
	tx=stm_init_thread();
	tx->total_tx_wasted_per_active_transactions=(stm_time_t*)malloc((max_concurrent_threads+1)*sizeof(stm_time_t));
	tx->total_tx_committed_per_active_transactions=(long*)malloc((max_concurrent_threads+1)*sizeof(long));
	tx->total_tx_useful_per_active_transactions = (stm_time_t *)malloc((max_concurrent_threads +1) * sizeof(stm_time_t));
	tx->total_conflict_per_active_transactions=(long*)malloc((max_concurrent_threads+1)*sizeof(long));
	tx->thread_identifier=id;
	tx->i_am_the_collector_thread=0;
	tx->i_am_waiting=0;
	reset_local_stats(tx);
	if(id==main_thread){
		current_collector_thread=main_thread;
		tx->start_no_tx_time=STM_TIMER_READ();
		tx->i_am_the_collector_thread=1;
	}

	return tx;
}

inline void stm_wait(int id) {

	TX_GET;

	int active_txs, max_txs;
	int entered=0;
	stm_time_t start_spin_time=0;

	while(1){
		active_txs=running_transactions;
		max_txs=max_allowed_running_transactions;
		if(active_txs<max_txs){
			if (ATOMIC_CAS_FULL(&running_transactions, active_txs, active_txs+1) != 0){
				if(tx->i_am_the_collector_thread==1){
					tx->first_start_tx_time=tx->last_start_tx_time=STM_TIMER_READ();
					tx->total_no_tx_time+=tx->last_start_tx_time - tx->start_no_tx_time;
				}
				entered=1;
				break;
			}
		} else {
			break;
		}
	}


	if(entered==0){
		ATOMIC_FETCH_INC_FULL(&queued_transactions);
		if(tx->i_am_the_collector_thread==1){
			//collect statistics
			start_spin_time=STM_TIMER_READ();
			tx->total_no_tx_time+=start_spin_time - tx->start_no_tx_time;
			tx->queued_transactions++;
		}
		//busy waiting or sleeping?

		if (//(tx->i_am_the_collector_thread!=1) &&
				((float)(queued_transactions-1)>(float)busy_waiting_time_threashold)) {
			//sleeping
			//stm_time_t start, end;
			//start = STM_TIMER_READ();
			tx->sleepy_transactions++;
			usleep(1000);
			//end = STM_TIMER_READ();
			/*
			printf("\nQueued_transactions-1: %i, Average spin time per waiting transaction %f, product %f, thread slept for ticks=%llu",
					queued_transactions-1,
					(double)average_spin_time_per_waiting_transacton,
					(double)(queued_transactions-1) * (double)average_spin_time_per_waiting_transacton,
					end-start);
			fflush(stdout);
			*/

		} else {
			//printf("\nThread %i no slept", id);
			//fflush(stdout);
		}
		// starting busy waiting

		int cycle=500000,i=1;
		while(1){
			active_txs=running_transactions;
			max_txs=max_allowed_running_transactions;
			if(active_txs<max_txs)
				if (ATOMIC_CAS_FULL(&running_transactions, active_txs, active_txs+1) != 0) break;
			tx->i_am_waiting=1;
			for(i=0;i<cycle;i++){
				if(tx->i_am_waiting==0)break;
			}
			tx->i_am_waiting=0;
		}

		ATOMIC_FETCH_DEC_FULL(&queued_transactions);

		if (tx->i_am_the_collector_thread==1){
			tx->first_start_tx_time=tx->last_start_tx_time=STM_TIMER_READ();
			tx->total_spin_time+=tx->first_start_tx_time-start_spin_time;
		}
	}
	if (tx->i_am_the_collector_thread==1){
		tx->start_no_tx_time=0;
	}
}

float get_throughput(float lambda, float *mu, int m) {
	int N=max_concurrent_threads;
	float *c=(float*)malloc(sizeof(float)*N+1);
	float *p=(float*)malloc(sizeof(float)*N+1);
	float th=0;
	int k;
	//a_k
	c[0]=1; //auxiliar
	float a=0.0,b=0.0;
	for (k=1;k<=N;k++){
			if(k<=m){
				c[k]= c[k-1] * (lambda*((float)N-k+1)/(k * mu[k]));
				a+=c[k];
			}else{
				c[k]=c[k-1] * (lambda*((float)N-k+1)/(m * mu[m]));
				b+=c[k];
			}
	}
	//printf("\na: %f, b: %f",a,b);
	p[0]=1/(1+a+b);
	for (k=1;k<=N;k++){
		p[k]=p[0]*c[k];
	}

	//th
	for (k=1;k<=m;k++){
		th+=p[k]*k*mu[k];
		//printf("\np[%i] %f - c[%i] %f", k, p[k], k, mu[k]);
	}
	for (k=m+1;k<=N;k++){
		th+=p[k]*m*mu[m];
		//printf("\np[%i] %f - c[%i] %f", k, p[k], m, mu[m]);
	}

	return th;
}


inline void stm_tune_scheduler(){
	TX_GET;
	int m=max_allowed_running_transactions;
	stm_time_t now=STM_TIMER_READ();
	stm_time_t total_tx_wasted_time=0;
	stm_time_t total_tx_time=0;
	stm_time_t total_no_tx_time=0;
	stm_time_t total_tx_spin_time=0;
	memset(conflicts_per_active_transactions, 0, (max_concurrent_threads+1) * sizeof(long));
	memset(committed_per_active_transactions, 0, (max_concurrent_threads+1) * sizeof(long));
	memset(wasted_time_k, 0, (max_concurrent_threads+1) * sizeof(stm_time_t));
	memset(useful_time_k, 0, (max_concurrent_threads+1) * sizeof(stm_time_t));
	long total_committed_transactions_by_collector_threads=0;
	long total_committed_transactions=0;
	long total_aborted_transactions=0;
	long total_queued_transactions=0;
	long_total_sleepy_transactions=0;
	float average_running_transactions=0;

	tx->total_no_tx_time+=now - tx->start_no_tx_time ;
	stm_tx_t *thread=_tinystm.threads;
	int i=0;
	while(thread!=NULL){
		total_tx_time+=thread->total_useful_time;
		total_no_tx_time+=thread->total_no_tx_time;
		total_tx_wasted_time+=thread->total_wasted_time;
		total_tx_spin_time+=thread->total_spin_time;
		total_committed_transactions_by_collector_threads+=thread->committed_transactions_as_a_collector_thread;
		total_committed_transactions+=thread->committed_transactions;
		total_aborted_transactions+=thread->aborted_transactions;
		total_queued_transactions+=thread->queued_transactions;
		total_sleepy_transactions+=thread->sleepy_transactions;

		for(i=0;i<max_concurrent_threads+1;i++){
			wasted_time_k[i]+=thread->total_tx_wasted_per_active_transactions[i];
			//printf("\nwasted_time_k[%i] %llu", i, thread->total_tx_wasted_per_active_transactions[i]);
			useful_time_k[i]+=thread->total_tx_useful_per_active_transactions[i];
			committed_per_active_transactions[i]+=thread->total_tx_committed_per_active_transactions[i];
			average_running_transactions+=(float)i * (float) thread->total_tx_committed_per_active_transactions[i];
			conflicts_per_active_transactions[i]+=thread->total_conflict_per_active_transactions[i];
		}
		reset_local_stats(thread);
		thread=thread->next;
	}
	//for(i=0;i<max_concurrent_threads+1;i++) printf("\nwasted_time_k[%i] %llu", i, wasted_time_k[i]);
	//printf("\ntotal_tx_time %llu, total_tx_wasted_time %llu, total_no_tx_time %llu, total_committed_transactions_by_collector_threads %i", total_tx_time, total_tx_wasted_time, total_no_tx_time, total_committed_transactions_by_collector_threads);
	average_running_transactions=average_running_transactions/(float)total_committed_transactions_by_collector_threads;
	average_spin_time_per_waiting_transacton=0;
	if (total_queued_transactions>0)
		average_spin_time_per_waiting_transacton=(double)total_tx_spin_time/(double)total_queued_transactions;
	printf("\nTotal_queued_transactions: %i, sleepy_transactions: %i, total_tx_spin_time: %llu, Average_spin_time_per_waiting_transacton: %f", total_queued_transactions, total_sleepy_transactions, total_tx_spin_time, average_spin_time_per_waiting_transacton);

	/*
	float *mu_k=(float*)malloc((max_concurrent_threads+1) * sizeof(float));
	float lambda = 1.0 / (((float) total_no_tx_time/(float)1000000000)/(float) total_committed_transactions_by_collector_threads);
	for (i=0;i<max_concurrent_threads+1;i++){
		if((wasted_time_k[i]>0 || useful_time_k[i]>0) && committed_per_active_transactions[i] > 0){
			mu_k[i]= 1.0 / ((((float) wasted_time_k[i] / (float)1000000000) / (float)committed_per_active_transactions[i]) + (((float) useful_time_k[i]/(float)1000000000) / (float) committed_per_active_transactions[i]));
			//printf("\nk:%i\tmu_k: %f, %llu, %llu, %llu", i, mu_k[i], wasted_time_k[i], useful_time_k[i], committed_per_active_transactions[i]);
		}else{
			mu_k[i]= 1.0 / ((((float)total_tx_wasted_time/(float)1000000000)/(float)total_committed_transactions_by_collector_threads)+(((float)total_tx_time/(float)1000000000) / (float) total_committed_transactions_by_collector_threads));
			//printf("\nk:%i\tmu_k: %f - average", i, mu_k[i]);
		}
	}



	float th = get_throughput(lambda,mu_k,m);
	float th_minus_1=0.0,th_plus_1=0.0,th_minus_2=0.0;
	if(m>3){
		th_minus_1=get_throughput(lambda,mu_k,m-1);
		th_minus_2=get_throughput(lambda,mu_k,m-2);
	}else if(m>2)th_minus_1=get_throughput(lambda,mu_k,m-1);
	if(th_minus_2 >= th && th_minus_2 >= th_minus_1 && m>3) {
		max_allowed_running_transactions-=2;
		//printf("\nSelected th_minus_2");
	}else if(th_minus_1>=th){
		max_allowed_running_transactions--;
		//printf("\nSelected th_minus_1");
	}else if(m<max_concurrent_threads){
		float average_restarted_transactions= (float)conflicts_per_active_transactions[m]/(float)committed_per_active_transactions[m];
		float p_a_k = average_restarted_transactions /(1.0 + average_restarted_transactions);
		float p_a_1 = 1- pow(1-p_a_k,1.0/(double)(m-1));
		float average_restarted_transactions_plus_1 = ((1.0 - pow((1.0 - p_a_1),m))/ pow((1-p_a_1),m));
		float w_m=0.0,u_m=0.0;
		if(conflicts_per_active_transactions[m]>0)
			w_m=((float)wasted_time_k[m]/(float)1000000000)/(float)conflicts_per_active_transactions[m];
		else if(total_aborted_transactions>0)w_m=((float)total_tx_wasted_time/(float)1000000000)/(float)total_aborted_transactions;
		if(committed_per_active_transactions[m]>0)
			u_m = ((float)useful_time_k[m]/(float)1000000000)/(float)committed_per_active_transactions[m];
		else u_m = ((float)total_tx_time/(float)1000000000)/(float)total_committed_transactions_by_collector_threads;
		mu_k[m + 1]= 1.0/((w_m * average_restarted_transactions_plus_1) + u_m );
		th_plus_1 = get_throughput(lambda,mu_k,m + 1);
		if(th_plus_1 > th) {
			max_allowed_running_transactions++;
			//printf("\nSelected th_plus_1");
		} else {
			//printf("\nSelected th");
		}
	}//

	tx->start_no_tx_time=STM_TIMER_READ();
	//printf("\nPredicted: %f|%f|%f|%f, measured: %f, max txs: %i", th_minus_2, th_minus_1, th, th_plus_1, (float)total_committed_transactions/((float)(now-last_tuning_time)/(float)1000000000), max_allowed_running_transactions);
	//printf("\tTotal commits: %i (as a collector: %i)",total_committed_transactions, total_committed_transactions_by_collector_threads);
	//printf("\nlambda: %f mu: %f", lambda, 1.0 / ((((float)total_tx_wasted_time/(float)1000000000)/(float)total_committed_transactions_by_collector_threads)+(((float)total_tx_time/(float)1000000000) / (float) total_committed_transactions_by_collector_threads)));
	//printf("\naverage_running_transactions: %f", average_running_transactions, 1.0);
	//fflush(stdout);
	 */
	last_tuning_time=STM_TIMER_READ();

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
#ifdef STM_MCATS
	tx->last_k=running_transactions;
#endif
	ret=int_stm_commit(tx);
#ifdef STM_MCATS
	tx->committed_transactions++;
	if (tx->i_am_the_collector_thread==1 && ret==1) {
		stm_word_t active=running_transactions;
		tx->start_no_tx_time=STM_TIMER_READ();
		ATOMIC_FETCH_DEC_FULL(&running_transactions);

		stm_time_t useful = tx->start_no_tx_time - tx->last_start_tx_time;
		tx->total_wasted_time+=tx->last_start_tx_time-tx->first_start_tx_time;
		tx->committed_transactions_as_a_collector_thread++;
		tx->total_tx_useful_per_active_transactions[active]+=useful;
		tx->total_tx_committed_per_active_transactions[active]++;
		tx->total_useful_time+=useful;
		if(tx->committed_transactions_as_a_collector_thread==transactions_per_tuning_cycle){
			if(tx->thread_identifier==max_concurrent_threads - 1) stm_tune_scheduler();
			current_collector_thread =(current_collector_thread + 1)% max_concurrent_threads;
			tx->i_am_the_collector_thread=0;
		}

	}else if(current_collector_thread==tx->thread_identifier){
		tx->start_no_tx_time=STM_TIMER_READ();
		ATOMIC_FETCH_DEC_FULL(&running_transactions);
		tx->i_am_the_collector_thread=1;
	}else ATOMIC_FETCH_DEC_FULL(&running_transactions);
	stm_tx_t *transaction=_tinystm.threads;
	int i;
	for (i=1;i< max_concurrent_threads-1;i++){
		if(transaction==NULL)
			break;
		if(transaction->i_am_waiting==1){
			transaction->i_am_waiting=0;
			break;
		}
	transaction=transaction->next;
	}
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


