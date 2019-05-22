#include <buzzer_driver.h>
#include <driverlib_aux.h> // single pin port map
#include <gpio.h>
#include <pmap.h>
#include <timer_a.h>
#include <rom_map.h>

#include <stdint.h>

/* Struct to hold buzzer information */
struct buzzer_driver {
    uint32_t timer;
    Timer_A_PWMConfig buzzer_pwm_config;
};

/* Helper function to map inputs */
static uint32_t map_intensity(uint32_t intensity);
static void set_port_map(uint32_t timer, uint32_t ccr);
static uint32_t get_timer_ccr(uint32_t timer, uint32_t ccr);

/* actual "object" being used */
struct buzzer_driver _buzzer_driver_;

/* port mapping array */
uint8_t port2_mapping_buzzer[] =
{
 PM_NONE, PM_NONE, PM_NONE, PM_NONE, PM_NONE, PM_NONE, PM_NONE , PM_NONE
};


/*
 * If timer and ccr are valid, then continue setting up, otherwise exit with error
 *
 * Assumptions: buzzer is on 2.7
 *              base clock has already been configured
 */
int init_buzzer(uint32_t pd, uint32_t timer, uint32_t ccr)
{
    /* make sure timer exists */
    if ( (timer != TIMER_A0_BASE) && (timer != TIMER_A1_BASE))
        return BZ_ERR_TIMER_NOT_EXIST;

    /* make sure ccr exists */
    if ( (ccr != TIMER_A_CAPTURECOMPARE_REGISTER_0) &&
         (ccr != TIMER_A_CAPTURECOMPARE_REGISTER_1) &&
         (ccr != TIMER_A_CAPTURECOMPARE_REGISTER_2) &&
         (ccr != TIMER_A_CAPTURECOMPARE_REGISTER_3) &&
         (ccr != TIMER_A_CAPTURECOMPARE_REGISTER_4))
        return BZ_ERR_CCR_NOT_EXIST;

    /* library does not support ccr0 for timer_a1_base for generatePWM */
    if ( (timer == TIMER_A1_BASE) && (ccr == TIMER_A_CAPTURECOMPARE_REGISTER_0))
        return BZ_ERR_CCR_NOT_EXIST;

    /* configure PWM Config in _buzzer_timer_ */
    _buzzer_driver_.timer = timer;
    _buzzer_driver_.buzzer_pwm_config.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    _buzzer_driver_.buzzer_pwm_config.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    _buzzer_driver_.buzzer_pwm_config.timerPeriod = pd;
    _buzzer_driver_.buzzer_pwm_config.compareRegister = ccr;
    _buzzer_driver_.buzzer_pwm_config.compareOutputMode = TIMER_A_OUTPUTMODE_RESET_SET;
    set_duty_cycle_pct_buzzer(50);


    set_port_map(timer, ccr);

    MAP_GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2,
                   GPIO_PIN7, GPIO_PRIMARY_MODULE_FUNCTION);

    return 1;
}

/* generates the pwm for the buzzer */
void start_buzzer()
{
    Timer_A_generatePWM(_buzzer_driver_.timer, &_buzzer_driver_.buzzer_pwm_config);
}

/* Sets duty cycle to 0 */
void stop_buzzer()
{
    _buzzer_driver_.buzzer_pwm_config.dutyCycle = 0;
    Timer_A_generatePWM(_buzzer_driver_.timer, &_buzzer_driver_.buzzer_pwm_config);
}

/* sets the duty cycle in a percentage */
void set_duty_cycle_pct_buzzer(uint32_t pct)
{
    if ( pct > 100)
        pct = 100;
    _buzzer_driver_.buzzer_pwm_config.dutyCycle = (_buzzer_driver_.buzzer_pwm_config.timerPeriod / 100) * pct;
}

/* sets intensity 0 - 255 to be used like Arduino */
void set_intensity_buzzer(uint32_t intensity)
{
    _buzzer_driver_.buzzer_pwm_config.dutyCycle = map_intensity(intensity);
}

/* sets the buzzer TIMER_A_PWMConfig timer period */
/* does not generate pwm */
void set_period_buzzer(uint32_t pd)
{
    _buzzer_driver_.buzzer_pwm_config.timerPeriod = pd;

}


/* Maps range [0, 255] to [0, timer_period] so user does not have to worry about details
 * Based off of the standard mapping algorithm:
 * output = (x * in_min) * (out_max - out_min) / (in_max - in_min) + out_min
 * In this case, in_min = out_min = 0
*/
static uint32_t map_intensity(uint32_t intensity)
{
  if (intensity > 255)
    intensity = 255;
  return ( (intensity - 0) * (_buzzer_driver_.buzzer_pwm_config.timerPeriod - 0) ) / 255;
}

/* port2_mapping[7] is defined as buzzer for mkii board */
static void set_port_map(uint32_t timer, uint32_t ccr)
{
    uint32_t timer_ccr = get_timer_ccr(timer, ccr);
    port2_mapping_buzzer[7] = timer_ccr;

    /* configure port map */
    PMAP_configurePort(GPIO_PIN7, timer_ccr, PMAP_P2MAP, PMAP_DISABLE_RECONFIGURATION);

   /* MAP_PMAP_configurePorts((const uint8_t *) port2_mapping_buzzer, PMAP_P2MAP, 1,
        PMAP_DISABLE_RECONFIGURATION);*/

}
 // set_port_map}

static uint32_t get_timer_ccr(uint32_t timer, uint32_t ccr)
{
    uint32_t timer_ccr;
    if (timer == TIMER_A0_BASE) {
        switch(ccr) {
        case TIMER_A_CAPTURECOMPARE_REGISTER_0:
            timer_ccr =PM_TA0CCR0A;
            break;

        case TIMER_A_CAPTURECOMPARE_REGISTER_1:
            timer_ccr = PM_TA0CCR1A;
            break;

        case TIMER_A_CAPTURECOMPARE_REGISTER_2:
            timer_ccr = PM_TA0CCR2A;
            break;

        case TIMER_A_CAPTURECOMPARE_REGISTER_3:
            timer_ccr = PM_TA0CCR3A;
            break;

        case TIMER_A_CAPTURECOMPARE_REGISTER_4:
            timer_ccr = PM_TA0CCR4A;
            break;

        // assert in the future
        default:
            break;
        } // switch
    } //  if
    else if (timer == TIMER_A1_BASE) {
        switch(ccr) {
        case TIMER_A_CAPTURECOMPARE_REGISTER_1:
            timer_ccr = PM_TA1CCR1A;
            break;

        case TIMER_A_CAPTURECOMPARE_REGISTER_2:
            timer_ccr = PM_TA1CCR2A;
            break;

        case TIMER_A_CAPTURECOMPARE_REGISTER_3:
            timer_ccr = PM_TA1CCR3A;
            break;

        case TIMER_A_CAPTURECOMPARE_REGISTER_4:
            timer_ccr = PM_TA1CCR4A;
            break;

        // assert in the future
        default:
            break;
        } // end switch
    } // end else if
    return timer_ccr;
}
