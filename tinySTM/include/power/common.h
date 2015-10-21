#ifndef COMMON_H_
#define COMMON_H_
#define _GNU_SOURCE

#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <execinfo.h>

#define GET_TID() syscall(SYS_gettid)

#define ENABLE_LOGGING
#define POWER_INFO
//#define POWER_DEBUG
#define POWER_ALERT
//#define POWER_PRINT_STATS

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

/*
 ***************************************************************************
 * Various keywords and constants
 ***************************************************************************
 */

#define	MAXU32VAL	0x11111111
#define	MAXU64VAL	0x1111111111111111LL
/*
 * We define u64 as unsigned long long for every architecture
 * so that we can print it with %Lx without getting warnings.
 */
typedef unsigned long long	u64;
typedef signed long long	s64;
typedef unsigned int		u32;
typedef signed int			s32;
typedef unsigned short		u16;
typedef signed short		s16;
typedef unsigned char		u8;
typedef signed char			s8;

typedef u64	stm_time_t;

/***************************************************************************
 * Interface definitions
 ***************************************************************************/

#define TX_LIST_SIZE 102400
#define NEXT_TX_COMMIT_ENTRY(i) (((i)+1) % TX_LIST_SIZE)
#define PREV_TX_COMMIT_ENTRY(i) (((i)+TX_LIST_SIZE-1) % TX_LIST_SIZE)

struct tx_commit {
	stm_time_t start;
	stm_time_t end;
	unsigned int parallelism_level;
	unsigned int power_index;
	int to_elaborate;
};
#define TX_COMMIT_SIZE (sizeof(struct tx_commit))

struct tx_commit_list {
	float *tx_power_consumption; /* expressed in mW */
	struct tx_commit list[TX_LIST_SIZE];
	unsigned int write_index;
	unsigned int elaborate_index;
};
#define TX_COMMIT_LIST_SIZE (sizeof(struct tx_commit_list))


/***************************************************************************
 * Macro functions definitions.
 ***************************************************************************/

static void term_handler(int signum) {
	pthread_exit((void *)NULL);

	void *array[50];
	char cmd[1204], file[512], address[128];
	char format[50];
	size_t size, i;
	char **strings;

	size = backtrace(array, 50);
	strings = backtrace_symbols(array, size);

	sprintf(format, "%%s [%%s");
	for (i = 0; i < size; i++) {
		sscanf(strings[i], format, file,  address);
		*strchr(file, '(') = '\0';
		address[strlen(address) - 1] = '\0';
		sprintf(cmd, "addr2line -e %s -fp %s", file, address);
		system(cmd);
	}

	free (strings);
	pthread_exit((void *)NULL);
}

#define INIT_JOB_THREAD() ({ \
	struct sigaction sa; \
	memset(&sa, 0, sizeof(sa)); \
	sa.sa_flags = SA_RESTART; \
	sigfillset(&sa.sa_mask); \
	sa.sa_handler = &term_handler; \
	sigaction(SIGTERM, &sa, NULL); \
})

/*
 * Under very special circumstances, STDOUT may become unavailable.
 * This is what we try to guess here
 */
#define TEST_STDOUT(_fd_) ({					\
	if (write(_fd_, "", 0) == -1) {	\
			perror("stdout");	\
			exit(6);		\
	}				\
})

#define MINIMUM(a,b)	((a) < (b) ? (a) : (b))
#define MAXIMUM(a,b)	((a) > (b) ? (a) : (b))

#define DELTA_MOD(a, b, mod) ((a) < (b) ? ((a)+(mod)-(b))%(mod) : ((a)-(b))%(mod))

/* redefinition of the macro in stm.h */
#define STM_TIMER_READ() ({ \
	unsigned int lo; \
	unsigned int hi; \
	__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi)); \
	((stm_time_t)hi) << 32 | lo; \
})

#ifdef ENABLE_LOGGING
#define LOG(level, format, ...) ({ \
	int _retval; \
	/*fprintf(level, "\\/\n");*/ \
	_retval = fprintf(level, _("<%f> " format), get_time() / 1000000.0, ##__VA_ARGS__); \
	/*fprintf(level, "/\\\n");*/ \
	/*fflush(level);*/ \
	_retval; \
})
#else
#define LOG(level, format, ...) ({ \
	0; \
})
#endif
#define INFO(level, format, ...) LOG(level, "INFO - " format, ##__VA_ARGS__)
#define WARNING(level, format, ...) LOG(level, "WARN - " format, ##__VA_ARGS__)
#define DEBUG(level, format, ...) LOG(level, "DEBUG - " format, ##__VA_ARGS__)

pthread_mutex_t comm_lock_;
#define LOCK(...) ({ \
	pthread_mutex_lock(&comm_lock_); \
	__VA_ARGS__ \
	pthread_mutex_unlock(&comm_lock_); \
})

double pow(double, double);
int pthread_yield(void);


/***************************************************************************
 * Functions prototypes
 ***************************************************************************/
extern void init_nls(void);
extern u32 subcount(u32 newval, u32 oldval);
extern u64 u64_subcount(u64 newval, u64 oldval);
stm_time_t get_time();
#endif  /* _COMMON_H */
