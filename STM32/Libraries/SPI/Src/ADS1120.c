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
#include "USBprint.h"
#include "time32.h"

#define ADS_TIMEOUT 10 // In mSec.
#define INTERVAL_VOLTAGE_REF 2.048 // Volts
#define GAIN 16.0 // Defined from ADS1120_RegConfig.gain
#define QUANTIZATION 65536.0 // Quantization steps

// Helper union class for setting up registers in a 4 byte array.
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
    }; // anonymous struct.
    uint8_t regs[4];   // byte array used for transmission/receive.
    uint32_t u32reg;   // Nice to have in case of debug.
} ADS1120_RegConfig;

typedef union ADS1120Cmd {
    struct {
        uint8_t count    : 2; // No Of bytes - 1
        uint8_t regBegin : 2; // Register offset
        uint8_t rwcmd    : 4;
    }; // anonymous struct
    uint8_t byte;
} ADS1120Cmd;

static ADS1120_input nextInput(ADS1120_input current)
{
    switch(current)
    {
    case INPUT_TEMP:       return INPUT_CHA;
    case INPUT_CHA:        return INPUT_CHB;
    case INPUT_CHB:        return INPUT_CALIBRATE;
    case INPUT_CALIBRATE:  return INPUT_TEMP;
    }

    // Make compiler happy. If no return a warning from compiler is issued.
    return INPUT_CALIBRATE;
}


static double adc2Temp(int16_t adcValue, int16_t calibration, float internalTemp, float delta, float cj_delta)
{
    if (adcValue == 0x7fff)
        return 10000; // Nothing in port, send 10000 to set an invalid value.
    // TODO: How to detect a short?

    adcValue -= calibration;
    // Voltage across thermocouple
    float vTC = (float) adcValue/QUANTIZATION*INTERVAL_VOLTAGE_REF/GAIN;
    // Voltage across cold-junction
    float vCJ = internalTemp*cj_delta;
    // return temperature
    return (vTC + vCJ)/delta;
}

// Helper function to test if device has new data.
static bool isDataReady(ADS1120Device *dev)
{
    // If low , data is ready, else something is wrong.
    return !stmGetGpio(dev->drdy);
}

//static void powerDown(SPI_HandleTypeDef *hspi);     // Enter power-down mode (not implemented)
// Reset the device
static HAL_StatusTypeDef reset(ADS1120Device *dev)
{
    ADS1120Cmd cmd = { .byte = 0b00000110 };

    return HAL_SPI_Transmit(dev->hspi, &cmd.byte, 1, ADS_TIMEOUT);
}

// Start or restart conversions.
// Not used since writing registers starts a new conversion.
static HAL_StatusTypeDef ADCSync(ADS1120Device *dev)
{
    ADS1120Cmd cmd = { .byte = 0b00001000 };
    return HAL_SPI_Transmit(dev->hspi, &cmd.byte, 1, ADS_TIMEOUT);
}

// Read data by command
static HAL_StatusTypeDef readADC(ADS1120Device *dev, uint16_t* value)
{
    ADS1120Cmd cmd = { .rwcmd = 1, .regBegin = 0, .count = 0 };
    if (!isDataReady(dev))
        return HAL_ERROR; // No data from chip.

    uint8_t rxData[4] = { 0 };
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
    return HAL_SPI_Receive(dev->hspi, regs->regs, sizeof(regs->regs), ADS_TIMEOUT);
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

static int setInput(ADS1120Device *dev, ADS1120_input selectedInput, bool verify)
{
    // A default configuration
    ADS1120_RegConfig cfg =
    {
            .pga_bypass = 0, // large gain should be used so this is invalid
            .gain       = 5, // Multiply input by 16.
            .mux        = 0, // This is altered below.
            .bcs        = 0, // Burn-out current sensor not used

            .ts         = 0, // Set to one if internal temp should be measured.
            .cm         = 0, // Single shot
            .mode       = 0, // Normal Mode
            .dr         = 0, // 20HZ, see not in filter below.

            .idac       = 0, // 0, since external voltage supply is used.
            .psw        = 0, // Our application is not a power bridge.
            .fir_filter = 1, // Filter on, used for 20 SPS and 5SPS and we are running at 20 HZ
            .vref       = 0, // use internal Analog supply.

            .nop        = 0,
            .drdym      = 0, // The dedicated DRDY pin is used to indicate when data are ready
            .i1mux      = 0, // not routed, since idac is 0
            .i2mux      = 0, // not routed, since idac is 0
    };

    // Alter config to match the selected type.
    switch(selectedInput)
    {
    case INPUT_TEMP:
        cfg.ts = 1; // temperature mode.
        // no need to set mux/gain since ts is set.
        break;
    case INPUT_CHA:
        // all default.
        break;
    case INPUT_CHB:
        cfg.mux = 5;
        break;
    case INPUT_CALIBRATE:
        cfg.mux = 0x0E;
        break;
    }

    dev->data.readStart = 0;
    dev->data.currentInput = selectedInput;

    if (writeRegister(dev, &cfg, 4, 0) != HAL_OK) {
        return -1;
    }

    if (!verify) {
        dev->data.readStart = HAL_GetTick();
        dev->data.currentInput = selectedInput;
        return 0;
    }

    // Do not set the internal settings since reading registers will stop the reading.
    // Read back the configuration.
    ADS1120_RegConfig test;
    bzero(&test, sizeof(test));
    if (readRegisters(dev, &test) != HAL_OK) {
        return -2;
    }

    if (memcmp(test.regs, cfg.regs, 4) != 0) {
        return -3;
    }

    return 0;
}

// Public functions begin.
int ADS1120Init(ADS1120Device *dev)
{
    int ret = 0;
    memset(&dev->data, 0, sizeof(dev->data));

    stmSetGpio(dev->cs, false);
    if (reset(dev) != HAL_OK)
    {
        ret = 1;
    }
    else
    {
        HAL_Delay(1); // wait 50µs+32*t(CLK) < 1mSec after reset

        // Check that the configuration can be set.
        ret = setInput(dev, INPUT_CALIBRATE, true);
        if (ret != 0) {
            ret = 2;
        }
    }

    // Release device
    stmSetGpio(dev->cs, true);
    return ret;
}

void ADS1120Loop(ADS1120Device *dev, float *type_calibration)
{
    const int mAvgTime = 6; // Moving average time, see https://en.wikipedia.org/wiki/Moving_average.

    ADS1120Data* data = &dev->data;

    // current setting is lowest sample rate, 1/20 Sec. Timeout is 70mSec.
    if (tdiff_u32(HAL_GetTick(), data->readStart) > 70)
    {
        // Something is wrong. Restart from calibrate
        stmSetGpio(dev->cs, false);
        setInput(dev, INPUT_CALIBRATE, false);
        ADCSync(dev); // If no change in SPI flags a new ADC acquire is not started.
        stmSetGpio(dev->cs, true);

        // TODO: How to invalidate?
        return;
    }

    if (!isDataReady(dev))
    {
        // Data is not ready. This is OK since SPI device needs to acquire the data
        return;
    }

    // Get the new value present in the SPI device.
    stmSetGpio(dev->cs, false);

    uint16_t adcValue;
    if (readADC(dev, &adcValue) != HAL_OK)
    {
        // Something is wrong. Restart from calibrate
        setInput(dev, INPUT_CALIBRATE, false);
        ADCSync(dev); // If no change in SPI flags a new ADC acquire is not started.
    }
    else
    {
        // The smallest possible State machine
        double temp;
        switch(data->currentInput)
        {
        case INPUT_TEMP:
            data->internalTemp += (((int16_t) adcValue)/4.0 * 0.03125 - data->internalTemp)/mAvgTime;
            break;

        case INPUT_CHA:
            temp = adc2Temp(((int16_t) adcValue), data->calibration, data->internalTemp, *type_calibration, *(type_calibration + 1));
            if (temp == 10000 || data->chA == 10000)
                data->chA = temp;
            else
                data->chA += (temp - data->chA)/mAvgTime;
            break;

        case INPUT_CHB:
            temp = adc2Temp(((int16_t) adcValue), data->calibration, data->internalTemp, *type_calibration, *(type_calibration + 1));
            if (temp == 10000 || data->chB == 10000)
                data->chB = temp;
            else
                data->chB += (temp - data->chB)/mAvgTime;
            break;

        case INPUT_CALIBRATE:
            // The offset value from ADC1120 can be either negative or positive.
            data->calibration += (((int16_t) adcValue) - data->calibration) / mAvgTime;
            break;
        }
        setInput(dev, nextInput(data->currentInput), false);
    }

    stmSetGpio(dev->cs, true);
}
