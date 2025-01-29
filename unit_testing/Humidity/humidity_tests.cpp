/*!
** @file   humidity_tests.cpp
** @author Matias
** @date   08/01/2025
*/

#include <cmath>
#include <cstdint>
#include <vector>

extern "C" {
    uint32_t _FlashAddrCal = 0;
}

#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_stm32xxxx_hal.h"
#include "fake_USBprint.h"
#include "fake_StmGpio.h"

/* Real supporting units */
#include "stm32f4xx_hal_gpio.c"
#include "CAProtocol.c"
#include "CAProtocolStm.c"
#include "sht45.c"
#include "crc.c"

#define __NOP() do { continue; } while (0)

/* UUT */
#include "humidityApp.c"


using ::testing::Contains;
using ::testing::ElementsAre;
using namespace std;

class mockSht45 : public stm32I2cTestDevice {
    public:
        mockSht45(uint32_t serial, I2C_TypeDef* _bus) {
            _serial = serial;
            addr = 0x44 << 1;
            bus = _bus;
        }

        void setTemp(uint16_t temp) {
            _temp = temp;
        }

        void setHumidity(uint16_t humidity) {
            _humid = humidity;
        }

        HAL_StatusTypeDef transmit(uint8_t* buf, uint8_t size) {
            _mode = buf[0];
            return HAL_OK;
        }
        
        HAL_StatusTypeDef recv(uint8_t* buf, uint8_t size) {
            if(size >= 6) {
                if(_mode == SHT4X_READ_SERIAL) {
                    buf[0] = (_serial >> 24) & 0xFFU;
                    buf[1] = (_serial >> 16) & 0xFFU;
                    buf[3] = (_serial >>  8) & 0xFFU;
                    buf[4] = (_serial >>  0) & 0xFFU;

                }
                else {
                    buf[0] = (_temp >>  8) & 0xFFU;
                    buf[1] = (_temp >>  0) & 0xFFU;
                    buf[3] = (_humid >> 8) & 0xFFU;
                    buf[4] = (_humid >> 0) & 0xFFU;
                }
                
                initCrc8(CRC_INIT, CRC_POLY);
                buf[2] = crc8Calculate(buf + 0, 2);
                buf[5] = crc8Calculate(buf + 3, 2);
            }

            return HAL_OK;
        }

    private:
        uint32_t _serial;
        uint8_t  _mode;
        uint16_t _temp;
        uint16_t _humid;
};

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class HumidityUnitTest: public CaBoardUnitTest
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        HumidityUnitTest() : CaBoardUnitTest(&LoopHumidity, HumidityChip, {LATEST_MAJOR, LATEST_MINOR}) {
            humiditySensor1 = new mockSht45(0x1234789A, I2C1);
            humiditySensor2 = new mockSht45(0xBCDEF012, I2C2);
        }


        void simTick()
        {
            // For every update in print out the main loop must be called 3 times for the humidity to be updated
            // through the state machine
            for (int i = 0; i < 3; i++)
            {
                LoopHumidity(bootMsg);
            }

            if(tickCounter != 0 && (tickCounter % 100 == 0)) {
                HAL_TIM_PeriodElapsedCallback(&printTim);
            }
        }
        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/
        
        WWDG_HandleTypeDef wwdg;
        I2C_HandleTypeDef i2cHumidity2 = {
            .Instance = I2C2
        };
        I2C_HandleTypeDef i2cHumidity1 = {
            .Instance = I2C1
        };
        TIM_HandleTypeDef printTim = {
            .Instance = TIM2
        };

        mockSht45* humiditySensor1;
        mockSht45* humiditySensor2;

        SerialStatusTest sst = {
            .boundInit = bind(InitHumidity, &i2cHumidity1, &i2cHumidity2, &wwdg),
            .testFixture = this
        };
};


/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(HumidityUnitTest, goldenPath) {
    fakeHAL_I2C_addDevice(humiditySensor1);
    humiditySensor1->setTemp(0x8000);
    humiditySensor1->setHumidity(0x8000);

    fakeHAL_I2C_addDevice(humiditySensor2);
    humiditySensor2->setTemp(0x8000);
    humiditySensor2->setHumidity(0x8000);

    sst.boundInit();
    goldenPathTest(sst, "42.50, 56.50, 32.76, 42.50, 56.50, 32.76, 0x00000000");
}

TEST_F(HumidityUnitTest, incorrectBoard) {
    incorrectBoardTest(sst);
}

TEST_F(HumidityUnitTest, printStatus) {
    fakeHAL_I2C_addDevice(humiditySensor1);
    fakeHAL_I2C_addDevice(humiditySensor2);

    statusPrintoutTest(sst, {
        "Serial number sensor 0: 0x1234789a.\r", 
        "Serial number sensor 1: 0xbcdef012.\r", 
        "Humidity sensor 0 is operating normally.\r", 
        "Humidity sensor 1 is operating normally.\r",
    });
}

TEST_F(HumidityUnitTest, printSerial) {
    serialPrintoutTest(sst, "HumidityChip");
}
