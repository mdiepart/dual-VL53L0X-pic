/*
 * COPYRIGHT (C) STMicroelectronics 2015. All rights reserved.
 *
 * This software is the confidential and proprietary information of
 * STMicroelectronics ("Confidential Information").  You shall not
 * disclose such Confidential Information and shall use it only in
 * accordance with the terms of the license agreement you entered into
 * with STMicroelectronics
 *
 * Programming Golden Rule: Keep it Simple!
 *
 */

/*!
 * \file   VL53L0X_platform.c
 * \brief  Code function defintions for Doppler Testchip Platform Layer
 *
 */

/* sprintf(), vsnprintf(), printf()*/
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#include "vl53l0x_i2c_platform.h"
#include "vl53l0x_def.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "vl53l0x_platform_log.h"

#include <xc.h>

#ifdef VL53L0X_LOG_ENABLE
#define trace_print(level, ...) trace_print_module_function(TRACE_MODULE_PLATFORM, level, TRACE_FUNCTION_NONE, ##__VA_ARGS__)
#define trace_i2c(...) trace_print_module_function(TRACE_MODULE_NONE, TRACE_LEVEL_NONE, TRACE_FUNCTION_I2C, ##__VA_ARGS__)
#endif

char  debug_string[VL53L0X_MAX_STRING_LENGTH_PLT];

uint8_t cached_page = 0;

#define MIN_COMMS_VERSION_MAJOR     1
#define MIN_COMMS_VERSION_MINOR     8
#define MIN_COMMS_VERSION_BUILD     1
#define MIN_COMMS_VERSION_REVISION  0

#define STATUS_OK              0x00
#define STATUS_FAIL            0x01

bool_t _check_min_version(void)
{
    return true;
}

/*Uses MSSP1 in I2C mode. Speed is 100 kbit*/
int VL53L0X_i2c_init(void)
{
    /*Using standard speed mode*/
    SSP1STAT.SMP=1;
    
    /*PCIE=0 - No interruption on stop bit detection
     *SCIE=0 - No interruption on start bit detection
     *Already set to 0 by default*/
    
    /*Sets clock divider*/
    SSP1ADD=0x4F; /* If Fosc = 16MHz*/
    /*SSP1ADD=0x27; // If Fosc = 8MHz*/
    
    /*Set TRIS bits 
     *I2C1 uses RB8 (SCL) and RB9 (SDA)
    
     *SSPEN=1 - Enables serial port and configures SDA1 and SCL1 as the serial port pins
     *SSPM=1000 I2C master mode.*/
    SSP1CON1 |= 0b0000000000101000;
    
    return STATUS_OK;
}
int32_t VL53L0X_comms_close(void)
{
    SSP1CON1 &= 0b1111111111011111;
    
    return STATUS_OK;
}

int32_t VL53L0X_write_multi(uint8_t address, uint8_t reg, uint8_t *pdata, int32_t count)
{   
    /*1. Start*/
    SSP1CON2.SEN = 1;
    
    /*2. Send address*/
    SSP1BUF = address;
    while(IFS1.SSP1IF == 0){} /*Wait for interrupt*/
    
    /*3. Check for ack*/
    if(SSP1CON2.ACKSTAT != 0){
        /*Error*/
        SSP1CON2.PEN = 1;
        return STATUS_FAIL;
    }
    /*4. Send Index*/
    SSP1BUF = reg;
    while(IFS1.SSP1IF == 0){} /*Wait for interrupt*/
    
    /*5. Check for ack*/
    if(SSP1CON2.ACKSTAT != 0){
        /*Error*/
        SSP1CON2.PEN = 1;
        return STATUS_FAIL;
    }
    
    uint8_t i = 0;
    for(; i < count; i++){
        /*6. send data*/
        SSP1BUF = pdata[i];
        while(IFS1.SSP1IF == 0){} /*Wait for interrupt*/
        
        /*Check for ack*/
        if(SSP1CON2.ACKSTAT != 0){
            /*Error*/
            SSP1CON2.PEN = 1;
            return STATUS_FAIL;
        }
    }
    
    /*8. Send stop*/
    SSP1CON2.PEN = 1;

    return STATUS_OK;
}

int32_t VL53L0X_read_multi(uint8_t address, uint8_t index, uint8_t *pdata, int32_t count)
{
    /*1. Start*/
    SSP1CON2.SEN = 1;
    
    /*2. Send address */
    SSP1BUF = address;
    while(IFS1.SSP1IF == 0){} /*Wait for interrupt*/
    
    /*3. Check for ack*/
    if(SSP1CON2.ACKSTAT != 0){
        /*Error*/
        SSP1CON2.PEN = 1;
        return STATUS_FAIL;
    }
    /*4. Send Index*/
    SSP1BUF = index;
    while(IFS1.SSP1IF == 0){} /*Wait for interrupt*/
    
    /*5. Check for ack*/
    if(SSP1CON2.ACKSTAT != 0){
        /*Error*/
        SSP1CON2.PEN = 1;
        return STATUS_FAIL;
    }
    
    /*6. Enters receiving mode*/
    SSP1CON2.RCEN = 1;
    
    /*7. Send repeated start*/
    SSP1CON2.REN = 1;
    
    /*8. Send address + 1 for reading*/
    SSP1BUF = address+1;
    
    while(IFS1.SSP1IF == 0){} /*Wait for interrupt*/
    
    /*5. Check for ack*/
    if(SSP1CON2.ACKSTAT != 0){
        /*Error*/
        SSP1CON2.PEN = 1;
        SSP1CON2.RCEN = 0;
        return STATUS_FAIL;
    }
    
    uint8_t i = 0;
    for(; i < count; i++){
        /*9. gets data*/
        while(IFS1.SSP1IF == 0){} /*Wait for interrupt*/
        pdata[i] = SSP1BUF;
        /*Sends ack*/
        SSP1CON2.ACKEN = 1;
    }
    
    /*8. Send stop*/
    SSP1CON2.PEN = 1;
    SSP1CON2.RCEN = 0; /*leaves receive mode*/

    return STATUS_OK;
}


int32_t VL53L0X_write_byte(uint8_t address, uint8_t index, uint8_t data)
{
    int32_t status = STATUS_OK;
    const int32_t cbyte_count = 1;

#ifdef VL53L0X_LOG_ENABLE
    trace_print(TRACE_LEVEL_INFO,"Write reg : 0x%02X, Val : 0x%02X\n", index, data);
#endif

    status = VL53L0X_write_multi(address, index, &data, cbyte_count);

    return status;

}


int32_t VL53L0X_write_word(uint8_t address, uint8_t index, uint16_t data)
{
    int32_t status = STATUS_OK;

    uint8_t  buffer[BYTES_PER_WORD];

    /* Split 16-bit word into MS and LS uint8_t*/
    buffer[0] = (uint8_t)(data >> 8);
    buffer[1] = (uint8_t)(data &  0x00FF);

    if(index%2 == 1)
    {
        status = VL53L0X_write_multi(address, index, &buffer[0], 1);
        status = VL53L0X_write_multi(address, index + 1, &buffer[1], 1);
        /* serial comms cannot handle word writes to non 2-byte aligned registers.*/
    }
    else
    {
        status = VL53L0X_write_multi(address, index, buffer, BYTES_PER_WORD);
    }

    return status;

}


int32_t VL53L0X_write_dword(uint8_t address, uint8_t index, uint32_t data)
{
    int32_t status = STATUS_OK;
    uint8_t  buffer[BYTES_PER_DWORD];

    /* Split 32-bit word into MS ... LS bytes*/
    buffer[0] = (uint8_t) (data >> 24);
    buffer[1] = (uint8_t)((data &  0x00FF0000) >> 16);
    buffer[2] = (uint8_t)((data &  0x0000FF00) >> 8);
    buffer[3] = (uint8_t) (data &  0x000000FF);

    status = VL53L0X_write_multi(address, index, buffer, BYTES_PER_DWORD);

    return status;

}


int32_t VL53L0X_read_byte(uint8_t address, uint8_t index, uint8_t *pdata)
{
    int32_t status = STATUS_OK;
    int32_t cbyte_count = 1;

    status = VL53L0X_read_multi(address, index, pdata, cbyte_count);

#ifdef VL53L0X_LOG_ENABLE
    trace_print(TRACE_LEVEL_INFO,"Read reg : 0x%02X, Val : 0x%02X\n", index, *pdata);
#endif

    return status;

}


int32_t VL53L0X_read_word(uint8_t address, uint8_t index, uint16_t *pdata)
{
    int32_t  status = STATUS_OK;
	uint8_t  buffer[BYTES_PER_WORD];

    status = VL53L0X_read_multi(address, index, buffer, BYTES_PER_WORD);
	*pdata = ((uint16_t)buffer[0]<<8) + (uint16_t)buffer[1];

    return status;

}

int32_t VL53L0X_read_dword(uint8_t address, uint8_t index, uint32_t *pdata)
{
    int32_t status = STATUS_OK;
	uint8_t  buffer[BYTES_PER_DWORD];

    status = VL53L0X_read_multi(address, index, buffer, BYTES_PER_DWORD);
    *pdata = ((uint32_t)buffer[0]<<24) + ((uint32_t)buffer[1]<<16) + ((uint32_t)buffer[2]<<8) + (uint32_t)buffer[3];

    return status;

}



/* 16 bit address functions*/


int32_t VL53L0X_write_multi16(uint8_t address, uint16_t index, uint8_t *pdata, int32_t count)
{
    int32_t status = STATUS_OK;
    
    return status;
}

int32_t VL53L0X_read_multi16(uint8_t address, uint16_t index, uint8_t *pdata, int32_t count)
{
    int32_t status = STATUS_OK;
    
    return status;
}



int32_t VL53L0X_write_byte16(uint8_t address, uint16_t index, uint8_t data)
{
    int32_t status = STATUS_OK;
    const int32_t cbyte_count = 1;

#ifdef VL53L0X_LOG_ENABLE
    trace_print(TRACE_LEVEL_INFO,"Write reg : 0x%02X, Val : 0x%02X\n", index, data);
#endif

    status = VL53L0X_write_multi16(address, index, &data, cbyte_count);

    return status;

}


int32_t VL53L0X_write_word16(uint8_t address, uint16_t index, uint16_t data)
{
    int32_t status = STATUS_OK;

    uint8_t  buffer[BYTES_PER_WORD];

    /* Split 16-bit word into MS and LS uint8_t*/
    buffer[0] = (uint8_t)(data >> 8);
    buffer[1] = (uint8_t)(data &  0x00FF);

    if(index%2 == 1)
    {
        status = VL53L0X_write_multi16(address, index, &buffer[0], 1);
        status = VL53L0X_write_multi16(address, index + 1, &buffer[1], 1);
        /* serial comms cannot handle word writes to non 2-byte aligned registers.*/
    }
    else
    {
        status = VL53L0X_write_multi16(address, index, buffer, BYTES_PER_WORD);
    }

    return status;

}


int32_t VL53L0X_write_dword16(uint8_t address, uint16_t index, uint32_t data)
{
    int32_t status = STATUS_OK;
    uint8_t  buffer[BYTES_PER_DWORD];

    /* Split 32-bit word into MS ... LS bytes*/
    buffer[0] = (uint8_t) (data >> 24);
    buffer[1] = (uint8_t)((data &  0x00FF0000) > 16);
    buffer[2] = (uint8_t)((data &  0x0000FF00) > 8);
    buffer[3] = (uint8_t) (data &  0x000000FF);

    status = VL53L0X_write_multi16(address, index, buffer, BYTES_PER_DWORD);

    return status;

}


int32_t VL53L0X_read_byte16(uint8_t address, uint16_t index, uint8_t *pdata)
{
    int32_t status = STATUS_OK;
    int32_t cbyte_count = 1;

    status = VL53L0X_read_multi16(address, index, pdata, cbyte_count);

#ifdef VL53L0X_LOG_ENABLE
    trace_print(TRACE_LEVEL_INFO,"Read reg : 0x%02X, Val : 0x%02X\n", index, *pdata);
#endif

    return status;

}


int32_t VL53L0X_read_word16(uint8_t address, uint16_t index, uint16_t *pdata)
{
    int32_t  status = STATUS_OK;
    uint8_t  buffer[BYTES_PER_WORD];

    status = VL53L0X_read_multi16(address, index, buffer, BYTES_PER_WORD);
    *pdata = ((uint16_t)buffer[0]<<8) + (uint16_t)buffer[1];

    return status;

}

int32_t VL53L0X_read_dword16(uint8_t address, uint16_t index, uint32_t *pdata)
{
    int32_t status = STATUS_OK;
    uint8_t  buffer[BYTES_PER_DWORD];

    status = VL53L0X_read_multi16(address, index, buffer, BYTES_PER_DWORD);
    *pdata = ((uint32_t)buffer[0]<<24) + ((uint32_t)buffer[1]<<16) + ((uint32_t)buffer[2]<<8) + (uint32_t)buffer[3];

    return status;

}




int32_t VL53L0X_platform_wait_us(int32_t wait_us)
{
    int32_t status = STATUS_OK;
    float wait_ms = (float)wait_us/1000.0f;

    return status;

}


int32_t VL53L0X_wait_ms(int32_t wait_ms)
{
    int32_t status = STATUS_OK;

    return status;

}


int32_t VL53L0X_set_gpio(uint8_t level)
{
    int32_t status = STATUS_OK;
    /*status = VL53L0X_set_gpio_sv(level);*/

    return status;

}


int32_t VL53L0X_get_gpio(uint8_t *plevel)
{
    int32_t status = STATUS_OK;

    return status;
}


int32_t VL53L0X_release_gpio(void)
{
    int32_t status = STATUS_OK;

    return status;

}

int32_t VL53L0X_cycle_power(void)
{
    int32_t status = STATUS_OK;

	return status;
}


int32_t VL53L0X_get_timer_frequency(int32_t *ptimer_freq_hz)
{
       *ptimer_freq_hz = 0;
       return STATUS_FAIL;
}


int32_t VL53L0X_get_timer_value(int32_t *ptimer_count)
{
       *ptimer_count = 0;
       return STATUS_FAIL;
}