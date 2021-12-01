/*
 * Description: Handling of the ADS1120 SPI device. This implementation assumes that
 * measure is done using differential measure. i.e all 4 ADS inputs is used.
 * This implementation is based on SBAS535C – AUGUST 2013 – REVISED FEBRUARY 2017 from
 * Texas instruments.
 *
 * The implementation is not generic since some logic is defined by the actual PCB layout.
 */
#include <stdint.h>
#include <string.h>
#include "ADS1120.h"

#define ADS_TIMEOUT 100 // ms, lowest possible value, device is handling operations in uSec.
typedef union ADS1120_RegConfig
{
    struct {
        struct {
            uint8_t pga_bypass : 1;
            uint8_t gain       : 3;
            uint8_t mux        : 4;
        };
        struct {
            uint8_t bcs        : 1;
            uint8_t ts         : 1;
            uint8_t cm         : 1;
            uint8_t mode       : 2;
            uint8_t dr         : 3;
        };
        struct {
            uint8_t idac       : 3;
            uint8_t psw        : 1;
            uint8_t fir_filter : 2;
            uint8_t vref       : 2;
        };
        struct {
            uint8_t nop        : 1;
            uint8_t drdym      : 1;
            uint8_t i2mux      : 3;
            uint8_t i1mux      : 3;
        };
    }; // anonymous
    uint8_t regs[4];
    uint32_t u32reg;
} ADS1120_RegConfig;

typedef union ADS1120Cmd {
    struct {
        uint8_t count    : 2; // No Of bytes - 1
        uint8_t regBegin : 2; // Register offset
        uint8_t rwcmd    : 4;
    }; // anonymous
    uint8_t byte;
} ADS1120Cmd;

//static void powerDown(SPI_HandleTypeDef *hspi);     // Enter power-down mode (not implemented)

// Reset the device
static HAL_StatusTypeDef reset(ADS1120Device *dev)
{
    ADS1120Cmd cmd = { .byte = 0b00000110 };

    return HAL_SPI_Transmit(dev->hspi, &cmd.byte, 1, ADS_TIMEOUT);
}

static bool waitDataReady(ADS1120Device *dev)
{
    for (int count=0; !stmGetGpio(dev->drdy) && count < 25000; count++) { };

    // If low , data is ready, else something is wrong.
    return stmGetGpio(dev->drdy);
}

// Start or restart conversions.
static HAL_StatusTypeDef ADCsync(ADS1120Device *dev)
{
    ADS1120Cmd cmd = { .byte = 0b00001000 };
    return HAL_SPI_Transmit(dev->hspi, &cmd.byte, 1, ADS_TIMEOUT);
}

// Read data by command
static HAL_StatusTypeDef readADC(ADS1120Device *dev, uint16_t* value)
{
    ADS1120Cmd cmd = { .rwcmd = 1, .regBegin = 0, .count = 0 };
    if (!waitDataReady(dev))
        return HAL_ERROR; // No data from chip.

    uint8_t rxData[2] = { 0 };
    HAL_SPI_Transmit(dev->hspi, &cmd.byte, 1, ADS_TIMEOUT);
    if (HAL_SPI_Receive(dev->hspi, rxData, sizeof(rxData), ADS_TIMEOUT) != HAL_OK)
        return HAL_ERROR;

    *value = (rxData[0] << 8) | rxData[1];

    return HAL_OK;
}

// it is possible to a single register but to simplify stuff, just read it all.
static HAL_StatusTypeDef readRegisters(ADS1120Device *dev, ADS1120_RegConfig* regs)
{
    ADS1120Cmd cmd = { .byte = 0b00100011 };

    HAL_StatusTypeDef ret = HAL_SPI_Transmit(dev->hspi, &cmd.byte, 1, ADS_TIMEOUT);
    if (ret != HAL_OK)
        return ret;

    if (waitDataReady(dev))
        return HAL_SPI_Receive(dev->hspi, regs->regs, sizeof(regs->regs), ADS_TIMEOUT);

    return HAL_ERROR;
}

// Write nn registers starting at address rr
static HAL_StatusTypeDef writeRegister(ADS1120Device *dev, const ADS1120_RegConfig* regs, uint8_t count, uint8_t offset)
{
    if ((count+offset) > 4)
        return HAL_ERROR; // Only 4 bytes is available.

    ADS1120Cmd cmd = { .count = (count - 1), .regBegin = offset,  .rwcmd = 4 };
    uint8_t spiReq[1 + 4] = { cmd.byte, 0 };
    memcpy(&spiReq[1], &regs->regs[offset], count);
    return HAL_SPI_Transmit(dev->hspi, spiReq, 1 + count, ADS_TIMEOUT);
}

// Public functions begin.
int ADS1120Configure(ADS1120Device *ads1120)
{
    int ret;
    const ADS1120_RegConfig cfg =
    {
            .pga_bypass = 0, // Gain should be used.
            .gain       = 1,
            .mux        = 0, // 0="AIN P = AIN0, AIN N = AIN1", 5="AIN P = AIN2, AIN N = AIN3"
            .bcs        = 0, // TBD, enable Burn-out current sensor.

            .ts         = 0, // Temperature mode
            .cm         = 0, // Single shot
            .mode       = 0, // Normal Mode
            .dr         = 4, // 90 SPS

            .idac       = 0, // Must be zero, else measure is wrong.
            .psw        = 0, // Our application is not a power bridge.
            .fir_filter = 0, // No filter, only used for 20 SPS and 5SPS
            .vref       = 2, // External reference selected using AIN0/REFP1 and AIN3/REFN1 inputs

            .nop        = 0,
            .drdym      = 0, // Only the dedicated DRDY pin is used to indicate when data are ready
            .i1mux      = 0, // not routed
            .i2mux      = 0, // not routed
    };

    // Select device.
    stmSetGpio(ads1120->cs, false);

    // Make a do-while to make it possible to break out in case of error
    // and thus make it possible to make common handling of CS.
    do {
        HAL_Delay(1); // Wait td(CSSC) < 1mSec.
        if (reset(ads1120) != HAL_OK) {
            ret = 1;
            break;
        }
        HAL_Delay(1); // wait 50µs+32*t(CLK) < 1mSec
        if (writeRegister(ads1120, &cfg, 4, 0) != HAL_OK) {
            ret = 2;
            break;
        }

        ADS1120_RegConfig test;
        bzero(&test, sizeof(test));
        if (readRegisters(ads1120, &test) != HAL_OK) {
            ret = 3;
            break; // Test the settings.
        }

        if (memcmp(test.regs, cfg.regs, 4) != 0) {
            ret = 4;
            break;
        }
        ret = 0; // All good.
    } while(0);

    // Release device
    stmSetGpio(ads1120->cs, true);
    return ret;
}

int ADS1120Read(ADS1120Device *dev, ADS1120_data* data)
{
    int ret = 0;
    uint16_t adcValue;
    stmSetGpio(dev->cs, false);

    ADCsync(dev);
    if (readADC(dev, &adcValue) == HAL_OK)
    {
        data->chA = adcValue;
    }
    else
    {
        ret = -1;
    }

    stmSetGpio(dev->cs, true);
    return ret;
}
