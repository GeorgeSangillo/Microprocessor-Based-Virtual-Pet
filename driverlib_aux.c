#include <driverlib_aux.h>
#include <gpio.h>
#include <pmap.h>
//#include <ti/devices/msp432p4xx/driverlib/debug.h>
//#include <ti/devices/msp432p4xx/driverlib/pmap.h>

void PMAP_configurePort(const uint8_t port_map_pin, const uint8_t port_mapping, uint8_t pxMAPy, uint8_t portMapReconfigure)
{
   /* ASSERT(
            (portMapReconfigure == PMAP_ENABLE_RECONFIGURATION)
                    || (portMapReconfigure == PMAP_DISABLE_RECONFIGURATION)); */

    uint_fast16_t i;
    switch(port_map_pin) {
    case GPIO_PIN0: i = 0; break;
    case GPIO_PIN1: i = 1; break;
    case GPIO_PIN2: i = 2; break;
    case GPIO_PIN3: i = 3; break;
    case GPIO_PIN4: i = 4; break;
    case GPIO_PIN5: i = 5; break;
    case GPIO_PIN6: i = 6; break;
    case GPIO_PIN7: i = 7; break;
    default:
        return;

    }
    //Get write-access to port mapping registers:
    PMAP->KEYID = PMAP_KEYID_VAL;

    //Enable/Disable reconfiguration during runtime
    PMAP->CTL = (PMAP->CTL & ~PMAP_CTL_PRECFG) | portMapReconfigure;

    HWREG8(PMAP_BASE + i + pxMAPy) = port_mapping;
}
