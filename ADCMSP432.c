/*
 * Edited by Drue Peters on 11/27/18
 * Only change I made was changing the memory configuration to using ADC_VREFPOS_AVCC_VREFNEG_VSS
 */


/*
 * Copyright (c) 2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdbool.h>

#include <ti/drivers/ADC.h>
#include <ti/drivers/adc/ADCMSP432.h>
#include <ti/drivers/ports/DebugP.h>
#include <ti/drivers/ports/HwiP.h>
#include <ti/drivers/ports/SemaphoreP.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerMSP432.h>

/* driverlib header files */
#include <rom.h>
#include <rom_map.h>
#include <adc14.h>
#include <gpio.h>
#include <interrupt.h>
#include <ref_a.h>

#define ALL_INTERRUPTS  (0xFFFFFFFFFFFFFFFF)

void ADCMSP432_close(ADC_Handle handle);
int_fast16_t ADCMSP432_control(ADC_Handle handle, uint_fast16_t cmd, void *arg);
int_fast16_t ADCMSP432_convert(ADC_Handle handle, uint16_t *value);
uint32_t ADCMSP432_convertRawToMicroVolts(ADC_Handle handle,
    uint16_t rawAdcValue);
void ADCMSP432_init(ADC_Handle handle);
ADC_Handle ADCMSP432_open(ADC_Handle handle, ADC_Params *params);

/* Keep track of the amount of adc handle instances */
static uint_fast16_t adcInstance = 0;

/* Global mutex ensuring exclusive access to ADC during conversions */
static SemaphoreP_Handle globalMutex = NULL;

/* ADC function table for ADCMSP432 implementation */
const ADC_FxnTable ADCMSP432_fxnTable = {
    ADCMSP432_close,
    ADCMSP432_control,
    ADCMSP432_convert,
    ADCMSP432_convertRawToMicroVolts,
    ADCMSP432_init,
    ADCMSP432_open
};

/*
 *  ======== initHW ========
 *
 *  Configures ADC peripheral
 */
static void initHw(ADCMSP432_Object *object, ADCMSP432_HWAttrs const *hwAttrs)
{
    /* Initializing ADC (MCLK/1/1) */
    MAP_ADC14_enableModule();
    MAP_ADC14_initModule(ADC_CLOCKSOURCE_MCLK, ADC_PREDIVIDER_1,
        ADC_DIVIDER_1, 0);

    /* Set trigger source */
    MAP_ADC14_setSampleHoldTrigger(ADC_TRIGGER_ADCSC, false);

    /* Set sample/hold time */
    MAP_ADC14_setSampleHoldTime(ADC_PULSE_WIDTH_4, ADC_PULSE_WIDTH_4);

    /* No repeat mode */
    MAP_ADC14_configureSingleSampleMode(ADC_MEM0, false);

    /* Configuring Sample Timer */
    MAP_ADC14_enableSampleTimer(ADC_MANUAL_ITERATION);
}

/*
 *  ======== ADCMSP432_close ========
 */
void ADCMSP432_close(ADC_Handle handle)
{
    uintptr_t         key;
    ADCMSP432_Object *object = handle->object;

    key = HwiP_disable();

    adcInstance--;
    if (adcInstance == 0) {
        /* Disable interrupts & the ADC */
        MAP_ADC14_disableInterrupt(ALL_INTERRUPTS);
        MAP_ADC14_disableModule();

        if (globalMutex) {
            SemaphoreP_delete(globalMutex);
            globalMutex = NULL;
        }

        /* Remove power constraints */
        Power_releaseConstraint(PowerMSP432_DISALLOW_SHUTDOWN_0);
        Power_releaseConstraint(PowerMSP432_DISALLOW_SHUTDOWN_1);
    }
    object->isOpen = false;

    HwiP_restore(key);

    DebugP_log0("ADC: Object closed");
}

/*
 *  ======== ADCMSP432_control ========
 */
int_fast16_t ADCMSP432_control(ADC_Handle handle, uint_fast16_t cmd, void *arg)
{
    /* No implementation yet */
    return (ADC_STATUS_UNDEFINEDCMD);
}

/*
 *  ======== ADCMSP432_convert ========
 */
int_fast16_t ADCMSP432_convert(ADC_Handle handle, uint16_t *value)
{
    ADCMSP432_HWAttrs const *hwAttrs = handle->hwAttrs;

    /* Acquire the lock for this particular ADC handle */
    SemaphoreP_pend(globalMutex, SemaphoreP_WAIT_FOREVER);

    /*
     * Set power constraints to keep peripheral active during convert
     * and to prevent performance level changes
     */
    Power_setConstraint(PowerMSP432_DISALLOW_DEEPSLEEP_0);
    Power_setConstraint(PowerMSP432_DISALLOW_PERF_CHANGES);

    /* Set internal reference voltage */
    MAP_REF_A_setReferenceVoltage(hwAttrs->refVoltage);
    MAP_REF_A_enableReferenceVoltage();

    /* Interrupt is kept disabled since using polling mode */
    MAP_ADC14_clearInterruptFlag(ADC_INT0);

    /* Set the ADC resolution */
    MAP_ADC14_setResolution(hwAttrs->resolution);

    /* Config the ADC memory0 with specified channel */
    MAP_ADC14_configureConversionMemory(ADC_MEM0,
       ADC_VREFPOS_AVCC_VREFNEG_VSS, hwAttrs->channel, ADC_NONDIFFERENTIAL_INPUTS);

    /* Enabling/Toggling Conversion */
    MAP_ADC14_enableConversion();

    /* Trigger conversion either from ADC14SC bit or Timer PWM */
    MAP_ADC14_toggleConversionTrigger();

    /* Interrupt polling */
    while (!(MAP_ADC14_getInterruptStatus() & ADC_INT0));
    MAP_ADC14_clearInterruptFlag(ADC_INT0);

    /* Disabling Conversion */
    MAP_ADC14_disableConversion();

    /* Store the result into object */
    *value = MAP_ADC14_getResult(ADC_MEM0);

    /* Remove constraints set after ADC conversion complete */
    Power_releaseConstraint(PowerMSP432_DISALLOW_DEEPSLEEP_0);
    Power_releaseConstraint(PowerMSP432_DISALLOW_PERF_CHANGES);

    /* Release the lock for this particular ADC handle */
    SemaphoreP_post(globalMutex);

    DebugP_log0("ADC: Convert completed");

    /* Return the number of bytes transfered by the ADC */
    return (ADC_STATUS_SUCCESS);
}

/*
 *  ======== ADCMSP432_convertRawToMicroVolts ========
 */
uint32_t ADCMSP432_convertRawToMicroVolts(ADC_Handle handle,
    uint16_t rawAdcValue)
{
    uint32_t                 refMicroVolts;
    ADCMSP432_HWAttrs const *hwAttrs = handle->hwAttrs;

    switch (hwAttrs->refVoltage) {
        case REF_A_VREF1_45V:
            refMicroVolts = 1450000;
            break;

        case REF_A_VREF1_2V:
            refMicroVolts = 1200000;
            break;

        case REF_A_VREF2_5V:
        default:
            refMicroVolts = 2500000;
    }

    if (rawAdcValue == 0x3FFF) {
        return (refMicroVolts);
    }
    else if (rawAdcValue == 0) {
        return (rawAdcValue);
    }
    else {
        return (rawAdcValue * (refMicroVolts / 0x4000));
    }
}

/*
 *  ======== ADCMSP432_init ========
 */
void ADCMSP432_init(ADC_Handle handle)
{
    /* Mark the object as available */
    ((ADCMSP432_Object *) handle->object)->isOpen = false;
}

/*
 *  ======== ADCMSP432_open ========
 */
ADC_Handle ADCMSP432_open(ADC_Handle handle, ADC_Params *params)
{
    uintptr_t                key;
    SemaphoreP_Params        semParams;
    ADCMSP432_Object        *object = handle->object;
    ADCMSP432_HWAttrs const *hwAttrs = handle->hwAttrs;

    /* Determine if the driver was already opened */
    key = HwiP_disable();

    if (object->isOpen) {
        HwiP_restore(key);

        DebugP_log0("ADC: Error! Already in use.");
        return (NULL);
    }

    if (adcInstance == 0) {
        SemaphoreP_Params_init(&semParams);
        semParams.mode = SemaphoreP_Mode_BINARY;
        globalMutex = SemaphoreP_create(1, &semParams);
        if (globalMutex == NULL) {
            HwiP_restore(key);

            DebugP_log0("ADC: SemaphoreP_create() failed.");
            return (NULL);
        }

        /* Power management support - Disable shutdown while driver is open */
        Power_setConstraint(PowerMSP432_DISALLOW_SHUTDOWN_0);
        Power_setConstraint(PowerMSP432_DISALLOW_SHUTDOWN_1);

        /* Initialize peripheral */
        initHw(object, hwAttrs);
    }
    adcInstance++;
    object->isOpen = true;

    HwiP_restore(key);

    /* Config GPIO for ADC channel analog input */
     MAP_GPIO_setAsPeripheralModuleFunctionInputPin(hwAttrs->gpioPort,
        hwAttrs->gpioPin, hwAttrs->gpioMode);

    DebugP_log0("ADC: Object opened");

    return (handle);
}
