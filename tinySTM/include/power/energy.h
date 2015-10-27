#ifndef ENERGY_H_
#define ENERGY_H_
#include <stdint.h>
#include "common.h"

/* Files */
#define STAT		 "/proc/stat"
#define SELF_STAT	 "/proc/self/stat"

/* Power Job */
#define ACQUIRING_INTERVAL	11		// expressed in ms
#define RETENTION_TIME		0.5	// expressed in s
#define RETENTION_SIZE		((int)(RETENTION_TIME * 1000 / ACQUIRING_INTERVAL) + 1) // buffer length for keeping x seconds of history
#define MAX_CPU_LEVELS		16
#define ELABORATION_CHUNK	50

extern stm_time_t init_timestamp;

struct power_consumption {
	stm_time_t timestamp;
	stm_time_t delta;
	float power; /* expressed in W */
};
#define POWER_CONSUMPTION_SIZE (sizeof(struct power_consumption))

/* CPU Stats */
#define CPU_DIR "/sys/devices/system/cpu"
#define CPU_FREQUENCY_STAT "/sys/devices/system/cpu/%s/cpufreq/stats/time_in_state"

/* process statistic structure */
struct pid_stats {
	u64      utime; // user mode time
	u64      stime; // system mode time
};
#define PID_STATS_SIZE	(sizeof(struct pid_stats))

struct sys_info {
	stm_time_t timestamp;
	float cpupower;
	u32 itv;
	u32 runtime;
	struct pid_stats stats;
};
#define SYS_INFO_SIZE	(sizeof(struct sys_info))

/*
 * Structure for CPU statistics.
 * In activity buffer: First structure is for global CPU utilisation ("all").
 * Following structures are for each individual CPU (0, 1, etc.)
 */
struct stats_cpu {
	u64 cpu_user;
	u64 cpu_nice;
	u64 cpu_sys;
	u64 cpu_idle;
	u64 cpu_iowait;
	u64 cpu_steal;
	u64 cpu_hardirq;
	u64 cpu_softirq;
	u64 cpu_guest;
};
#define STATS_CPU_SIZE	(sizeof(struct stats_cpu))

struct cpufreqdata {
	u64 frequency;
	u64 count;
};
#define CPUFREQDATA_SIZE	(sizeof(struct cpufreqdata))

/***************************************************************************
 * Prototypes for functions used to read system statistics
 ***************************************************************************/
static inline int read_proc_pid_stat(struct pid_stats *pst);
static inline void read_stat_cpu(struct sys_info *);
static inline int check_cpufreq_stats(void);
static inline void read_cpufreq_stats(struct sys_info *);
static inline int read_pid_stats(struct pid_stats *pst);
static inline void read_process_stats(int curr);

#endif  /* _ENERGY_H */
