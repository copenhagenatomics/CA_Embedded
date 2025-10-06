/*!
** @file   analog_input_tests.cpp
** @author Matias
** @date   02/05/2024
*/

#include <inttypes.h>
#include <cstdio>

extern "C" {
    uint32_t _FlashAddrCal = 0;
    uint32_t _FlashAddrUptime = 0;
}

#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_stm32xxxx_hal.h"
#include "fake_StmGpio.h"
#include "fake_USBprint.h"

/* Real supporting units */
#include "CAProtocol.c"
#include "CAProtocolStm.c"
#include "calibration.c"
#include "ADCmonitor.c"
#include "MCP4531.c"
#include "uptime.c"

/* UUT */
#include "analog_input.c"

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsSupersetOf;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

/* Mock I2C digital potentiometer for testing */
class mockMcp4531 : public stm32I2cTestDevice {
    public:
        mockMcp4531(I2C_TypeDef* _bus, uint8_t _addr) {
            addr = (0x28 | _addr) << 1;
            bus = _bus;
        }

        void setError(HAL_StatusTypeDef err) {
            _error = err;
        }

        /* Get the address to operate on and the command */
        HAL_StatusTypeDef transmit(uint8_t* buf, uint8_t size) {
            _reg_addr = (buf[0] >> 4) & 0x0FU;
            _cmd      = buf[0] & 0x0CU;

            if(_cmd == MCP4X_WRITE_CMD) {
                if( _reg_addr == MCP4X_WIPER_0_ADDR) {
                    wipers[0] = ((uint16_t)(buf[0] & 0x01)) << 8U | buf[1];
                }
                else if( _reg_addr == MCP4X_WIPER_1_ADDR) {
                    wipers[1] = ((uint16_t)(buf[0] & 0x01)) << 8U | buf[1];
                }
            }

            return _error;
        }
        
        HAL_StatusTypeDef recv(uint8_t* buf, uint8_t size) {
            if(size >= 2) {
                if(_reg_addr == MCP4X_WIPER_0_ADDR) {
                    buf[0] = (wipers[0] & 0x100) >> 8U;
                    buf[1] = (wipers[0] & 0x0FF);
                }
                else if(_reg_addr == MCP4X_WIPER_1_ADDR) {
                    buf[0] = (wipers[1] & 0x100) >> 8U;
                    buf[1] = (wipers[1] & 0x0FF);
                }
                else if (_reg_addr == MCP4X_STATUS_ADDR) {
                    buf[0] = 0x01U;
                    buf[1] = 0xF1U;
                }
            }

            return HAL_OK;
        }

        /* Wipers to verify data has been written correctly */
        uint16_t wipers[2U] = {64, 64};

    private:
        uint32_t _serial;
        HAL_StatusTypeDef _error = HAL_OK;
        uint8_t _reg_addr;
        uint8_t _cmd;
};

class AnalogInputTest: public CaBoardUnitTest
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        AnalogInputTest() : CaBoardUnitTest(&analogInputLoop, AnalogInput, {LATEST_MAJOR, LATEST_MINOR}) {
            hadc.Init.NbrOfConversion = 8;

            /* Add virtual potentiometers */
            for(int i = 0; i < NO_CALIBRATION_CHANNELS; i++) {
                digipots[i] = new mockMcp4531(I2C1, i);
                fakeHAL_I2C_addDevice(digipots[i]);
            }

            /* Most tests assume the main voltage rails are OK */
            sst.boundInit();
            setAdcChannelBuffer(ADC_CHANNEL_28V,  ADC_MAX * 28.0 / MAX_28V_IN);
            setAdcChannelBuffer(ADC_CHANNEL_VBUS, ADC_MAX * 5.0 / MAX_VBUS_IN);
        }

        void simTick() {
            if(tickCounter != 0 && (tickCounter % 100 == 0)) {
                if(tickCounter % 200 == 0) {
                    HAL_ADC_ConvCpltCallback(&hadc);
                }
                else {
                    HAL_ADC_ConvHalfCpltCallback(&hadc);
                }
            }
            analogInputLoop(bootMsg);
        }

        void setAdcChannelBuffer(int channel, int value) {
            /* Fill channel with ADC value */
            for(uint32_t i = 0; i < hadc.dma_length/ADC_CHANNELS; i++) {
                *((int16_t*)hadc.dma_address + (ADC_CHANNELS*i + channel)) = value;
            }
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/

        ADC_HandleTypeDef hadc;
        CRC_HandleTypeDef hcrc;
        I2C_HandleTypeDef hi2c = {
            .Instance = I2C1
        };

        mockMcp4531* digipots[NO_CALIBRATION_CHANNELS] = {0};

        SerialStatusTest sst = {
            .boundInit = bind(analogInputInit, &hadc, &hcrc, &hi2c, bootMsg),
            .testFixture = this
        };
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(AnalogInputTest, testanalogInputInitCorrectParams) {
    goldenPathTest(sst, "0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0x00000000", 200);
}

TEST_F(AnalogInputTest, testanalogInputInitWrongBoard) {
    incorrectBoardTest(sst);
}

TEST_F(AnalogInputTest, testanalogInputInitWrongSwVersion) {
    incorrectBoardVersionTest(sst);
}

TEST_F(AnalogInputTest, testAnalogInputStatus) {
    setAdcChannelBuffer(ADC_CHANNEL_28V,  0);
    setAdcChannelBuffer(ADC_CHANNEL_VBUS, 0);
    simTicks(100);

    statusPrintoutTest(sst, {
        "Under voltage. The board operates at too low voltage of 0.00V. Check power supply.\r", 
        "Port 1 measures voltage [0-5V]\r", 
        "Port 2 measures voltage [0-5V]\r", 
        "Port 3 measures voltage [0-5V]\r", 
        "Port 4 measures voltage [0-5V]\r", 
        "Port 5 measures voltage [0-5V]\r", 
        "Port 6 measures voltage [0-5V]\r", 
        "VCC_28V is: 0.00. It should be >=26.00V \r", 
        "VBUS is: 0.00. It should be >=4.6V \r", 
    });

    setAdcChannelBuffer(ADC_CHANNEL_28V,  ADC_MAX * 28.0 / MAX_28V_IN);
    setAdcChannelBuffer(ADC_CHANNEL_VBUS, ADC_MAX * 5.0 / MAX_VBUS_IN);
    simTicks(100);
    
    statusPrintoutTest(sst, {
        "The board is operating normally.\r",
        "Port 1 measures voltage [0-5V]\r", 
        "Port 2 measures voltage [0-5V]\r", 
        "Port 3 measures voltage [0-5V]\r", 
        "Port 4 measures voltage [0-5V]\r", 
        "Port 5 measures voltage [0-5V]\r", 
        "Port 6 measures voltage [0-5V]\r", 
    });

    int port = 1;
    int measureCurrent = 1;
    // Update port one to measure current rather than voltage.
    const CACalibration calibration[1] = {port, 0.00188, -1.79, measureCurrent};
    calibrateSensorOrBoard(1, calibration);

    statusPrintoutTest(sst, {
        "The board is operating normally.\r",
        "Port 1 measures current [4-20mA]\r", 
        "Port 2 measures voltage [0-5V]\r", 
        "Port 3 measures voltage [0-5V]\r", 
        "Port 4 measures voltage [0-5V]\r", 
        "Port 5 measures voltage [0-5V]\r", 
        "Port 6 measures voltage [0-5V]\r", 
    });
}

TEST_F(AnalogInputTest, testAnalogInputStatusDef) {
    statusDefPrintoutTest(sst, "0x7e003fc0,System errors\r", {
        "0x00002000,Error I2C Channel 6\r", 
        "0x00001000,Error I2C Channel 5\r", 
        "0x00000800,Error I2C Channel 4\r", 
        "0x00000400,Error I2C Channel 3\r", 
        "0x00000200,Error I2C Channel 2\r", 
        "0x00000100,Error I2C Channel 1\r", 
        "0x00000080,VBUS Error\r", 
        "0x00000040,VCC_28V Error\r",
        "0x00000020,Port 6 measure type [Voltage/Current]\r",
        "0x00000010,Port 5 measure type [Voltage/Current]\r",
        "0x00000008,Port 4 measure type [Voltage/Current]\r",
        "0x00000004,Port 3 measure type [Voltage/Current]\r",
        "0x00000002,Port 2 measure type [Voltage/Current]\r",
        "0x00000001,Port 1 measure type [Voltage/Current]\r"
    });
}

TEST_F(AnalogInputTest, testAnalogInputSerial) {
    serialPrintoutTest(sst, "AnalogInput", 
        "Calibration: CAL 1,1.0000000000,0.0000000000,0 2,1.0000000000,0.0000000000,0 3,1.0000000000,0.0000000000,0 4,1.0000000000,0.0000000000,0 5,1.0000000000,0.0000000000,0 6,1.0000000000,0.0000000000,0\r");
}

TEST_F(AnalogInputTest, testAnalogInputUptime) {
    uptimeTest(sst, (uintptr_t)&_FlashAddrUptime);
}

TEST_F(AnalogInputTest, testAnalogInputOutputVoltage) {
    /* First write flushes USB, so always sim 1 tick first */
    simTicks(1);

    for(int i = 0; i < NO_CALIBRATION_CHANNELS; i++) {
        /* Calculated value for 5.1V */
        EXPECT_EQ(digipots[i]->wipers[1], 26) << "Channel " << i;
    }

    for(int i = 0; i < NO_CALIBRATION_CHANNELS; i++) {
        char buf[100] = {0};
        sprintf(buf, "p%d volt 10.1\n", (i+1));
        writeBoardMessage(buf);
        
        /* Board should ramp to the new voltage (10.1) over the course of n cycles */
        for(int j = 1; j < 25; j++) {
            EXPECT_EQ(digipots[i]->wipers[1], 26+j)  << "Channel " << i;
            simTicks(1);
        }

        /* Calculated value for 10.1V */
        EXPECT_EQ(digipots[i]->wipers[1], 51)  << "Channel " << i;

        /* Remaining potentiometers should not have been changed */
        for(int j = i + 1; j < NO_CALIBRATION_CHANNELS; j++) {
            EXPECT_EQ(digipots[j]->wipers[1], 26)  << "Channel " << j;
        }
    }
}

TEST_F(AnalogInputTest, testAnalogInputMeasurementRange) {
    /* Set all channels to mid-rail */
    for(int i = 0; i < NO_CALIBRATION_CHANNELS; i++) {
        setAdcChannelBuffer(i, (4095/2));
    }

    /* First write flushes USB, so always sim 1 tick first */
    simTicks(100);

    /* By default, range should be 5.1 and calibration should be 1.0, so output should be ~2.610V */
    /* First two lines are boot messages */
    vector<string> lines = hostUSBread(true);
    vector<string> channels = getChannelsFromLine(lines[2]);

    for(int i = 0; i < NO_CALIBRATION_CHANNELS; i++) {
        double channel = stod(channels[i]);
        EXPECT_NEAR(channel, 2.610, 1E-3);
    }

    /* Adjust the the voltage to 10.1V range, but keep the ADC values mid rail. The voltage should 
    ** convert to 5.123 */
    writeBoardMessage("p1 inmax 10.1\n");

    for(int i = 1; i < 26; i++) {
        EXPECT_EQ(digipots[0]->wipers[0], 27 + i);
        simTicks(1);
    }
    EXPECT_EQ(digipots[0]->wipers[0], 53);
    simTicks(75); /* Resynchronise with 10 Hz output */
    lines = hostUSBread(true);
    channels = getChannelsFromLine(lines[1]);

    EXPECT_NEAR(stod(channels[0]), 5.123, 1E-3);
    for(int i = 1; i < NO_CALIBRATION_CHANNELS; i++) {
        double channel = stod(channels[i]);
        EXPECT_NEAR(channel, 2.610, 1E-3);
    }

    /* Reduce the ADC voltage back down to the ~2.5 range */
    setAdcChannelBuffer(0, 1043);
    simTicks(100);
    lines = hostUSBread(true);
    channels = getChannelsFromLine(lines[1]);

    for(int i = 0; i < NO_CALIBRATION_CHANNELS; i++) {
        double channel = stod(channels[i]);
        EXPECT_NEAR(channel, 2.610, 1E-3);
    }
}