/*!
** @file   AC_tests.cpp
** @author Luke W
** @date   12/10/2023
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "caBoardUnitTests.h"
#include "serialStatus_tests.h"

/* Fakes */
#include "fake_StmGpio.h"
#include "fake_stm32xxxx_hal.h"
#include "fake_USBprint.h"

/* Real supporting units */
#include "HeatCtrl.c"
#include "ADCmonitor.c"
#include "CAProtocol.c"
#include "CAProtocolStm.c"
#include "CAProtocolACDC.c"
#include "faultHandlers.c"

/* Prevents attempting to access non-existent linker script variables */
#define FLASH_ADDR_CAL ((uint32_t) 0U)

#include "flashHandler.c"

/* UUT */
#include "ACBoard.c"

using ::testing::AnyOf;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class ACBoard: public CaBoardUnitTest
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        ACBoard() : CaBoardUnitTest(ACBoardLoop, AC_Board, {LATEST_MAJOR, LATEST_MINOR}) {
            hadc.Init.NbrOfConversion = 8;

            /* Prevent fault info triggering automatically at startup */
            depositUnit_t tmp = {};
            tmp.fault_info.fault = NO_FAULT;
            writeToFlash(FLASH_ADDR_CAL, (uint8_t*)&tmp, sizeof(depositUnit_t));
        }

        void simTick()
        {
            if(tickCounter != 0 && (tickCounter % 100 == 0)) {
                if(tickCounter % 200 == 0) {
                    HAL_ADC_ConvCpltCallback(&hadc);
                }
                else {
                    HAL_ADC_ConvHalfCpltCallback(&hadc);
                }
            }
            ACBoardLoop(bootMsg);
        }

        void setPowerStatus(bool state)
        {
            ACBoardInit(&hadc);
            powerStatus.state = state;
            for (int i = 0; i < 1000; i++)
            {
                updateBoardStatus();
            }
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
        
        SerialStatusTest sst = {
            .boundInit = bind(ACBoardInit, &hadc),
            .testFixture = this
        };
};

/* ADC raw value that produces ~13.1A: ADCtoCurrent(1000) = 0.013138*1000 - 0.01 = 13.13A */
static const int16_t ADC_OVERCURRENT   = 1000;
/* ADC raw value that produces ~9.2A: ADCtoCurrent(700) = 0.013138*700 - 0.01 = 9.19A */
static const int16_t ADC_SAFE_CURRENT  = 700;
/* ADC raw value that produces ~12.5A: above default 12A limit, below a 15A custom limit */
static const int16_t ADC_MID_CURRENT   = 950;

/***************************************************************************************************
** TESTS
***************************************************************************************************/

TEST_F(ACBoard, CorrectBoardParams) {
    setPowerStatus(true);

    goldenPathTest(sst, "-0.0100, -0.0100, -0.0100, -0.0100, -50.00, -50.00, -50.00, -50.00, 0x00000000\r");
}

TEST_F(ACBoard, printStatus) {
    setPowerStatus(true);

    statusPrintoutTest(sst, {"The board is operating normally.\r",
                             "Fan     On: 0\r",
                             "Port 0: On: 0, PWM percent: 0\r", 
                             "Port 1: On: 0, PWM percent: 0\r", 
                             "Port 2: On: 0, PWM percent: 0\r", 
                             "Port 3: On: 0, PWM percent: 0\r",
                             "Power   On: 1\r"});

    setPowerStatus(false);
    writeBoardMessage("fan on\n");
    writeBoardMessage("p2 on 1\n");
    writeBoardMessage("p4 on 1\n");
    writeBoardMessage("Status\n");
    
    EXPECT_FLUSH_USB(ElementsAre(
        "Start of board status:\r",
        "Fan     On: 1\r",
        "Port 0: On: 0, PWM percent: 0\r",
        "Port 1: On: 1, PWM percent: 100\r",
        "Port 2: On: 0, PWM percent: 0\r",
        "Port 3: On: 1, PWM percent: 100\r",
        "Power   On: 0\r",
        "End of board status.\r"
    ));

    /* It would be nice to be easily able to test PWM too, but this would require mocking/faking 
    ** heatCtrl module */
}

TEST_F(ACBoard, printStatusDef) {
    statusDefPrintoutTest(sst, "0x7e0007a0,System errors\r",
                          {"0x00000020,Mains not-connected error\r",
                           "0x00000010,Port 4 switching state\r",
                           "0x00000008,Port 3 switching state\r",
                           "0x00000004,Port 2 switching state\r",
                           "0x00000002,Port 1 switching state\r",
                           "0x00000001,Fan state\r",
                           "0x00000080,Port 1 e-fuse overcurrent error\r",
                           "0x00000100,Port 2 e-fuse overcurrent error\r",
                           "0x00000200,Port 3 e-fuse overcurrent error\r",
                           "0x00000400,Port 4 e-fuse overcurrent error\r"});
}

TEST_F(ACBoard, printSerial) {
    serialPrintoutTest(sst, "AC Board", "Calibration: CAL 1,12.00,0,0 2,12.00,0,0 3,12.00,0,0 4,12.00,0,0\r");
}

TEST_F(ACBoard, calibrationUpdate)
{
    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);
    hostUSBread(true);

    /* Verify default limits appear in Serial response */
    writeBoardMessage("Serial\n");
    EXPECT_READ_USB(Contains("Calibration: CAL 1,12.00,0,0 2,12.00,0,0 3,12.00,0,0 4,12.00,0,0\r"));

    /* Set a custom limit on port 2 and verify it is reflected in the next Serial response */
    writeBoardMessage("CAL 2,15.0,0,0\n");
    writeBoardMessage("Serial\n");
    EXPECT_READ_USB(Contains("Calibration: CAL 1,12.00,0,0 2,15.00,0,0 3,12.00,0,0 4,12.00,0,0\r"));

    /* Invalid calibration commands must produce a MISREAD error and leave limits unchanged */
    writeBoardMessage("CAL 12,-5,0,0\n");
    EXPECT_FLUSH_USB(Contains("Invalid calibration input: 12,-5.00,0,0\r"));

    writeBoardMessage("CAL bad\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: CAL bad\r"));

    writeBoardMessage("Serial\n");
    EXPECT_READ_USB(Contains("Calibration: CAL 1,12.00,0,0 2,15.00,0,0 3,12.00,0,0 4,12.00,0,0\r"));
}

TEST_F(ACBoard, incorrectBoardParams) {
    incorrectBoardTest(sst);
}

/* Note: this test uses some knowledge of internals of ACBoard.c (e.g. white box), which isn't 
** perfect */
TEST_F(ACBoard, fanInput) 
{
    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);

    EXPECT_FALSE(isFanForceOn);
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    writeBoardMessage("fan on\n");
    EXPECT_TRUE(isFanForceOn);
    EXPECT_TRUE(stmGetGpio(fanCtrl));

    writeBoardMessage("fan off\n");
    EXPECT_FALSE(isFanForceOn);
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    writeBoardMessage("fan wrong\n");
    EXPECT_FALSE(isFanForceOn);
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    EXPECT_READ_USB(Contains("MISREAD: fan wrong\r"));
}

/* Note: this test uses some knowledge of internals of ACBoard.c (e.g. white box), which isn't 
** perfect */
TEST_F(ACBoard, GpioInit) 
{
    auto expectStmNull = [](StmGpio* stm) {
        EXPECT_EQ(stm->set,    nullptr);
        EXPECT_EQ(stm->get,    nullptr);
        EXPECT_EQ(stm->toggle, nullptr);
    };

    auto expectStmNotNull = [](StmGpio* stm) {
        EXPECT_NE(stm->set,    nullptr);
        EXPECT_NE(stm->get,    nullptr);
        EXPECT_NE(stm->toggle, nullptr);
    };

    expectStmNull(&fanCtrl);
    for(int i = 0; i < AC_BOARD_NUM_PORTS; i++) expectStmNull(&heaterPorts[i].heater);

    ACBoardInit(&hadc);

    expectStmNotNull(&fanCtrl);
    for(int i = 0; i < AC_BOARD_NUM_PORTS; i++) expectStmNotNull(&heaterPorts[i].heater);

    /* The GPIO should turn on immediately if a normal message is sent */
    EXPECT_FALSE(stmGetGpio(heaterPorts[0].heater));

    ACBoardLoop(bootMsg);
    writeBoardMessage("p1 on 1\n");
    EXPECT_TRUE(stmGetGpio(heaterPorts[0].heater));

    /* The GPIO should turn on at the next PWM cycle if a % message is sent */
    EXPECT_FALSE(stmGetGpio(heaterPorts[1].heater));
    writeBoardMessage("p2 on 2 50%\n");
    EXPECT_FALSE(stmGetGpio(heaterPorts[1].heater));
    
    /* Check PWM starts at beginning of next cycle */
    goToTick(1000);
    EXPECT_TRUE(stmGetGpio(heaterPorts[1].heater));

    /* Still on all the way to 50%? */
    goToTick(1499);
    EXPECT_TRUE(stmGetGpio(heaterPorts[1].heater));

    /* Turns off at 50% */
    goToTick(1500);
    EXPECT_FALSE(stmGetGpio(heaterPorts[1].heater));

    /* Doesn't turn on again (duration set to 2 secs at t=0) */
    goToTick(2000);
    EXPECT_FALSE(stmGetGpio(heaterPorts[1].heater));

    /* Quick test "next cycle turn on" works elsewhere within PWM period */
    goToTick(2750);
    EXPECT_FALSE(stmGetGpio(heaterPorts[2].heater));
    writeBoardMessage("p3 on 3 25%\n");
    EXPECT_FALSE(stmGetGpio(heaterPorts[2].heater));
    goToTick(2999);

    EXPECT_FALSE(stmGetGpio(heaterPorts[2].heater));
    goToTick(3000);
    EXPECT_TRUE(stmGetGpio(heaterPorts[2].heater));
}

/* Stacked switching basically checked in heatCtrl tests */

TEST_F(ACBoard, InvalidCommands)
{
    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);
    hostUSBread(true); /* Flush USB buffer */

    /* OK messages */
    writeBoardMessage("p1 on 1\n");
    EXPECT_READ_USB(IsEmpty());
    writeBoardMessage("p2 on 1\n");
    EXPECT_READ_USB(IsEmpty());
    writeBoardMessage("p3 on 1\n");
    EXPECT_READ_USB(IsEmpty());
    writeBoardMessage("p4 on 1\n");
    EXPECT_READ_USB(IsEmpty());

    /* Fail messages */
    writeBoardMessage("p1 on\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: p1 on -1\r"));
    writeBoardMessage("p2 on -1\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: p2 on -1\r"));
    writeBoardMessage("p3 on 10%\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: p3 on -1\r"));
    writeBoardMessage("p4 on 80%\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: p4 on -1\r"));
    writeBoardMessage("all on\n");
    EXPECT_FLUSH_USB(Contains("MISREAD: all on\r"));
}

TEST_F(ACBoard, UsbTimeout)
{
    static const int TEST_LENGTH_MS = 10000;
    static const int TIMEOUT_LENGTH_MS = 5000;

    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);
    writeBoardMessage("all on 60\n");

    for(int i = 0; i < TEST_LENGTH_MS / 10; i++)
    {
        if(i == 0.25 * TEST_LENGTH_MS / 10) {
            hostUSBDisconnect();
        }

        goToTick(i * 10);

        if(i < (0.25 * TEST_LENGTH_MS / 10 + TIMEOUT_LENGTH_MS / 10)) {
            ASSERT_TRUE(stmGetGpio(heaterPorts[0].heater)) << i;
            ASSERT_TRUE(stmGetGpio(heaterPorts[1].heater)) << i;
            ASSERT_TRUE(stmGetGpio(heaterPorts[2].heater)) << i;
            ASSERT_TRUE(stmGetGpio(heaterPorts[3].heater)) << i;
        }
        else {
            ASSERT_FALSE(stmGetGpio(heaterPorts[0].heater)) << i;
            ASSERT_FALSE(stmGetGpio(heaterPorts[1].heater)) << i;
            ASSERT_FALSE(stmGetGpio(heaterPorts[2].heater)) << i;
            ASSERT_FALSE(stmGetGpio(heaterPorts[3].heater)) << i;
        }
    }
}

TEST_F(ACBoard, heatsinkLoop) 
{
    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);

    setPowerStatus(true);

    EXPECT_FALSE(stmGetGpio(fanCtrl));

    const int TEMP_CHANNEL = 4;

    /* Fill the temperature buffer with ~54 degC - fan should stay off */
    for(unsigned i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + TEMP_CHANNEL + ADC_CHANNELS*i) = 1300;
    goToTick(100);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 54.78, -50.00, -50.00, -50.00, 0x00000000\r"));
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~56 degC - fan should turn on */
    for(unsigned i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + TEMP_CHANNEL +  ADC_CHANNELS*i) = 1320;
    goToTick(200);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 56.39, -50.00, -50.00, -50.00, 0x00000000\r"));
    EXPECT_TRUE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~51 degC - fan should remain on */
    /* Note: Status changes to 0x1 because fan pin is enabled. The previous message didn't contain 
    ** it because the printout and heatSink update are in the same function, and the heatsinkLoop 
    ** is outside the function */
    for(unsigned i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + TEMP_CHANNEL +  ADC_CHANNELS*i) = 1250;
    goToTick(300);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 50.75, -50.00, -50.00, -50.00, 0x00000001\r"));
    EXPECT_TRUE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~49 degC - fan should turn off */
    for(unsigned i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + TEMP_CHANNEL +  ADC_CHANNELS*i) = 1228;
    goToTick(400);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 48.98, -50.00, -50.00, -50.00, 0x00000001\r"));
    EXPECT_FALSE(stmGetGpio(fanCtrl));

    /* Fill the temperature buffer with ~71 degC - fan should turn on and PWM of heaters should be reduced */
    writeBoardMessage("p1 on 10\n");
    for(unsigned i = 0; i < hadc.dma_length / hadc.Init.NbrOfConversion; i++) *((int16_t*)hadc.dma_address + TEMP_CHANNEL +  ADC_CHANNELS*i) = 1500;
    goToTick(600);
    EXPECT_FLUSH_USB(Contains("-0.0100, -0.0100, -0.0100, -0.0100, 70.90, -50.00, -50.00, -50.00, 0xc0000003\r"));
    EXPECT_TRUE(stmGetGpio(fanCtrl));
}

TEST_F(ACBoard, efuse_tripOnOvercurrent)
{
    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);

    /* Let calibration run at tick 100 with all-zero ADC buffer → zero bias */
    simTicks(100);

    /* Set channel 0 to ~13A and turn port on */
    setAdcChannelBuffer(0, ADC_OVERCURRENT);
    writeBoardMessage("p1 on 10\n");
    EXPECT_TRUE(stmGetGpio(heaterPorts[0].heater));

    /* Run more ADC callbacks. ~13.1A exceeds the 12A limit immediately on the first callback. */
    simTicks(100);

    EXPECT_FALSE(stmGetGpio(heaterPorts[0].heater));
    uint32_t status = flushAndGetUSBStatus();
    EXPECT_TRUE(status & AC_EFUSE_OVERCURRENT_Msk(1));
    EXPECT_TRUE(status & BS_OVER_CURRENT_Msk);
    /* Other channels must not be affected */
    for(int i = 2; i <= AC_BOARD_NUM_PORTS; i++) {
        EXPECT_FALSE(status & AC_EFUSE_OVERCURRENT_Msk(i));
    }
}

TEST_F(ACBoard, efuse_noTripBelowLimit)
{
    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);

    simTicks(100); /* calibration */

    /* Set channel 0 to ~9.2A (below the 10A default limit) */
    setAdcChannelBuffer(0, ADC_SAFE_CURRENT);
    writeBoardMessage("p1 on 10\n");

    /* Run well past a full window (10 callbacks = 1000ms) */
    simTicks(2000);

    EXPECT_TRUE(stmGetGpio(heaterPorts[0].heater));
    EXPECT_FALSE(flushAndGetUSBStatus() & AC_EFUSE_OVERCURRENT_Msk(1));
}

TEST_F(ACBoard, efuse_clearOnNextCommand)
{
    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);

    simTicks(100); /* calibration */

    /* Trip the e-fuse on channel 1 */
    setAdcChannelBuffer(0, ADC_OVERCURRENT);
    writeBoardMessage("p1 on 10\n");
    simTicks(1000);
    ASSERT_FALSE(stmGetGpio(heaterPorts[0].heater));
    ASSERT_TRUE(flushAndGetUSBStatus() & AC_EFUSE_OVERCURRENT_Msk(1));

    /* Drop current to zero — bit must still be set, only a command can clear it */
    setAdcChannelBuffer(0, 0);
    simTicks(1100);

    EXPECT_TRUE(flushAndGetUSBStatus() & AC_EFUSE_OVERCURRENT_Msk(1));
    EXPECT_FALSE(stmGetGpio(heaterPorts[0].heater));

    /* New command clears the bit and re-enables the channel */
    writeBoardMessage("p1 on 5\n");
    EXPECT_TRUE(stmGetGpio(heaterPorts[0].heater));
    simTicks(100);
    uint32_t status = flushAndGetUSBStatus();
    EXPECT_FALSE(status & AC_EFUSE_OVERCURRENT_Msk(1));
    EXPECT_FALSE(status & BS_OVER_CURRENT_Msk);

    /* Channel works normally for the rest of the requested duration */
    simTicks(4800);
    EXPECT_TRUE(stmGetGpio(heaterPorts[0].heater));
}

TEST_F(ACBoard, efuse_clearWithTwoChannels)
{
    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);

    simTicks(100); /* calibration */

    /* Trip the e-fuse on channel 1 */
    setAdcChannelBuffer(0, ADC_OVERCURRENT);
    setAdcChannelBuffer(1, ADC_OVERCURRENT);
    writeBoardMessage("p1 on 10\n");
    writeBoardMessage("p2 on 10\n");
    simTicks(1000);
    ASSERT_FALSE(stmGetGpio(heaterPorts[0].heater));
    uint32_t status = flushAndGetUSBStatus();
    ASSERT_TRUE(status & AC_EFUSE_OVERCURRENT_Msk(1));
    ASSERT_TRUE(status & AC_EFUSE_OVERCURRENT_Msk(2));
    ASSERT_TRUE(status & BS_OVER_CURRENT_Msk);
    /* Drop current to zero — bit must still be set, only a command can clear it */
    setAdcChannelBuffer(0, 0);
    simTicks(1100);

    EXPECT_TRUE(flushAndGetUSBStatus() & AC_EFUSE_OVERCURRENT_Msk(1));
    EXPECT_FALSE(stmGetGpio(heaterPorts[0].heater));

    /* New command clears the bit and re-enables the channel */
    writeBoardMessage("p1 on 5\n");
    EXPECT_TRUE(stmGetGpio(heaterPorts[0].heater));
    simTicks(100);
    status = flushAndGetUSBStatus();
    EXPECT_FALSE(status & AC_EFUSE_OVERCURRENT_Msk(1));

    /* Since the other channel overcurrent is still active, the overall overcurrent flag should 
    ** remain set */
    EXPECT_TRUE(status & AC_EFUSE_OVERCURRENT_Msk(2));
    EXPECT_TRUE(status & BS_OVER_CURRENT_Msk);
}

TEST_F(ACBoard, efuse_configurableLimit)
{
    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);

    simTicks(100); /* calibration */

    /* Raise channel 1 limit to 15A via calibration command (format: port,current,x,x) */
    writeBoardMessage("CAL 1,15.0,0,0\n");

    /* Set current to ~12.5A — above the default 12A but below the new 15A limit */
    setAdcChannelBuffer(0, ADC_MID_CURRENT);
    writeBoardMessage("p1 on 10\n");

    /* Run several callbacks; would trip with the default 12A limit but must not with 15A */
    simTicks(2000);

    EXPECT_TRUE(stmGetGpio(heaterPorts[0].heater));
    EXPECT_FALSE(flushAndGetUSBStatus() & AC_EFUSE_OVERCURRENT_Msk(1));
}

TEST_F(ACBoard, faultInfoPrintout) {
    depositUnit_t tmp = {};
    tmp.fault_info.fault = HARD_FAULT;
    writeToFlash(FLASH_ADDR_CAL, (uint8_t*)&tmp, sizeof(depositUnit_t));

    ACBoardInit(&hadc);
    ACBoardLoop(bootMsg);

    EXPECT_FLUSH_USB(ElementsAre(
        "Boot Unit Test\r", 
        "Start of fault info\r", 
        "Last fault was: 1\r", 
        "CFSR was: 0x00000000\r", 
        "HFSR was: 0x00000000\r", 
        "MMFA was: 0x00000000\r", 
        "BFA was:  0x00000000\r", 
        "AFSR was: 0x00000000\r", 
        "Stack Frame was:\r", "0x00000000, 0x00000000, 0x00000000, 0x00000000\r", 
        "0x00000000, 0x00000000, 0x00000000, 0x00000000\r", 
        "End of fault info\r"
    ));
}
