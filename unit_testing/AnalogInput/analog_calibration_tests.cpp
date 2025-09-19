/*!
** @file   analog_calibration_tests.cpp
** @author Matias
** @date   02/05/2024
*/

#include <inttypes.h>

extern "C" {
    uint32_t _FlashAddrCal = 0;
}

#include <gtest/gtest.h>
#include <gmock/gmock.h>

/* Fakes */
#include "fake_stm32xxxx_hal.h"
#include "fake_StmGpio.h"
#include "fake_USBprint.h"

/* UUT */
#include "calibration.c"

using ::testing::Contains;
using namespace std;

/***************************************************************************************************
** TEST FIXTURES
***************************************************************************************************/

class AnalogCalibrationTest: public ::testing::Test 
{
    protected:
        /*******************************************************************************************
        ** METHODS
        *******************************************************************************************/
        AnalogCalibrationTest()
        {

        }
    
        /*******************************************************************************************
        ** MEMBERS
        *******************************************************************************************/
        FlashCalibration cal = {0};
        CRC_HandleTypeDef hcrc;
};

/***************************************************************************************************
** TESTS
***************************************************************************************************/

/* This test simply re-tests the GPIO init and */
TEST_F(AnalogCalibrationTest, testCalibrationInit)
{
    auto expectStmNull = [](StmGpio* stm) {
        ASSERT_EQ(stm->set,    nullptr);
        ASSERT_EQ(stm->get,    nullptr);
        ASSERT_EQ(stm->toggle, nullptr);
    };

    auto expectStmNotNull = [](StmGpio* stm) {
        ASSERT_NE(stm->set,    nullptr);
        ASSERT_NE(stm->get,    nullptr);
        ASSERT_NE(stm->toggle, nullptr);
    };

    // Check the GPIOs are not initialised prior to init function call
    for (int i = 0; i<6; i++) { expectStmNull(&CH_Ctrl[i]); }

    // Check that the calibration struct has not been initialised
    for (int i = 0; i<NO_CALIBRATION_CHANNELS; i++) 
    { 
        EXPECT_EQ(cal.sensorCalVal[i*2], 0);
        EXPECT_EQ(cal.sensorCalVal[i*2+1], 0);
        EXPECT_EQ(cal.portCalVal[i], 0);
        EXPECT_EQ(cal.measurementType[i], 0);
    }

    calibrationInit(&hcrc, &cal, sizeof(cal));

    // Check GPIOs are initialised 
    for (int i = 0; i<6; i++) { expectStmNotNull(&CH_Ctrl[i]); }

    // Check default calibration values have been set
    for (int i = 0; i<NO_CALIBRATION_CHANNELS; i++) 
    { 
        EXPECT_NEAR(cal.sensorCalVal[i*2], 1.0, 1e-5);
        EXPECT_NEAR(cal.sensorCalVal[i*2+1], 0.0, 1e-5);
        EXPECT_NEAR(cal.portCalVal[i], 1.0, 1e-5);
        EXPECT_EQ(cal.measurementType[i], 0);
    }   

    // Ensure all GPIOs are off (standard calibration)
    for (int i = 0; i<NO_CALIBRATION_CHANNELS; i++) { 
        EXPECT_EQ(stmGetGpio(CH_Ctrl[i]),0); 
    }
}

TEST_F(AnalogCalibrationTest, testCalibrateBoard)
{
    calibrationInit(&hcrc, &cal, sizeof(cal));
    // Check that the port calibration value is set to its default value
    for (int i = 0; i < NO_CALIBRATION_CHANNELS; i++) {
        EXPECT_NEAR(cal.portCalVal[i], 1.0, 1e-4);
    }   

    // Try to calibrate port 1 which is already correctly calibrated
    int port = 1;
    float vinput = 2.5; // Measured input voltage on ports to be calibrated
    int noOfCalibrations = 1;
    const CACalibration calibration[noOfCalibrations] = {port, vinput, 0, 2};

    // ADC value corresponding to 2.5V on ports 
    float ADCMeansRaw[NO_CALIBRATION_CHANNELS] = {2047.5,2047.5,2047.5,2047.5,2047.5,2047.5};

    // Calibrate board ports 1
    calibrateBoard(noOfCalibrations, calibration, &cal, ADCMeansRaw, sizeof(cal), 5.0);

    // Calibration should be unchanged
    EXPECT_NEAR(cal.portCalVal[0], 1, 1e-4)  << "Channel is " << 1;
    for (int i = 1; i<NO_CALIBRATION_CHANNELS; i++) 
    { 
        EXPECT_NEAR(cal.portCalVal[i], 1.0, 1e-4)  << "Channel is " << i+1;
    }   

    ADCMeansRaw[0] = 2060.0;
    // Calibrate 1st port that read wrong ADC value at 2.5V input
    calibrateBoard(noOfCalibrations, calibration, &cal, ADCMeansRaw, sizeof(cal), 5.0);

    EXPECT_NEAR(cal.portCalVal[0], 0.9939, 1e-4)  << "Channel is " << 1;
    for (int i = 1; i<NO_CALIBRATION_CHANNELS; i++) 
    { 
        EXPECT_NEAR(cal.portCalVal[i], 1.0, 1e-4) << "Channel is " << i+1;
    }   

    // Reset the calibration
    ADCMeansRaw[0] = 2047.5;
    calibrateBoard(noOfCalibrations, calibration, &cal, ADCMeansRaw, sizeof(cal), 5.0);
    EXPECT_NEAR(cal.portCalVal[0], 1, 1e-4) << "Channel is " << 1;

    // Call the calibration function with wrong ports and check the calibration
    // is unchanged
    CACalibration wrongPorts[2] = {{0, vinput, 0, 2}, {NO_CALIBRATION_CHANNELS+1, vinput, 0, 2}};
    ADCMeansRaw[0] = 2060.0;

    calibrateBoard(2, wrongPorts, &cal, ADCMeansRaw, sizeof(cal), 5.0);
    EXPECT_NEAR(cal.portCalVal[0], 1, 1e-4) << "Channel is " << 1;
    for (int i = 1; i<NO_CALIBRATION_CHANNELS; i++) 
    { 
        EXPECT_NEAR(cal.portCalVal[i], 1.0, 1e-4) << "Channel is " << i+1;
    }   

    // Call the calibration function with invalid input voltages and check the calibration
    // is unchanged
    CACalibration wrongInputVoltages[2] = {{1, -0.1, 0, 2}, {2, 6.0, 0, 2}};
    ADCMeansRaw[0] = 2047.5;

    calibrateBoard(2, wrongInputVoltages, &cal, ADCMeansRaw, sizeof(cal), 5.0);
    EXPECT_NEAR(cal.portCalVal[0], 1, 1e-4) << "Channel is " << 1;
    for (int i = 1; i<NO_CALIBRATION_CHANNELS; i++) 
    { 
        EXPECT_NEAR(cal.portCalVal[i], 1.0, 1e-4) << "Channel is " << i+1;
    }   
}

TEST_F(AnalogCalibrationTest, testCalibrateSensor)
{
    calibrationInit(&hcrc, &cal, sizeof(cal));

    // Check default calibration values have been set
    for (int i = 0; i<NO_CALIBRATION_CHANNELS; i++) 
    { 
        EXPECT_NEAR(cal.sensorCalVal[i*2], 1.0, 1e-5);
        EXPECT_NEAR(cal.sensorCalVal[i*2+1], 0.0, 1e-5);
        EXPECT_EQ(cal.measurementType[i], 0);
    }   

    // Update a single channel
    int port = 1;
    float alpha = 0.5;
    float beta = 1.56;
    int measurementType = 0;
    const int noOfCalibrations = 1;
    CACalibration newCalibration[noOfCalibrations] = {port, alpha, beta, measurementType};

    // Run calibration command
    calibrateSensor(noOfCalibrations, newCalibration, &cal, sizeof(cal));

    // Check default calibration values have been set
    int channel = port - 1;
    for (int i = 0; i<NO_CALIBRATION_CHANNELS; i++) 
    { 
        // Test that channel has been updated with new calibrations
        if (i == channel)
        {
            EXPECT_NEAR(cal.sensorCalVal[channel*2], alpha, 1e-5);
            EXPECT_NEAR(cal.sensorCalVal[channel*2+1], beta, 1e-5);
            EXPECT_EQ(cal.measurementType[channel], measurementType);
            continue;
        }

        // The remaining channels should not have changed.
        EXPECT_NEAR(cal.sensorCalVal[i*2], 1.0, 1e-5);
        EXPECT_NEAR(cal.sensorCalVal[i*2+1], 0.0, 1e-5);
        EXPECT_EQ(cal.measurementType[i], 0);
    }   

    // Test calibration of multiple ports at a time
    int port4 = 4;
    int port5 = 5;
    CACalibration doubleCalibration[2] = {{port4, alpha+1, beta+1, 1}, {port5, alpha-1, beta-1, 1}};

    // Run calibration command
    calibrateSensor(2, doubleCalibration, &cal, sizeof(cal));

    EXPECT_NEAR(cal.sensorCalVal[(port4-1)*2], alpha+1, 1e-5);
    EXPECT_NEAR(cal.sensorCalVal[(port4-1)*2+1], beta+1, 1e-5);
    EXPECT_EQ(cal.measurementType[port4-1], 1);

    EXPECT_NEAR(cal.sensorCalVal[(port5-1)*2], alpha-1, 1e-5);
    EXPECT_NEAR(cal.sensorCalVal[(port5-1)*2+1], beta-1, 1e-5);
    EXPECT_EQ(cal.measurementType[port5-1], 1);

    // Reset calibration
    CACalibration defaultCal[NO_CALIBRATION_CHANNELS] = {0};
    for (int i = 0; i<NO_CALIBRATION_CHANNELS; i++)
    {
        defaultCal[i] = {i, 1.0, 0.0, 0};
    }
    calibrateSensor(NO_CALIBRATION_CHANNELS, defaultCal, &cal, sizeof(cal));

    // Check default calibration values have been set
    for (int i = 0; i<NO_CALIBRATION_CHANNELS; i++) 
    { 
        EXPECT_NEAR(cal.sensorCalVal[i*2], 1.0, 1e-5);
        EXPECT_NEAR(cal.sensorCalVal[i*2+1], 0.0, 1e-5);
        EXPECT_EQ(cal.measurementType[i], 0);
    }       

    // Test that calling the calibration function with invalid channels
    // does not update any port calibrations
    CACalibration wrongPortCalibration[2] = {{0, 10, 5, 0}, {NO_CALIBRATION_CHANNELS+1, 4, -3, 0}};
    calibrateSensor(2, wrongPortCalibration, &cal, sizeof(cal));

    // Check default calibration values are still set
    for (int i = 0; i<NO_CALIBRATION_CHANNELS; i++) 
    { 
        EXPECT_NEAR(cal.sensorCalVal[i*2], 1.0, 1e-5);
        EXPECT_NEAR(cal.sensorCalVal[i*2+1], 0.0, 1e-5);
        EXPECT_EQ(cal.measurementType[i], 0);
    }       
}

TEST_F(AnalogCalibrationTest, testCalibrationRW)
{
    calibrationInit(&hcrc, &cal, sizeof(cal));

    // Should read out standard calibration
    calibrationRW(false, &cal, sizeof(cal));
    EXPECT_FLUSH_USB(Contains("Calibration: CAL 1,1.0000000000,0.0000000000,0 2,1.0000000000,0.0000000000,0 3,1.0000000000,0.0000000000,0 4,1.0000000000,0.0000000000,0 5,1.0000000000,0.0000000000,0 6,1.0000000000,0.0000000000,0\r"));

    // Update port 1 with new calibration values
    int port = 1;
    float alpha = 0.5;  
    float beta = 1.50;  
    int measurementType = 1; // Update port to measure current
    const int noOfCalibrations = 1;
    CACalibration calibration[noOfCalibrations] = {port, alpha, beta, measurementType};
    calibrateSensor(noOfCalibrations, calibration, &cal, sizeof(cal));

    // The calibration should read out the updated values
    calibrationRW(false, &cal, sizeof(cal));
    EXPECT_FLUSH_USB(Contains("Calibration: CAL 1,0.5000000000,1.5000000000,1 2,1.0000000000,0.0000000000,0 3,1.0000000000,0.0000000000,0 4,1.0000000000,0.0000000000,0 5,1.0000000000,0.0000000000,0 6,1.0000000000,0.0000000000,0\r"));
}