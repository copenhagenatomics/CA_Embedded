/*!
** @file   pressure_calibration_tests.cpp
** @author Matias
** @date   02/05/2024
*/

#include <inttypes.h>

extern "C" {
    uint32_t _FlashAddrCal = 0;
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

/* UUT */
#include "pressure.c"

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsSupersetOf;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class PressureTest: public CaBoardUnitTest
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        PressureTest() : CaBoardUnitTest(&pressureLoop, Pressure, {LATEST_MAJOR, LATEST_MINOR}) {
            hadc.Init.NbrOfConversion = 8;
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
            pressureLoop(bootMsg);
        }

        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/

        ADC_HandleTypeDef hadc;
        CRC_HandleTypeDef hcrc;

        SerialStatusTest sst = {
            .boundInit = bind(pressureInit, &hadc, &hcrc),
            .testFixture = this
        };
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(PressureTest, testPressureInitCorrectParams) {
    pressureInit(&hadc, &hcrc);

    /* Basic test, was everything OK?  */
    EXPECT_FALSE(bsGetStatus() && BS_VERSION_ERROR_Msk);

    /* This should force a print on the USB bus */
    pressureLoop(bootMsg);
    HAL_ADC_ConvCpltCallback(&hadc);
    pressureLoop(bootMsg);

    /* Check the printout is correct */
    EXPECT_READ_USB(Contains("-1.790000, -1.790000, -1.790000, -1.790000, -1.790000, -1.790000, 0xa0000180"));
}

TEST_F(PressureTest, testPressureInitWrongBoard)
{
    bi.v2.boardType = AC_Board; 
    HAL_otpWrite(&bi);
    pressureInit(&hadc, &hcrc);
    /* Wrong board type expect version error  */
    EXPECT_TRUE(bsGetStatus() && BS_VERSION_ERROR_Msk);

    /* This should force a print on the USB bus */
    pressureLoop(bootMsg);
    HAL_ADC_ConvCpltCallback(&hadc);
    pressureLoop(bootMsg);
    EXPECT_READ_USB(Contains("0xa4000180"));
}

TEST_F(PressureTest, testPressureInitWrongSwVersion)
{
    bi.v2.pcbVersion.minor = 1; 
    HAL_otpWrite(&bi);
    pressureInit(&hadc, &hcrc);
    /* Wrong sw version type expect version error  */
    EXPECT_TRUE(bsGetStatus() && BS_VERSION_ERROR_Msk);

    /* This should force a print on the USB bus */
    pressureLoop(bootMsg);
    HAL_ADC_ConvCpltCallback(&hadc);
    pressureLoop(bootMsg);
    EXPECT_READ_USB(Contains("0xa4000180"));
}

TEST_F(PressureTest, testPressureStatus)
{
    pressureInit(&hadc, &hcrc);

    /* This should force a print on the USB bus */
    pressureLoop(bootMsg);
    writeBoardMessage("Status\n");

    EXPECT_FLUSH_USB(ElementsAre(
         "\r", 
         "Boot Unit Test\r", 
         "Start of board status:\r", 
         "Under voltage. The board operates at too low voltage of 0.00V. Check power supply.\r", 
         "Port 1 measures voltage [0-5V]\r", 
         "Port 2 measures voltage [0-5V]\r", 
         "Port 3 measures voltage [0-5V]\r", 
         "Port 4 measures voltage [0-5V]\r", 
         "Port 5 measures voltage [0-5V]\r", 
         "Port 6 measures voltage [0-5V]\r", 
         "VCC is: 0.00. It should be >=5.05V \r", 
         "VCC raw is: 0.00. It should be >=4.6V \r", 
         "\r", 
         "End of board status. \r"
    ));

    /* Input ADC measurements that will yield a valid input VCC */
    for (int i = 0; i < ADC_CHANNELS*ADC_CHANNEL_BUF_SIZE*2; i++)
    {
        ADCBuffer[i] = 4000.0;
    }

    // Go to tick that will call the adcCallBack function
    goToTick(100);
    pressureLoop(bootMsg);
    writeBoardMessage("Status\n");
    
    EXPECT_FLUSH_USB(IsSupersetOf({
         "Start of board status:\r", 
         "The board is operating normally.\r",
         "Port 1 measures voltage [0-5V]\r", 
         "Port 2 measures voltage [0-5V]\r", 
         "Port 3 measures voltage [0-5V]\r", 
         "Port 4 measures voltage [0-5V]\r", 
         "Port 5 measures voltage [0-5V]\r", 
         "Port 6 measures voltage [0-5V]\r", 
         "\r", 
         "End of board status. \r"
    }));

    int port = 1;
    int measureCurrent = 1;
    // Update port one to measure current rather than voltage.
    const CACalibration calibration[1] = {port, 0.00188, -1.79, measureCurrent};
    calibrateSensorOrBoard(1, calibration);

    pressureLoop(bootMsg);
    writeBoardMessage("Status\n");
    
    EXPECT_FLUSH_USB(IsSupersetOf({ 
        "\r", 
        "Start of board status:\r", 
        "The board is operating normally.\r", 
        "Port 1 measures current [4-20mA]\r", 
        "Port 2 measures voltage [0-5V]\r", 
        "Port 3 measures voltage [0-5V]\r", 
        "Port 4 measures voltage [0-5V]\r", 
        "Port 5 measures voltage [0-5V]\r", 
        "Port 6 measures voltage [0-5V]\r",
        "\r", 
        "End of board status. \r"
    }));
}

TEST_F(PressureTest, testPressureStatusDef)
{
    statusDefPrintoutTest(sst,
        "0x7e000180,System errors\r",
        {"0x00000100,VBUS FB\r",
        "0x00000080,5V FB\r",
        "0x00000020,Port 6 measure type [Voltage/Current]\r",
        "0x00000010,Port 5 measure type [Voltage/Current]\r",
        "0x00000008,Port 4 measure type [Voltage/Current]\r",
        "0x00000004,Port 3 measure type [Voltage/Current]\r",
        "0x00000002,Port 2 measure type [Voltage/Current]\r",
        "0x00000001,Port 1 measure type [Voltage/Current]\r"});
}

TEST_F(PressureTest, testAdcCallback)
{
    pressureInit(&hadc, &hcrc);

    for (int i = 0; i < ADC_CHANNELS*ADC_CHANNEL_BUF_SIZE*2; i++)
    {
        ADCBuffer[i] = 2068.0;
    }

    // Go to tick that will call adcCallBack function
    goToTick(100);
    pressureLoop(bootMsg);

    // Check that the ADCMeans is the mean of the ADC measurements scaled by the portCalVal
    for (int i = 0; i < ADC_CHANNELS; i++)
    {
        EXPECT_NEAR(ADCMeans[i], 2068*cal.portCalVal[i], 1e-3);
    }

    // Check that the pressures are scaled by the linear functions
    for (int i = 0; i < ADC_CHANNELS - 2; i++)
    {
        EXPECT_NEAR(pressure[i], ADCMeans[i]*VOLTAGE_SCALING*cal.sensorCalVal[i*2] + cal.sensorCalVal[i*2+1], 1e-3);
    }

    // Check that the volts are scaled using the correct VCCs
    for (int i = 0; i < ADC_CHANNELS; i++)
    {
        if (i < NO_CALIBRATION_CHANNELS)
        {
            EXPECT_NEAR(volts[i], ADCMeans[i]/(ADC_MAX+1)*MAX_VIN, 1e-3);
        }
        else
        {
            EXPECT_NEAR(volts[i], ADCMeans[i]/(ADC_MAX+1)*MAX_VCC_IN, 1e-3);
        }   
    }
}