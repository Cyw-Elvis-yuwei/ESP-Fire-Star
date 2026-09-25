#ifndef CANBENCH_CAN_APP_H
#define CANBENCH_CAN_APP_H
#include <stdint.h>
#include <stdbool.h>
/* Inspect these symbols via SWD. Counters are not an on-wire extension. */
typedef struct {
    uint32_t rx_frames, rx_dropped, rejected_frames, tx_completed;
    uint32_t tx_busy_retries, last_hal_error, fault;
} canbench_diagnostics_t;
extern volatile canbench_diagnostics_t canbench_diag;
bool canbench_init(void);
void canbench_poll(void);
/* fault: 0=none, 1=init, 2=RX arm, 3=TX submit, 4=CAN IRQ, 5=TX timeout. */
#endif
