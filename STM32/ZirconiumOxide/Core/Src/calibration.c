/*
 * calibration.c
 *
 *  Created on: Oct 18, 2021
 *      Author: matias
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <main.h> // needed for GPIO pins.
#include "calibration.h"
#include "USBprint.h"
#include "FLASH_readwrite.h"
#include "AD5227x.h"
#include "activeHeatCtrl.h"

static CalibrationInfo calibrationInfo[NO_SENSOR_CHANNELS] =
{
        { false, 0.0, false },
        { false, 0.0, false }
};

static PortCfg portCfgs[NO_SENSOR_CHANNELS] =
{
    { DEFAULT_SENSORV_GAIN_HIGH, HIGH_RANGE_BIAS, HIGH_RANGE_SCALAR, HIGH_RANGE_QUAD },
    { DEFAULT_SENSORV_GAIN_LOW, LOW_RANGE_BIAS, LOW_RANGE_SCALAR, 0 }
};

// Used for adjusting output gain of sensor which is sensor dependent and
// should be adjusted during calibration procedure
PinGainCtrl pinGainCtrl[NO_SENSOR_CHANNELS] =
{
        { CLK_GPIO_Port, CLK_Pin, UD_Pin, CS1_2_GPIO_Port, CS1_2_Pin, &portCfgs[0] },
        { CLK_GPIO_Port, CLK_Pin, UD_Pin, CS2_2_GPIO_Port, CS2_2_Pin, &portCfgs[1] }
};

// Variables that need to be stored in flash memory.
typedef struct FlashCalibration
{
    PortCfg p1;
    PortCfg p2;
    float Rtargets[2];
} FlashCalibration;
FlashCalibration cal;

static void initSensorGains(const PinGainCtrl * pin)
{
    initGain(pin);
}

int initiateCalibration(int channel)
{
    return calibrationInfo[channel].calibrate;
}

void calibrateSensor(int noOfCalibrations, const CACalibration* calibrations)
{
    if (noOfCalibrations != 1)
        return;

    const int channel = calibrations[0].port - 1;

    // Store calibration target and put sensor into calibration mode
    calibrationInfo[channel].target = calibrations[0].alpha;
    calibrationInfo[channel].calibrate = true;
}

void updateSlopeDev(ZrO2Device * dev, int channel)
{
    PinGainCtrl *pin = &pinGainCtrl[channel];
    pin->cfg->bias		 =   dev->ADCToOxygenBias;
    pin->cfg->scalar 	 =   dev->ADCToOxygenScalar;
    pin->cfg->quadrant   =   dev->ADCToOxygenQuad;
}

void calibrateToTarget(ZrO2Device * dev, int channel, double oxygenLevel)
{
    // Calibrate output gain to become 2 percent within target to exploit
    // dynamic range of sensors and ensure the ADC does not max out at
    // high levels. After approaching level adjust to true oxygen level (target).
    if (channel < 0 || channel >= NO_SENSOR_CHANNELS)
        return;

    float tol = dev->maxLevel*0.01;
    if (fabs(calibrationInfo[channel].target - oxygenLevel) > tol || calibrationInfo[channel].calibrationErr)
    {
        calibratePort(&calibrationInfo[channel], oxygenLevel, channel);
    }
    else
    {
        calibrateSensorSlope(dev, oxygenLevel, calibrationInfo[channel].target);
        updateSlopeDev(dev, channel);
        calibrationInfo[channel].calibrate = false;
        calibrationInfo[channel].calibrationErr = false;
    }
}

void useStandardCalibration(ZrO2Device * dev1, ZrO2Device * dev2)
{
    ZrO2Device * devs[2] = {dev1, dev2};

    for (int i=0; i<NO_SENSOR_CHANNELS; i++)
    {
        if (devs[i]->type == LOW_RANGE)
        {
            portCfgs[i].gain     = DEFAULT_SENSORV_GAIN_LOW;
            portCfgs[i].bias  	 =	 LOW_RANGE_BIAS;
            portCfgs[i].scalar 	 =   LOW_RANGE_SCALAR;
            portCfgs[i].quadrant =   LOW_RANGE_QUAD;
        }
        else
        {
            portCfgs[i].gain     = DEFAULT_SENSORV_GAIN_HIGH;
            portCfgs[i].bias  	 =	 HIGH_RANGE_BIAS;
            portCfgs[i].scalar 	 =   HIGH_RANGE_SCALAR;
            portCfgs[i].quadrant =   HIGH_RANGE_QUAD;
        }
    }
}

void readInFlashCalibration()
{
    portCfgs[0] = cal.p1;
    portCfgs[1] = cal.p2;

    updateRTarget(0, cal.Rtargets[0]);
    updateRTarget(1, cal.Rtargets[1]);
}

CRC_HandleTypeDef* hcrc_ = NULL;
void initCalibration(ZrO2Device * dev1, ZrO2Device * dev2, CRC_HandleTypeDef* hcrc)
{
    hcrc_=hcrc;
    ZrO2Device * devs[2] = {dev1, dev2};

    for (int i = 0; i<NO_SENSOR_CHANNELS; i++)
        HAL_GPIO_WritePin(pinGainCtrl[i].cs_blk, pinGainCtrl[i].cs_pin, GPIO_PIN_SET);

    // This reading is done directly into the portCfgs struct, not via the pins
    if (readFromFlashCRC(hcrc_, (uint32_t) FLASH_ADDR_CAL, (uint8_t*) &cal, sizeof(cal)) != 0)
    {
        useStandardCalibration(dev1, dev2);
    }
    else
    {
        readInFlashCalibration();
    }

    for (int i = 0; i < NO_SENSOR_CHANNELS; i++)
        initSensorGains(&pinGainCtrl[i]);

    // Update devices with calibration
    for (int i = 0; i < NO_SENSOR_CHANNELS; i++)
    {
        PinGainCtrl *pin = &pinGainCtrl[i];
        devs[i]->ADCToOxygenBias		= pin->cfg->bias;
        devs[i]->ADCToOxygenScalar		= pin->cfg->scalar;
        devs[i]->ADCToOxygenQuad		= pin->cfg->quadrant;
    }
}

// Manual calibration.
void calibrateManual(ZrO2Device * dev, int channel, int gain, float bias, float scalar, float quadrant)
{
    if (channel < 0 || channel >= NO_SENSOR_CHANNELS)
    {
        USBnprintf("Invalid channel");
        return;
    }

    if (gain < 0 || gain >= MAX_GAIN)
    {
        USBnprintf("Invalid gain");
        return;
    }

    // Update gain
    PinGainCtrl *pin = &pinGainCtrl[channel];
    int currentGain = pin->cfg->gain;
    pin->cfg->gain = gain;
    setGain(pin, currentGain);

    // Update sensor function
    dev->ADCToOxygenBias	= pin->cfg->bias 	 = bias;
    dev->ADCToOxygenScalar	= pin->cfg->scalar	 = scalar;
    dev->ADCToOxygenQuad	= pin->cfg->quadrant = quadrant;
}

// Automatic calibration
void calibratePort(CalibrationInfo *calibration, double currentLevel, int channel)
{
    if (channel < 0 || channel >= NO_SENSOR_CHANNELS)
    {
        calibration->calibrationErr = false;
        return;
    }

    // Adjust gain of
    PinGainCtrl *pin = &pinGainCtrl[channel];
    int currentGain = pin->cfg->gain;

    pin->cfg->gain = ((calibration->target - currentLevel) > 0) ? pin->cfg->gain - 1 : pin->cfg->gain + 1;

    if (pin->cfg->gain <= MAX_GAIN && pin->cfg->gain > 0)
    {
        setGain(pin, currentGain);
        calibration->calibrationErr = false;
    }
    else
    {
        pin->cfg->gain = currentGain;
        calibration->calibrationErr = true;
    }
}

// Update struct with calibration info before saving to flash memory.
void updateCalibrationInfo()
{
    cal.p1 = portCfgs[0];
    cal.p2 = portCfgs[1];

    float Rtarget[2];
    getRTarget(&Rtarget[0], &Rtarget[1]);

    cal.Rtargets[0] = Rtarget[0];
    cal.Rtargets[1] = Rtarget[1];
}

void calibrationReadWrite(bool write)
{
    if (write)
    {
        if (writeToFlashCRC(hcrc_, (uint32_t) FLASH_ADDR_CAL, (uint8_t *) &cal, sizeof(cal)) != 0)
        {
            USBnprintf("Calibration was not stored in FLASH");
        }
    }
    else
    {
        char buf[150];
        int len = 0;
        len += snprintf(&buf[len], sizeof(buf)- len, "Calibration: CAL");

        for (int ch=0; ch<NO_SENSOR_CHANNELS; ch++)
        {
            const PortCfg *cfgGain = pinGainCtrl[ch].cfg;
            len += snprintf(&buf[len], sizeof(buf)- len, " %d,%d,%.2f,%.6f,%.5f", ch+1, cfgGain->gain, cfgGain->bias, cfgGain->scalar, cfgGain->quadrant);
        }

        float Rtarget[2];
        getRTarget(&Rtarget[0], &Rtarget[1]);
        len += snprintf(&buf[len], sizeof(buf)- len, " %.2f,%.2f", Rtarget[0], Rtarget[1]);
        len += snprintf(&buf[len], sizeof(buf) - len, "\r\n");
        writeUSB(buf, len);
    }
}


