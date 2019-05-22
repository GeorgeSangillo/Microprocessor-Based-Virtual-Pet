/*
 * Andrue Peters
 * 10/23/18
 *
 * This is designed to fix some shortcomings I had of the official driverlib
 * Features added as needed by me (or if someone actually requested it)
 */
#ifndef _DRIVERLIB_AUX_
#define _DRIVERLIB_AUX_
#include <stdint.h>
//#include <ti/devices/msp432p4xx/inc/msp.h>
//#include <ti/devices/msp432p4xx/driverlib/driverlib.h>


/* Configures only a single pin for port mapping */
/* avoids collisions with my libraries */
void PMAP_configurePort(const uint8_t port_map_pin, const uint8_t port_mapping, uint8_t pxMAPy, uint8_t portMapReconfigure);

#endif
