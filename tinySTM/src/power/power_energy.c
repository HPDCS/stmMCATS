#include "stm.h"
#include "stm_internal.h"

#ifdef POWER_CONSUMPTION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#include <errno.h>
#include <dirent.h>

#include "atomic.h"

#include "power/energy.h"
#include "power/interface.h"

/* for CPU frequency stuff */
static struct cpufreqdata freqs[MAX_CPU_LEVELS];
static struct cpufreqdata oldfreqs[MAX_CPU_LEVELS];
static struct cpufreqdata delta[MAX_CPU_LEVELS];
static float maxcpupower;	// in Watts
static float mincpupower;	// in Watts

/* for power calculation stuff */
static struct sys_info sys_stats[2];
static int cpufreq_stats;
static int curr;

/* for power transaction stuff*/
#define NEXT_RETENTION_SIZE(i) (((i)+1) % RETENTION_SIZE)
#define PREV_RETENTION_SIZE(i) (((i)+RETENTION_SIZE-1) % RETENTION_SIZE)
static int power_index;
static struct power_consumption *power_history;
static pthread_t power_job = 0;
static pthread_t tx_commit_job = 0;

void print_power_history(int start, int end) {
	int i;
	for(i=MAXIMUM(0, start); i<MINIMUM(RETENTION_SIZE, end); i++)
		LOG(stdout, "[%d] History - consumed %f @ %llu (%f)\n", i, power_history[i].power, power_history[i].timestamp, power_history[i].delta / 1000000.0f);
}

float get_energy()
{
	static unsigned long int count = 0;
	int prev = !curr;

	/* Don't buffer data if redirected to a pipe */
	//setbuf(stdout, NULL);

	memset(&sys_stats[curr], 0, SYS_INFO_SIZE);
	read_process_stats(curr);
	read_cpufreq_stats(&sys_stats[curr]);

	u32 delta_runtime = subcount(sys_stats[curr].runtime, sys_stats[prev].runtime);
	float delta_cpu = (u64_subcount(sys_stats[curr].stats.utime, sys_stats[prev].stats.utime) + u64_subcount(sys_stats[curr].stats.stime, sys_stats[prev].stats.stime)) * (100.0f / delta_runtime);

	u32 delta_itv = u64_subcount(sys_stats[curr].itv, sys_stats[prev].itv);
	stm_time_t delta_timestamp = u64_subcount(sys_stats[curr].timestamp, sys_stats[prev].timestamp);

	/* (avg cpu time) * (cpu power) * (run percent) / 100 */
	double power = delta_cpu * sys_stats[curr].cpupower * (delta_runtime * 1.0f / delta_itv) / 100.0f;
	if (power != power) // check not NaN
	{
#ifdef POWER_DEBUG
		DEBUG(stdout, "NaN result\n");
#endif
		power = 0;
	}

	power_history[power_index].power = power;
	power_history[power_index].delta = delta_timestamp;
	/* timestamp set as last element to avoid critical raise in CPUEnergy */
	power_history[power_index].timestamp = sys_stats[curr].timestamp;
#ifdef POWER_DEBUG
	DEBUG(stdout, "[%lu - %d/%d] Power %f @ %llu for %f\n", count, power_index, RETENTION_SIZE, power_history[power_index].power, power_history[power_index].timestamp, power_history[power_index].delta / 1000000.0f);
#endif
	if(count > 0 && !power) {
#ifdef POWER_ALERT
		WARNING(stdout, "Power consumption read 0 W, the acquiring interval '%f msec' is too small. Start: %llu, End: %llu, delta: %f\n", (float)ACQUIRING_INTERVAL, sys_stats[prev].timestamp, sys_stats[curr].timestamp, delta_timestamp / 1000000.0);
#endif
	} else {
		power_index = NEXT_RETENTION_SIZE(power_index);
		curr ^= 1;
	}
	count++;

	return power;
}

static void * power_acquisition_job(void *arg) {
	INIT_JOB_THREAD();

	while(1) {
		nanosleep((struct timespec[]){{0, ACQUIRING_INTERVAL * 1000000}}, NULL);
		get_energy();
	}

	return NULL;
}

static void * commit_management_job(void *arg) {
	INIT_JOB_THREAD();

	stm_tx_t *tx;
	struct tx_commit_list *l;
	struct tx_commit *entry;
	unsigned int elaboration;
	unsigned int i, ii;
	char elaborated;
	float power;
	struct power_consumption actual, before;

	while(1) {
		elaborated = 0;
		tx = _tinystm.threads;

		while(tx != NULL) {
			l = &tx->commit_list;
			elaboration = DELTA_MOD(l->write_index, l->elaborate_index, TX_LIST_SIZE) / ELABORATION_CHUNK;

			while (elaboration >= 0) {
				entry = &l->list[l->elaborate_index];
				if(
					entry->to_elaborate
					&& entry->end <= power_history[PREV_RETENTION_SIZE(power_index)].timestamp
				) {
					power = 0;
					i = NEXT_RETENTION_SIZE(entry->power_index);
					ii = entry->power_index;
					memcpy(&actual, &power_history[i], POWER_CONSUMPTION_SIZE);
					memcpy(&before, &power_history[ii], POWER_CONSUMPTION_SIZE);

					do {
						if(entry->end >= before.timestamp) {
							power += actual.power * 1000.0f / actual.delta /* unit of power in the interval */
									* u64_subcount(entry->end, MAXIMUM(entry->start, before.timestamp)) /* number of unit used by the transaction */
									/ entry->parallelism_level; /* consider a fraction based on the concurrent transactions */
						}

						i = ii;
						memcpy(&actual, &before, POWER_CONSUMPTION_SIZE);
						ii = PREV_RETENTION_SIZE(i);
						memcpy(&before, &power_history[ii], POWER_CONSUMPTION_SIZE);
					} while(
						/* With respect to the above computation i is the prev and ii is the prev prev*/
						actual.timestamp != 0 /* there exists a value */
						&& before.timestamp < actual.timestamp /* avoid looping on the circular vector */
						&& entry->start < actual.timestamp /* stop condition */
					);

#ifdef POWER_ALERT
					if (entry->start < actual.timestamp) { // before->timestamp > actual->timestamp
						WARNING(stdout, "Start time out of range, the retention value '%f msec (%d)' is too small. Start: %llu, End: %llu, Current timestamp: %llu (%d)\n", RETENTION_TIME * 1000.0, RETENTION_SIZE, entry->start, entry->end, actual.timestamp, i);
					}
#endif


					l->tx_power_consumption[entry->parallelism_level] += power;

					entry->to_elaborate = 0;
					l->elaborate_index = NEXT_TX_COMMIT_ENTRY(l->elaborate_index);
					elaborated = 1;
				} else
					break;

				elaboration--;
			}

			tx = tx->next;
		}

		if (!elaborated) {
			/* Interval = 1/4 of ACQUIRING_INTERVAL */
			nanosleep((struct timespec[]){{0, ACQUIRING_INTERVAL * 250000}}, NULL);
			//nanosleep((struct timespec[]){{0, ACQUIRING_INTERVAL * 250}}, NULL);
		}
	}
	return NULL;
}

float compute_power(stm_time_t start, stm_time_t end) {
	unsigned int i, ii;
	float power = 0;
	struct power_consumption actual, before;

	i = PREV_RETENTION_SIZE(power_index);
	ii = PREV_RETENTION_SIZE(i);
	memcpy(&actual, &power_history[i], POWER_CONSUMPTION_SIZE);
	memcpy(&before, &power_history[ii], POWER_CONSUMPTION_SIZE);

	do {
		if(end >= before.timestamp) {
			power += actual.power * 1000.0f / actual.delta /* unit of power in the interval */
					* u64_subcount(end, MAXIMUM(start, before.timestamp)); /* number of unit used by the transaction */
		}

		i = ii;
		memcpy(&actual, &before, POWER_CONSUMPTION_SIZE);
		ii = PREV_RETENTION_SIZE(i);
		memcpy(&before, &power_history[ii], POWER_CONSUMPTION_SIZE);
	} while(
		/* With respect to the above computation i is the prev and ii is the prev prev*/
		actual.timestamp != 0 /* there exists a value */
		&& before.timestamp < actual.timestamp /* avoid looping on the circular vector */
		&& start < actual.timestamp /* stop condition */
	);

#ifdef POWER_ALERT
	if (start < actual.timestamp) { // before->timestamp > actual->timestamp
		WARNING(stdout, "Start time out of range, the retention value '%f msec (%d)' is too small. Start: %llu, End: %llu, Current timestamp: %llu (%d)\n", RETENTION_TIME * 1000.0, RETENTION_SIZE, start, end, actual.timestamp, i);
	}
#endif

	return power;
}

/*
 ***************************************************************************
 * Init system state.
 ***************************************************************************/
inline void power_init(int nthreads)
{
#ifdef POWER_INFO
	INFO(stdout, "Inizializing power subsystem...\n");
	INFO(stdout, "Buffer allocation:\n"
		"\tstm_time_t: %u,\n"
		"\tunsigned int: %u\n"
		"\tint: %u\n"
		"\tPower structure: %u x %d = %f KB\n"
		"\tCommit structure: %u x %d x %d = %f MB\n",
		sizeof(stm_time_t),
		sizeof(unsigned int),
		sizeof(int),
		POWER_CONSUMPTION_SIZE, RETENTION_SIZE, POWER_CONSUMPTION_SIZE * RETENTION_SIZE / 1024.0,
		TX_COMMIT_SIZE, TX_LIST_SIZE, nthreads, TX_COMMIT_SIZE * TX_LIST_SIZE * MAXIMUM(1, nthreads) / 1024.0 / 1024.0
	);
#endif
	curr = 0;
	power_index = 0;

	/* Default cpu frequency account is not supported.*/
	cpufreq_stats = 0;

	/* Set the cpufreq_stats flag.*/
	cpufreq_stats = check_cpufreq_stats();
	if (!cpufreq_stats) {
		LOG(stderr, "CPU stats unsupported\n");
		exit(1);
	}

	/*
	 **********************************************************************
	 * -------Specifications-------
	 * CPU: in my Vaio laptop
	 *   - 800MHZ: 18W
	 *   - 2.Ghz: 30W
	 **********************************************************************/
	maxcpupower = 30.0f;       // in Watts
	mincpupower = 18.0f;        // in Watts

	power_history = (struct power_consumption *)malloc(RETENTION_SIZE * POWER_CONSUMPTION_SIZE);
	memset(&sys_stats, 0, SYS_INFO_SIZE * 2);

	/* Initialize old measurements */
	init_timestamp = 0;
	init_timestamp = get_time();
	get_energy();
#ifdef POWER_INFO
	INFO(stdout, "Power subsystem initialized\n");
	INFO(stdout, "Starting power job...\n");
#endif

	struct sigaction sa;
	struct itimerval timer;

	if(!power_job) {
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setschedparam (&attr, ((struct sched_param[]){{ .sched_priority = 100 }}));
		pthread_create(&power_job, &attr, power_acquisition_job, NULL);
#ifdef POWER_INFO
		INFO(stdout, "Power job created: %lu\n", power_job);
#endif
	}
	//TODO transazioni non gestite più per singolo thread
	/*if(!tx_commit_job) {
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setschedparam (&attr, ((struct sched_param[]){{ .sched_priority = 80 }}));
		pthread_create(&tx_commit_job, &attr, commit_management_job, NULL);
#ifdef POWER_INFO
		INFO(stdout, "Power job created: %lu\n", tx_commit_job);
#endif
	}*/
}

/*
 ***************************************************************************
 * Free structures according to system state.
 ***************************************************************************/
inline void power_clean(void)
{
#ifdef POWER_INFO
	INFO(stdout, "Cleaning power subsystem...\n");
#endif
	if (power_job) {
		pthread_kill(power_job, SIGTERM);
		pthread_join(power_job, NULL);
#ifdef POWER_INFO
		INFO(stdout, "Power job stopped\n");
#endif
	}
	if (tx_commit_job) {
		pthread_kill(tx_commit_job, SIGTERM);
		pthread_join(tx_commit_job, NULL);
#ifdef POWER_INFO
		INFO(stdout, "Commit job stopped\n");
#endif
	}

	free(power_history);

#ifdef POWER_INFO
	INFO(stdout, "Power subsystem cleared\n");
#endif
}

inline void append_commit(struct tx_commit_list *tx, stm_time_t start, stm_time_t end, int parallelism_level) {
	static unsigned int counter = 0;
	ATOMIC_FETCH_INC_FULL(&counter);
#ifdef POWER_ALERT
	if(tx->list[tx->write_index].to_elaborate)
		WARNING(stdout, "[tid: %lu - %u] Lost a commit power computation (%u/%u), commit buffer size %d is too small\n\tCommit discarded %llu - %llu @ %d. Value: %d\n", GET_TID(), counter, tx->elaborate_index, tx->write_index, TX_LIST_SIZE, tx->list[tx->write_index].start, tx->list[tx->write_index].end, tx->list[tx->write_index].parallelism_level, tx->list[tx->write_index].to_elaborate);
	assert(!tx->list[tx->write_index].to_elaborate);
#endif

	tx->list[tx->write_index].start = start;
	tx->list[tx->write_index].end = end;
	tx->list[tx->write_index].parallelism_level = parallelism_level;
	tx->list[tx->write_index].power_index = PREV_RETENTION_SIZE(power_index);
	tx->list[tx->write_index].to_elaborate = 1;
	tx->write_index = NEXT_TX_COMMIT_ENTRY(tx->write_index);
}

/***************************************************************************
 * Read stats from /proc/#[/task/##]/stat.
 *
 * IN:
 * @pid		Process whose stats are to be read.
 * @pst		Pointer on structure where stats will be saved.
 *
 * OUT:
 * @pst		Pointer on structure where stats have been saved.
 *
 * RETURNS:
 * 0 if stats have been successfully read, and 1 otherwise.
 ***************************************************************************/
static inline int read_proc_pid_stat(struct pid_stats *pst)
{
	FILE *fp;
	char format[1024];

	if ((fp = fopen(SELF_STAT, "r")) == NULL){
		/* No such process */
		return 1;
	}

	sprintf(format, "%%*d (%%*s %%*s %%*d %%*d %%*d %%*d %%*d %%*u %%*lu %%*lu %%*lu %%*lu %%lu %%lu");
	fscanf(fp, format, &pst->utime,  &pst->stime);
	fclose(fp);

	return 0;
}

/***************************************************************************
 * Read CPU statistics and machine uptime.
 *
 * IN:
 * @st_cpu	Structure where stats will be saved.
 * @nbr		Total number of CPU (including cpu "all").
 *
 * OUT:
 * @st_cpu	Structure with statistics.
 * @uptime	Machine uptime multiplied by the number of processors.
 * @uptime0	Machine uptime. Filled only if previously set to zero.
 ***************************************************************************/
static inline void read_stat_cpu(struct sys_info * sy)
{
	FILE *fp;
	struct stats_cpu st_cpu;
	char line[1024];

	if ((fp = fopen(STAT, "r")) == NULL) {
		LOG(stderr, "Cannot open %s: %s\n", STAT, strerror(errno));
		exit(2);
	}

	memset(&st_cpu, 0, STATS_CPU_SIZE);
	if (fgets(line, 1024, fp) != NULL) {
		sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu %llu %llu",
			       &st_cpu.cpu_user,
			       &st_cpu.cpu_nice,
			       &st_cpu.cpu_sys,
			       &st_cpu.cpu_idle,
			       &st_cpu.cpu_iowait,
			       &st_cpu.cpu_hardirq,
			       &st_cpu.cpu_softirq,
			       &st_cpu.cpu_steal,
			       &st_cpu.cpu_guest);

			sy->itv = st_cpu.cpu_user + st_cpu.cpu_nice    +
				st_cpu.cpu_sys    + st_cpu.cpu_idle    +
				st_cpu.cpu_iowait + st_cpu.cpu_hardirq +
				st_cpu.cpu_steal  + st_cpu.cpu_softirq;

			sy->runtime = st_cpu.cpu_user + st_cpu.cpu_sys - st_cpu.cpu_guest;
	}
	fclose(fp);
}

/*****************************************************************
 * Check if the system supports cpu freq accounting
 * RETURNS:
 * 0: Not supported.
 * 1: Supported.
 *****************************************************************/
static inline int check_cpufreq_stats(void) {
	FILE *file;
	int cpufreq_stats;
	file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/stats/time_in_state", "r");
	if (!file)
		cpufreq_stats = 0;	// not support
	else{
		cpufreq_stats = 1;	// support
		fclose(file);
	}

	return cpufreq_stats;
}

/****************************************************************
 * Calculate cpu freq stat, and relevant power consumption.
 ****************************************************************/
static inline void read_cpufreq_stats(struct sys_info *info)
{
	info->timestamp = get_time();
	info->cpupower = 0.0f;
	if (!cpufreq_stats) {
		LOG(stderr, "CPU stats unsupported\n");
		info->cpupower = 0;
		return;
	}
	else {
		DIR *dir;
		struct dirent *dirent;
		FILE *file;
		char filename[PATH_MAX];
		char line[32];
		float rangecpupower = maxcpupower - mincpupower;
		float delta_power;
		int ret = 0;
		int maxfreq = 0;
		u64 total_time = 0;

		memcpy(&oldfreqs, &freqs, sizeof(freqs));

		for (ret = 0; ret<MAX_CPU_LEVELS; ret++)
			freqs[ret].count = 0;

		dir = opendir(CPU_DIR);
		if (!dir)
			return;

		int i;
		while ((dirent = readdir(dir))) {
			if (strncmp(dirent->d_name,"cpu", 3) != 0)
				continue;

			sprintf(filename, CPU_FREQUENCY_STAT, dirent->d_name);

			file = fopen(filename, "r");
			if (!file)
				continue;

			memset(line, 0, 32);
			i = 0;
			while (!feof(file)) {
				if(fgets(line, 32,file) == NULL)
					break;

				sscanf(line, "%llu %llu", &freqs[i].frequency, &freqs[i].count);

				i++;
				if (i>=MAX_CPU_LEVELS)
					break;
			}

			maxfreq = i - 1;
			fclose(file);
		}

		closedir(dir);
		//count unit 10ms, usertime
		for (ret = 0; ret < MAX_CPU_LEVELS; ret++) {
			delta[ret].count = freqs[ret].count - oldfreqs[ret].count;
			total_time += delta[ret].count;
			delta[ret].frequency = freqs[ret].frequency;
		}

		delta_power = 0.0f;
		for (ret = 0; ret <= maxfreq; ret++) {
			if(total_time > 0 && maxfreq > 0)
				delta_power = (maxcpupower - rangecpupower * ret / maxfreq) * delta[ret].count / 100;//Watt * 10ms
			info->cpupower += delta_power;
		}
	}
}

/*
 ***************************************************************************
 * Read various stats for given PID.
 *
 * IN:
 * @pst		Pointer on structure where stats will be saved.
 *
 * OUT:
 * @pst		Pointer on structure where stats have been saved.

 *
 * RETURNS:
 * 0 if stats have been successfully read, and 1 otherwise.
 ***************************************************************************
 */
static inline int read_pid_stats(struct pid_stats *pst)
{
	memset(pst, 0, sizeof(struct pid_stats));

	if (read_proc_pid_stat(pst))
		return 1;

	return 0;
}

/*
 ***************************************************************************
 * Read various stats the the currently active process.
 *
 * IN:
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
static inline void read_process_stats(int curr)
{
	struct pid_stats *psti;

	read_stat_cpu(&sys_stats[curr]);

	psti = &sys_stats[curr].stats;
	if (read_pid_stats(psti)) {
		LOG(stderr, "Process has terminated\n");
		exit(5);
	}
}

#endif

