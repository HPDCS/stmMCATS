#ifndef INTERFACE_H_
#define INTERFACE_H_
#include "common.h"

/***************************************************************************
 *	Energy primitive API prototype	*
 ***************************************************************************/
inline void power_init(int nthreads);
inline void power_clean(void);
float get_energy();
inline void append_commit(struct tx_commit_list *tx, stm_time_t, stm_time_t, int);
float compute_power(stm_time_t start, stm_time_t end);
#endif  /* _INTERFACE_H */
