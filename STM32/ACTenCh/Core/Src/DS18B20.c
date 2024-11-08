/*
 * DS18B20.c
 *
 *  Created on: 7 Nov 2024
 *      Author: matias
 * 
 * 	Library based heavily on the implementation done by [vtx22](https://github.com/vtx22/STM32-DS18B20/blob/master/src/DS18B20.cpp)
 */

#include "DS18B20.h"

TIM_HandleTypeDef* DS18B20_tim = NULL;

typedef struct Comm {
    StmGpio * dataGpio;	// GPIO for sending/receiving data to/from DS18B20
    GPIO_TypeDef *blk; 	
    uint16_t pin;
} Comm; 

Comm comm = {0};

/***************************************************************************************************
** PRIVATE FUNCTION PROTOTYPES
***************************************************************************************************/

static void delay_us(uint32_t us);
static void writeData(uint8_t data);
static uint8_t readData();
static uint8_t startSensor();

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

void setPinOutput()
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = comm.pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    HAL_GPIO_Init(comm.blk, &GPIO_InitStruct);
    stmGpioInit(comm.dataGpio, comm.blk, comm.pin, STM_GPIO_OUTPUT);
}

void setPinInput()
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = comm.pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(comm.blk, &GPIO_InitStruct);
    stmGpioInit(comm.dataGpio, comm.blk, comm.pin, STM_GPIO_INPUT);
}

/*!
** @brief Returns after us microseconds
** @note The DS18B20_tim updates with 1MHz
*/
static void delay_us(uint32_t us)
{
    __HAL_TIM_SET_COUNTER(DS18B20_tim, 0);
    while (DS18B20_tim->Instance->CNT < us)
    {

    }
}

/*!
** @brief Sends a byte to the DS18B20
*/
static void writeData(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (data & (1 << i))
        {
            setPinOutput();
            stmSetGpio(*comm.dataGpio, false);
            delay_us(1);

            setPinInput();
            delay_us(50);
            continue;
        }

        setPinOutput();
        stmSetGpio(*comm.dataGpio, false);
        delay_us(50);

        setPinInput();
    }
}

/*!
** @brief Receives a byte from the DS18B20
*/
static uint8_t readData()
{
    uint8_t value = 0;
    setPinInput();
    for (uint8_t i = 0; i < 8; i++)
    {
        setPinOutput();
        stmSetGpio(*comm.dataGpio, false);
        delay_us(2);

        setPinInput();

        if (stmGetGpio(*comm.dataGpio))
        {
            value |= 1 << i;
        }

        delay_us(60);
    }
    return value;
}

/*!
** @brief Initialisation of communication to DS18B20
** @note startSensor() is called prior to any communication with the DS18B20
*/
static uint8_t startSensor()
{
    uint8_t alive = 0;
    setPinOutput();
    stmSetGpio(*comm.dataGpio, false);

    delay_us(480);
    setPinInput();
    delay_us(80);
    alive = (!stmGetGpio(*comm.dataGpio)) ? 1 : 0;
    delay_us(400);
    return alive;
}


/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

/*!
** @brief Initialises a temperature conversion and gets it when ready.
** @note Should be called in main loop.
*/
float getTemp()
{
    static int acquiring = 0;
    static uint32_t resetTime = 0;
    static uint16_t adc = 0;
    static float temp = 0;

    if (!acquiring)
    {
        acquiring = 1;

        // If sensor doesn't respond return latest temp
        if (startSensor() == -1)
        {
            acquiring = 0;
            return temp; 
        }

        writeData(0xCC); // skip ROM
        writeData(0x44); // start temperature acquisition

        resetTime = HAL_GetTick();
    } 
    else if (acquiring && ((HAL_GetTick() - resetTime) >= 800))
    {
        acquiring = 0;

        // If sensor doesn't respond return latest temp
        if (startSensor() == -1)
        {
            return temp; 
        }

        writeData(0xCC); // skip ROM
        writeData(0xBE); // read temperature from scratch pad

        uint8_t adc_low = readData();
        uint8_t adc_high = readData();
        adc = (adc_high << 8) | adc_low;
        temp = (float) (adc / 16.0f);
    }
    return temp;
}

/*!
** @brief Initialises handles and timer needed for DS18B20 communication
*/
void DS18B20Init(TIM_HandleTypeDef* htim, StmGpio* gpio, GPIO_TypeDef *blk, uint16_t pin)
{
    comm.dataGpio = gpio;
    comm.blk = blk;
    comm.pin = pin;

    DS18B20_tim = htim;
    HAL_TIM_Base_Start(DS18B20_tim);
}
