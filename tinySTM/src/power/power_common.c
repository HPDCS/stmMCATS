#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "power/common.h"

#ifdef USE_NLS
/***************************************************************************
 * Init National Language Support
 ***************************************************************************/
void init_nls(void)
{
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	setlocale(LC_TIME, "");
	setlocale(LC_NUMERIC, "");

	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
}
#endif

/***************************************************************************
 * Generic function to subtract two counters taking into
 * account the possibility of overflow of a 32-bit kernel-counter.
 *
 * IN:
 * @newval	New value
 * @oldval  Old value
 ***************************************************************************/
u32 subcount(u32 newval, u32 oldval)
{
	if (newval >= oldval)
		return newval - oldval;
	else
		return (MAXU32VAL- oldval) + newval;
}

u64 u64_subcount(u64 newval, u64 oldval)
{
	if (newval >= oldval)
		return newval - oldval;
	else
		return (MAXU64VAL - oldval) + newval;
}

/***************************************************************************
 * Timestamp management.
 ***************************************************************************/
stm_time_t init_timestamp;
inline stm_time_t get_time() {
	return STM_TIMER_READ();

	/*stm_time_t timestamp;
	struct timespec tms;

	if (clock_gettime(CLOCK_MONOTONIC, &tms)) {
		LOG(stderr, "Cannot get timestamp: %s\n", strerror(errno));
		exit(3);
	}
	timestamp = tms.tv_sec * 1000000000;
	timestamp += tms.tv_nsec;

	return timestamp - init_timestamp;*/
}
