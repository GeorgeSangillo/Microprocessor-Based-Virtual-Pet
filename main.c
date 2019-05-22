//****************************************************************************
//
// main.c - Project 6
// By: George Sangillo
// Date: 12/12/2018
// Instructor: Dr. DeBrunner
// Course: Advanced Microprocessors
//
// Requirements:
// (1) Use the TI-RTOS with the MSP432 launchpad and EDUMKII booster pack.
// (2) Display logo on LCD for at least 5 seconds. The logo is displayed
// (3) Use the buzzer.
// (4) Use the joystick.
// (5) Use the temperature sensor.
// (6) Use both pushbuttons.
// (7) Use the accelerometer.
//
// HOW THE GAME WORKS:
// - Use joystick to move pet
// - Use BSP_S1 to pause/resume
// - Use BSP_S2 to invert joystick controls
// - Accelerometer decides LCD orientation
// - TMP006 affects health lost/gained
// - Restore health by refilling with AIR
// - Game lasts up to 10 minutes (600 seconds)
// - BUZZ alerts End Of Game
//
// What works:
// - the splash screen
// - the buzzer
// - the joystick
// - the temperature sensor
// - both BSP pushbuttons
// - the accelerometer
//
// Issues:
// - NONE have been noticed yet
//
//****************************************************************************

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <stdio.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Semaphore.h>

/* TI-RTOS Header files */
#include <driverlib.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/PWM.h>
#include <ti/drivers/ADC.h>
#include "buzzer_driver.h"

/* Graphics library stuff */
#include <grlib.h>
#include "LcdDriver/Crystalfontz128x128_ST7735.h"
#include "LcdDriver/HAL_MSP_EXP432P401R_Crystalfontz128x128_ST7735.h"

#include "celtics_splash_screen.h"
// #include "bball.h"           // NOT IMPLEMENTED
// #include "bball_inverse.h"   // NOT IMPLEMENTED

/* Board Header file */
#include "Board.h"

#include "HAL_I2C.h"
#include "HAL_TMP006.h"

/* Graphic library context */
Graphics_Context g_sContext;

#define TASKSTACKSIZE   2000

Clock_Struct clkStruct_logo, clkStruct_rules, clkStruct_periodic;
Clock_Handle clkHandle_logo, clkHandle_rules, clkHandle_periodic;
Task_Struct tskStruct_updateJOY, tskStruct_updateACC, tskStruct_updateTMP;
char tskStack_updateJOY[TASKSTACKSIZE], tskStack_updateACC[TASKSTACKSIZE], tskStack_updateTMP[TASKSTACKSIZE];
Semaphore_Struct semStruct_updateADC, semStruct_updateTMP;
Semaphore_Handle semHandle_updateADC, semHandle_updateTMP;

/* Flags */
int init_flag = 0;      // Starts periodic tasks after splash screen and game rules
int pause = 0;          // PAUSED: 1  RESUME:   0
int invert = 1;         // NORMAL: 1  INVERSE: -1

/* Variables */
float temp = 0;         // TMP006 value
float tempP = 0;        // TMP006 previous value
uint8_t Tmult = 1;      // temp<90: 1   temp>90: 2  Used to double loss of health
uint16_t time = 600;    // remaining time in seconds
uint8_t health = 50;    // health of pet    Max health is 100
uint32_t count = 0;     // counter variable used to divide periodic tasks further
uint32_t airCount = 1;  // used to force user to refill pet with air for atleast 5 seconds to regain health
int prevH=64,  posH;    // previous and current horizontal position values used for pet based on JOY data
int prevV=112, posV;    // previous and current vertical position values used for pet based on JOY data
struct ACCdata
{
    uint16_t X;
    uint16_t Y;
    uint16_t Z;
}; // Holds X Y Z values from Accelerometer
struct JOYdata
{
    int   H;
    int   V;
    uint8_t Sel; // NOT IMPLEMENTED
}; // Holds H V Sel values from Joystick
struct ACCdata ACCcur;
struct JOYdata JOYcur;

/* Draw data on the LCD */
void Draw_LCD()
{
    static char string[20];
    struct Graphics_Rectangle backboard={48,25,80,41};
    struct Graphics_Rectangle square={60,31,68,39};
    struct Graphics_Rectangle airpump={3,88,15,124};
    struct Graphics_Rectangle textbox1={76,0,127,11};
    struct Graphics_Rectangle textbox2={76,11,127,21};

    // Update position values based on LCD orientation
    if (Lcd_Orientation == LCD_ORIENTATION_UP)
    {
        posH = prevH + (invert * JOYcur.H);
        posV = prevV - (invert * JOYcur.V);
    }
    else if (Lcd_Orientation == LCD_ORIENTATION_LEFT)
    {
        posH = prevH - (invert * JOYcur.V);
        posV = prevV - (invert * JOYcur.H);
    }
    else if (Lcd_Orientation == LCD_ORIENTATION_DOWN)
    {
        posH = prevH - (invert * JOYcur.H);
        posV = prevV + (invert * JOYcur.V);
    }
    else
    {
        posH = prevH + (invert * JOYcur.V);
        posV = prevV + (invert * JOYcur.H);
    }

    // Draw pet and check if health should be refilled
    if(((posH > 26)&&(posH < 102))&&((posV > 51)&&(posV < 114)))
    {
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        Graphics_fillCircle(&g_sContext, prevH, prevV, 9);
        Graphics_drawCircle(&g_sContext, prevH, prevV, 10);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_ORANGE);
        Graphics_fillCircle(&g_sContext, posH, posV, 9);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
        Graphics_drawCircle(&g_sContext, posH, posV, 10);
        /*
        Graphics_drawImage(&g_sContext, &bball_inverse,prevH,prevV);  // NOT IMPLEMENTED
        Graphics_drawImage(&g_sContext, &bball,posH,posV);            // NOT IMPLEMENTED
        */
        // Determine if pet is in location for refilling
        if(posH<30)
        {
            if(posV>88)
            {
                if(airCount%50==0)
                {
                    if(health>90)
                        health = 100; // prevent going past max health
                    else
                        health += 10 / Tmult;
                }
                airCount++;
            }
        }
        else
            airCount = 1;
        prevH = posH; // update H value for next period
        prevV = posV; // update V value for next period
    }
    // Used to not update values if the new ones would have been outside of acceptable range
    else
    {
        posH = prevH;
        posV = prevV;
    }

    // Erase last digit when # of digits decreases for health
    if(health<10)
    {
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        Graphics_drawString(&g_sContext, (int8_t *)"0000", 5, 58, 4, OPAQUE_TEXT);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
    }
    else if(health<100)
    {
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        Graphics_drawString(&g_sContext, (int8_t *)"00", 3, 64, 4, OPAQUE_TEXT);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
    }

    // Draw stats, backboard, and airpump
    sprintf(string, "Health: %d", health);
    Graphics_drawString(&g_sContext, (int8_t *)string, AUTO_STRING_LENGTH, 4, 4, OPAQUE_TEXT);
    sprintf(string, "Time:   %d", 600-time);
    Graphics_drawString(&g_sContext, (int8_t *)string, AUTO_STRING_LENGTH, 4, 12, OPAQUE_TEXT);
    // Prevents pause and invert status from not being erased
    if(pause==0)
    {
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        Graphics_fillRectangle(&g_sContext, &textbox1);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
    }
    if(invert==-1)
        Graphics_drawString(&g_sContext, (int8_t *)"INVERTED", 9, 80, 12, OPAQUE_TEXT);
    else
    {
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        Graphics_fillRectangle(&g_sContext, &textbox2);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
    }
    Graphics_drawLineH(&g_sContext, 0, 127, 22);
    Graphics_drawRectangle(&g_sContext, &backboard);
    Graphics_drawRectangle(&g_sContext, &square);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_ORANGE_RED);
    Graphics_drawCircle(&g_sContext, 64, 45, 4);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
    Graphics_drawRectangle(&g_sContext, &airpump);
    Graphics_drawString(&g_sContext, (int8_t *)"A", 2, 7, 96, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int8_t *)"I", 2, 7, 104, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int8_t *)"R", 2, 7, 112, OPAQUE_TEXT);
}

/* One-Shot Task to Erase the Splash Screen After 5 seconds */
void CLK_TSK_LCD_erase_splash_screen()
{
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLUE);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawStringCentered(&g_sContext, (int8_t *)"GAME RULES", 11, 64, 5, OPAQUE_TEXT);
    Graphics_drawLineH(&g_sContext, 0, 127, 12);
    Graphics_drawString(&g_sContext, (int8_t *)"JOY 2 move", 11, 5, 20, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int8_t *)"S1 2 pause", 11, 5, 30, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int8_t *)"S2 2 invert JOY", 16, 5, 40, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int8_t *)"ACC 4 orient", 13, 5, 50, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int8_t *)"TMP affects health", 19, 5, 60, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int8_t *)"AIR gains health", 17, 5, 70, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int8_t *)"Game lasts 600 s", 17, 5, 80, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int8_t *)"BUZZ alerts EOG", 16, 5, 90, OPAQUE_TEXT);

    Clock_start(clkHandle_rules);
}

/* One-Shot Task to Erase the Splash Screen After 5 seconds */
void CLK_TSK_LCD_erase_game_rules()
{
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
    Graphics_clearDisplay(&g_sContext);
    Clock_start(clkHandle_periodic);
}

/* Periodic Task to update ADC and TMP006 values */
void CLK_TSK_update()
{
    init_flag = 1;
    static char string[20];
    if(pause==0)
    {
        Semaphore_post(semHandle_updateADC); // For JOY and ACC tasks
        if(count%5==0)
            Semaphore_post(semHandle_updateTMP);
        count++;
        if(count%10==0)
            time--;
        if(count%150==0)
            health -= 5 * Tmult;
    }
    else
        Graphics_drawString(&g_sContext, (int8_t *)"PAUSED   ", 10, 80, 4, OPAQUE_TEXT);
    if(time==0)
    {
        set_intensity_buzzer(255);
        start_buzzer(); // low BUZZ alerts End Of Game by Win
        Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP);
        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        GrContextFontSet(&g_sContext, &g_sFontCmsc14);
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawStringCentered(&g_sContext, (int8_t *)"GAME OVER", 10, 64, 20, OPAQUE_TEXT);
        Graphics_drawStringCentered(&g_sContext, (int8_t *)"You Won!!", 10, 64, 40, OPAQUE_TEXT);
        GrContextFontSet(&g_sContext, &g_sFontCmss12i);
        sprintf(string, "HEALTH: %d", health);
        Graphics_drawStringCentered(&g_sContext, (int8_t *)string, AUTO_STRING_LENGTH, 64, 80, OPAQUE_TEXT);
        stop_buzzer();
        BIOS_exit(1);
    }
    if(health==0)
    {
        set_intensity_buzzer(64);
        start_buzzer(); // high BUZZ alerts End Of Game by Loss
        Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP);
        Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
        Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
        GrContextFontSet(&g_sContext, &g_sFontCmsc14);
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawStringCentered(&g_sContext, (int8_t *)"GAME OVER", 10, 64, 20, OPAQUE_TEXT);
        Graphics_drawStringCentered(&g_sContext, (int8_t *)"You Lost", 9, 64, 40, OPAQUE_TEXT);
        GrContextFontSet(&g_sContext, &g_sFontCmss12i);
        sprintf(string, "TIME: %d s", 600-time);
        Graphics_drawStringCentered(&g_sContext, (int8_t *)string, AUTO_STRING_LENGTH, 64, 80, OPAQUE_TEXT);
        stop_buzzer();
        BIOS_exit(1);
    }
}

/* read Horizontal values from Joystick */
float ADC_read_H()
{
    ADC_Handle   joy;
    ADC_Params   params;
    uint16_t     adcValueH;

    ADC_Params_init(&params);
    joy = ADC_open(BSP_JOYh, &params);
    ADC_convert(joy, &adcValueH);
    ADC_close(joy);

    return adcValueH;
}

/* read Vertical values from Joystick */
float ADC_read_V()
{
    ADC_Handle   joy;
    ADC_Params   params;
    uint16_t     adcValueV;

    ADC_Params_init(&params);
    joy = ADC_open(BSP_JOYv, &params);
    ADC_convert(joy, &adcValueV);
    ADC_close(joy);

    return adcValueV;
}

/* Reads values from Joystick on BSP to move pet */
void ADC_JOY_TSK()
{
    while(1)
    {
        Semaphore_pend(semHandle_updateADC, BIOS_WAIT_FOREVER);

        JOYcur.H = ADC_read_H();
        if((JOYcur.H>120)&&(JOYcur.H<136))
            JOYcur.H = 0;
        else
            JOYcur.H = (JOYcur.H - 128) / 32;

        JOYcur.V = ADC_read_V();
        if((JOYcur.V>120)&&(JOYcur.V<136))
            JOYcur.V = 0;
        else
            JOYcur.V = (JOYcur.V - 128) / 32;

        if(init_flag)
            Draw_LCD();
    }
}

/* read X values from Accelerometer */
float ADC_read_X()
{
    ADC_Handle   adc;
    ADC_Params   params;
    uint16_t     adcValueX;

    ADC_Params_init(&params);
    adc = ADC_open(Board_ACCx, &params);
    ADC_convert(adc, &adcValueX);
    ADC_close(adc);

    return adcValueX;
}

/* read Y values from Accelerometer */
float ADC_read_Y()
{
    ADC_Handle   adc;
    ADC_Params   params;
    uint16_t     adcValueY;

    ADC_Params_init(&params);
    adc = ADC_open(Board_ACCy, &params);
    ADC_convert(adc, &adcValueY);
    ADC_close(adc);

    return adcValueY;
}

/* Task for reading from Accelerometer to determine LCD orientation */
void ADC_ACC_TSK()
{
    while(1)
    {
        Semaphore_pend(semHandle_updateADC, BIOS_WAIT_FOREVER);

        ACCcur.X = ADC_read_X();
        ACCcur.Y = ADC_read_Y();

        if (ACCcur.X < 5600)
        {
            if (Lcd_Orientation != LCD_ORIENTATION_LEFT)
            {
                Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_LEFT);
                Graphics_clearDisplay(&g_sContext);
            }
        }
        else if (ACCcur.X > 10400)
        {
            if (Lcd_Orientation != LCD_ORIENTATION_RIGHT)
            {
                Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_RIGHT);
                Graphics_clearDisplay(&g_sContext);
            }
        }
        else if (ACCcur.Y < 5600)
        {
            if (Lcd_Orientation != LCD_ORIENTATION_UP)
            {
                Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP);
                Graphics_clearDisplay(&g_sContext);
            }
        }
        else if (ACCcur.Y > 10400)
        {
            if (Lcd_Orientation != LCD_ORIENTATION_DOWN)
            {
                Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_DOWN);
                Graphics_clearDisplay(&g_sContext);
            }
        }
    }
}

/* Task for reading TMP006 */
void TEMP_SENS_TSK()
{
    while (1)
    {
        Semaphore_pend(semHandle_updateTMP, BIOS_WAIT_FOREVER);
        temp = TMP006_getTemp();
        if((temp>90)&&(tempP<90))
            Tmult = 2;
        else
            Tmult = 1;
        tempP = temp;
    }
}

/* Task for pause/resume game upon BSP_S1 press */
void GPIO_TSK_BSP_S1(unsigned int index)
{
    if(init_flag==1)
    {
        GPIO_toggle(Board_LED1); // represents value of game status
        if(pause == 0)
            pause = 1;
        else
            pause = 0;
    }
}

/* Task for inverting Joytick controls upon BSP_S2 press */
void GPIO_TSK_BSP_S2(unsigned int index)
{
    if((pause==0)&&(init_flag==1))
    {
        GPIO_toggle(Board_LED2R); // represents value of invert
        if(invert == 1)
            invert = -1;
        else
            invert = 1;
    }
}

/*
 *  ======== main ========
 */
int main(void)
{
    Clock_Params     clkParams;
    Task_Params      taskParams;
    Semaphore_Params semParams;

    /* Call board init functions */
    Board_initGeneral();
    Board_initADC();
    Board_initGPIO();
    Board_initPWM();
    init_buzzer(10000, TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_4);
    Init_I2C_GPIO();
    I2C_init();
    TMP006_init();

    /* Initialize Parameters */
    Clock_Params_init(&clkParams);
    Task_Params_init(&taskParams);
    Semaphore_Params_init(&semParams);

    /* Construct the one-shot Clock Instance for splash screen*/
    clkParams.period = 0;
    clkParams.startFlag = TRUE;
    Clock_construct(&clkStruct_logo, (Clock_FuncPtr)CLK_TSK_LCD_erase_splash_screen,
                    Clock_tickPeriod*5, &clkParams);
    clkHandle_logo = Clock_handle(&clkStruct_logo);

    /* Construct the one-shot Clock Instance for game rules*/
    clkParams.startFlag = FALSE;
    Clock_construct(&clkStruct_rules, (Clock_FuncPtr)CLK_TSK_LCD_erase_game_rules,
                    Clock_tickPeriod*10, &clkParams);
    clkHandle_rules = Clock_handle(&clkStruct_rules);

    /* Construct the periodic Clock Instance for remainder of game */
    clkParams.period = Clock_tickPeriod/10;
    Clock_construct(&clkStruct_periodic, (Clock_FuncPtr)CLK_TSK_update,
                    1, &clkParams);
    clkHandle_periodic  = Clock_handle(&clkStruct_periodic);

    /* Construct Tasks */
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.stack = &tskStack_updateJOY;
    Task_construct(&tskStruct_updateJOY, (Task_FuncPtr)ADC_JOY_TSK, &taskParams, NULL);
    taskParams.stack = &tskStack_updateACC;
    Task_construct(&tskStruct_updateACC, (Task_FuncPtr)ADC_ACC_TSK, &taskParams, NULL);
    taskParams.stack = &tskStack_updateTMP;
    Task_construct(&tskStruct_updateTMP, (Task_FuncPtr)TEMP_SENS_TSK, &taskParams, NULL);

    /* Construct Semaphore objects to be use as a resource lock */
    semParams.mode = ti_sysbios_knl_Semaphore_Mode_BINARY;
    Semaphore_construct(&semStruct_updateADC, 0, &semParams);
    semHandle_updateADC = Semaphore_handle(&semStruct_updateADC);
    Semaphore_construct(&semStruct_updateTMP, 0, &semParams);
    semHandle_updateTMP = Semaphore_handle(&semStruct_updateTMP);

    /* install Button callback */
    GPIO_setCallback(BSP_S1, GPIO_TSK_BSP_S1);
    GPIO_setCallback(BSP_S2, GPIO_TSK_BSP_S2);

    /* Enable Interrupts */
    GPIO_enableInt(BSP_S1);
    GPIO_enableInt(BSP_S2);

    /* Initializes display */
    Crystalfontz128x128_Init();

    /* Set default screen orientation */
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP);

    /* Initializes graphics context */
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128, &g_sCrystalfontz128x128_funcs);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
    GrContextFontSet(&g_sContext, &g_sFontFixed6x8);
    Graphics_clearDisplay(&g_sContext);

    /* Draw Splash Screen at start */
    Graphics_drawImage(&g_sContext, &celtics_splash_screen,0,0);

    /* Start BIOS */
    BIOS_start();

    return (0);
}
