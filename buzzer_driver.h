/*
 * Andrue Peters
 * 10/22/18
 *
 * Notes: * User must configure Timer A0 or A1 themselves and pass in the period they chose for this to work.
 *
 *        * Must call start_buzzer() after every change in setting. This was done intentionally.
 */
#ifndef _BUZZER_DRIVER_H
#define _BUZZER_DRIVER_H

#include <stdint.h>
#define BZ_ERR_TIMER_NOT_EXIST (-1)
#define BZ_ERR_CCR_NOT_EXIST (-2)


int init_buzzer(uint32_t pd, uint32_t timer, uint32_t ccr);
void start_buzzer();
void stop_buzzer();
void set_duty_cycle_pct_buzzer(uint32_t pct);
void set_intensity_buzzer(uint32_t intensity);
void set_period_buzzer(uint32_t pd);
#endif
