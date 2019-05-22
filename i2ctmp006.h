/*
 * i2ctmp006.h
 *
 *  Created on: Nov 26, 2018
 *      Author: georg
 */

#ifndef I2CTMP006_H_
#define I2CTMP006_H_

//#define TASKSTACKSIZE       640

//Task_Struct task0Struct;
//char task0Stack[TASKSTACKSIZE];

/*
 *  ======== taskFxn ========
 *  Task for this function is created statically. See the project's .cfg file.
 */
void taskFxn(UArg arg0, UArg arg1)
{
    unsigned int    i;
    uint16_t        temperature;
    uint8_t         txBuffer[1];
    uint8_t         rxBuffer[2];
    I2C_Handle      i2c;
    I2C_Params      i2cParams;
    I2C_Transaction i2cTransaction;

    /* Create I2C for usage */
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }
    else {
        System_printf("I2C Initialized!\n");
    }

    /* Point to the T ambient register and read its 2 bytes */
    txBuffer[0] = 0x01;
    i2cTransaction.slaveAddress = Board_TMP006_ADDR;
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = rxBuffer;
    i2cTransaction.readCount = 2;

    /* Take 20 samples and print them out onto the console */
    for (i = 0; i < 20; i++) {
        if (I2C_transfer(i2c, &i2cTransaction)) {
            /* Extract degrees C from the received data; see TMP102 datasheet */
            temperature = (rxBuffer[0] << 6) | (rxBuffer[1] >> 2);

            /*
             * If the MSB is set '1', then we have a 2's complement
             * negative value which needs to be sign extended
             */
            if (rxBuffer[0] & 0x80) {
                temperature |= 0xF000;
            }
           /*
            * For simplicity, divide the temperature value by 32 to get rid of
            * the decimal precision; see TI's TMP006 datasheet
            */
            temperature /= 32;

            System_printf("Sample %u: %d (C)\n", i, temperature);
        }
        else {
            System_printf("I2C Bus fault\n");
        }

        System_flush();
        Task_sleep(1000);
    }

    /* Deinitialized I2C */
    I2C_close(i2c);
    System_printf("I2C closed!\n");

    System_flush();
}

#endif /* I2CTMP006_H_ */
