/*!
 *  @file humidityApp.c
 *
 *  @date Apr 6, 2022
 *  @author matias
 */

#include <math.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "humidityApp.h"
#include "systemInfo.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "USBprint.h"
#include "time32.h"
#include "sht45.h"
#include "HAL_otp.h"
#include "pcbversion.h"

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static WWDG_HandleTypeDef* hwwdg_ = NULL;

static Measurement mvgMeasurement[NUM_SENSORS] = {{0},{0}};
static int reset_avg_filter[NUM_SENSORS] = {0};
static int error_count[NUM_SENSORS] = {0};

static int inDecontaminationMode = 0;
static uint32_t decontaminationStartTime = 0;


static sht4x_handle_t humiditySensors[NUM_SENSORS] = {
    {.hi2c = NULL, .device_address = SHT45_I2C_ADDR, .serial_number = 0x00},
    {.hi2c = NULL, .device_address = SHT45_I2C_ADDR, .serial_number = 0x00}
};

typedef enum
{
    MEASURE_HUMIDITY,
    START_HEATING,
    WAIT_FOR_CONVERSION,
    UPDATE_HUMIDITY
} sht_state;

typedef enum
{
    NORMAL,
    HIGH
} humidity_state;

typedef struct sht45_heating_state
{
    humidity_state hum_state;
    float tempBeforeHeating;
    uint32_t lastHeating;
    int isFirstHeatingCycle;
} sht45_heating_state;

sht45_heating_state heating_state[2];

/***************************************************************************************************
** PRIVATE FUNCTION PROTOTYPES
***************************************************************************************************/

typedef sht_state (*humidityCb)(sht4x_handle_t* dev, int channel);

static void HumidityPrintStatus();
static void userInputs(const char *input);
static void setMeasurementError(int channel);
static void resetI2C(sht4x_handle_t* dev, int channel);
static void clearI2CBus(I2C_HandleTypeDef* hi2c, uint16_t scl);
static sht_state startConversion(sht4x_handle_t* dev, int channel);
static sht_state getConversion(sht4x_handle_t* dev, int channel);
static sht_state updateHumidity(sht4x_handle_t* dev, int channel);
static sht_state startHeating(sht4x_handle_t* dev, int channel, uint8_t heating_program);
static sht_state monitorTempInBurnin(sht4x_handle_t* dev, int channel);
static void humidityStateMachine();
static void initHumiditySensors(I2C_HandleTypeDef* hi2c1, I2C_HandleTypeDef* hi2c2);

static CAProtocolCtx caProto =
{
        .undefined = userInputs,
        .printHeader = CAPrintHeader,
        .printStatus = HumidityPrintStatus,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL,
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

static humidityCb humidityRead = updateHumidity;

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

/*!
** @brief Verbose print of the Humidity board status.
*/
static void HumidityPrintStatus()
{
    static char buf[600] = { 0 };
    int len = 0;

    len += snprintf(&buf[len], sizeof(buf) - len, "Serial number sensor %d: 0x%08" PRIx32 ".\r\n", 0, humiditySensors[0].serial_number);
    len += snprintf(&buf[len], sizeof(buf) - len, "Serial number sensor %d: 0x%08" PRIx32 ".\r\n", 1, humiditySensors[1].serial_number);

    for (int i = 0; i < NUM_SENSORS; i++)
    {
        if (bsGetField(SHT45_ERROR_Msk(i)))
        {
            len += snprintf(&buf[len], sizeof(buf) - len, "Humidity sensor %d is not responding.\r\n", i);
        }
        else
        {
            len += snprintf(&buf[len], sizeof(buf) - len, "Humidity sensor %d is operating normally.\r\n", i);
        }
    }
    writeUSB(buf, len);
}

/*!
** @brief User input handler that allows to go into burn-in mode.
*/
static void userInputs(const char *input)
{
    if (strncmp(input, "burnin", 6) == 0)
    {
        inDecontaminationMode = 1;
        decontaminationStartTime = HAL_GetTick();
    }
    else if (strncmp(input, "normal", 6) == 0)
    {
        inDecontaminationMode = 0;
    }
    else
    {
        HALundefined(input);
    }
}

/*!
** @brief Sets error value for sensor and resets moving average filter.
*/
static void setMeasurementError(int channel)
{
    /* If a humidity sensor has 10 consecutive failed read-outs it prints 
    ** out an error value of -1 for all outputs to prevent stalling values */
    static const int MAX_ERROR_COUNT = 10;

    // Set error value for sensor
    if(++error_count[channel] >= MAX_ERROR_COUNT)
    {
        error_count[channel] = 10; // Sustain at 10 to prevent overflow
        reset_avg_filter[channel] = 1;
        mvgMeasurement[channel].temp = 10000;
        mvgMeasurement[channel].rh = -1;
        mvgMeasurement[channel].ah = -1;
    }
}

/*!
** @brief Clears the I2C bus of the SHT45 by toggling the SCL line.
*/
static void clearI2CBus(I2C_HandleTypeDef* hi2c, uint16_t scl)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = scl;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    for (int i = 0; i < 10; i++)
    {
        HAL_GPIO_WritePin(GPIOB, scl, GPIO_PIN_RESET);
        for (int j = 0; j < 40; j++) {__NOP();}
        HAL_GPIO_WritePin(GPIOB, scl, GPIO_PIN_SET);
        for (int j = 0; j < 40; j++) {__NOP();}
    }
    HAL_GPIO_DeInit(GPIOB, scl);
}

/*!
** @brief Resets and re-initialises I2C connection.
*/
static void resetI2C(sht4x_handle_t* dev, int channel)
{
    // SCL and SDA pins for I2C1 and I2C2
    static const uint16_t SCLs[NUM_SENSORS] = {GPIO_PIN_6, GPIO_PIN_10};
    static const uint16_t SDAs[NUM_SENSORS] = {GPIO_PIN_7, GPIO_PIN_3};

    HAL_I2C_DeInit(dev->hi2c);

    // If SDA is forced low by the SHT45, clear the I2C bus to release it.
    if (HAL_GPIO_ReadPin(GPIOB, SDAs[channel]) == GPIO_PIN_RESET)
    {
        clearI2CBus(dev->hi2c, SCLs[channel]);
    }

    HAL_I2C_Init(dev->hi2c);
}

/*!
** @brief Initiates a new humidity measurement if a valid connection is established. Otherwise,
**        it tries to re-establish a connection.
*/
static sht_state startConversion(sht4x_handle_t* dev, int channel)
{    
    // In case of error reset I2C and try to re-establish connection.
    if (bsGetField(SHT45_ERROR_Msk(channel)))
    {
        // Reset I2C handle
        resetI2C(dev, channel);

        // Try to establish connection again. If successful then initiate a new measurement. 
        if (sht4x_get_serial(dev) == HAL_OK) 
        { 
            bsClearField(SHT45_ERROR_Msk(channel));
        }
        else 
        {
            bsSetError(SHT45_ERROR_Msk(channel)); 
            setMeasurementError(channel);
            return MEASURE_HUMIDITY;
        }
    }
        
    // Initiate new measurement
    if (sht4x_initiate_measurement(dev, SHT4X_MEASURE_HIGHREP) != HAL_OK)
    {
        bsSetError(SHT45_ERROR_Msk(channel));
        setMeasurementError(channel);
        return MEASURE_HUMIDITY;
    }
    return WAIT_FOR_CONVERSION;
}


/*!
** @brief Gets the conversion result from the sensor. If the sensor has not yet converted the sample
**        it will retry until the conversion is ready.
*/
static sht_state getConversion(sht4x_handle_t* dev, int channel)
{
    sht4x_get_measurement(dev);
    if (dev->hi2c->ErrorCode)
    {
        /* The datasheet specifies that the maximum time for a high repition conversion is max 8.3 ms. 
           If the sensor has not yet fully converted the sample it will respond with a NACK.
           As soon as a new measurement is ready the NACK flag will go low. 
           On any other errors reset the I2C peripheral and soft reset SHT45. */
        if (dev->hi2c->ErrorCode == HAL_I2C_ERROR_AF)
        {
            return WAIT_FOR_CONVERSION;
        }

        // Set error value for sensor and initiate new measurement.
        bsSetError(SHT45_ERROR_Msk(channel));
        return MEASURE_HUMIDITY;
    }

    return UPDATE_HUMIDITY;
}


/*!
** @brief Reads the current temperature, relative and absolute humidity. The function
**        averages the 10 last samples.
*/
static sht_state updateHumidity(sht4x_handle_t* dev, int channel)
{
    // Moving average of last 10 samples.
    static const int M_AVG_COUNT = 10; 
    // Reset error count.
    error_count[channel] = 0;

    if ((dev->data.temperature - heating_state[channel].tempBeforeHeating >= 0.1) && 
        ((HAL_GetTick() - heating_state[channel].lastHeating) <= HEATING_SETTLING_TIME) && 
         heating_state[channel].hum_state == HIGH)
    {
        // Update temperature, but do not update humidity levels until the temperature has stabilised.
        mvgMeasurement[channel].temp += (dev->data.temperature - mvgMeasurement[channel].temp)/M_AVG_COUNT;
    }   
    else if (reset_avg_filter[channel])
    {
        /* After re-establishing connection set the outputs directly and use moving averaging
        ** filter from next iteration. */
        reset_avg_filter[channel] = 0;
        mvgMeasurement[channel].temp = dev->data.temperature;
        mvgMeasurement[channel].rh = dev->data.relative_humidity;
        mvgMeasurement[channel].ah = dev->data.absolute_humidity;
    }
    else
    {
        /* IIR low pass filtering. Normal operation. */
        mvgMeasurement[channel].temp += (dev->data.temperature - mvgMeasurement[channel].temp)/M_AVG_COUNT;
        mvgMeasurement[channel].rh += (dev->data.relative_humidity - mvgMeasurement[channel].rh)/M_AVG_COUNT;
        mvgMeasurement[channel].ah += (dev->data.absolute_humidity - mvgMeasurement[channel].ah)/M_AVG_COUNT;
    }
    
    bsClearError(HUMIDITY_NO_ERROR_Msk);
    
    heating_state[channel].hum_state = (mvgMeasurement[channel].rh >= RH_THRESHOLD_HIGH) ? HIGH : NORMAL;

    if (heating_state[channel].hum_state == HIGH && 
        ((HAL_GetTick() - heating_state[channel].lastHeating >= HEATING_INTERVAL) || heating_state[channel].isFirstHeatingCycle))
    {
        heating_state[channel].isFirstHeatingCycle = 0;
        return START_HEATING;
    }
    return MEASURE_HUMIDITY;
}

/*!
** @brief Starts a heating cycle on the sensor.
*/
static sht_state startHeating(sht4x_handle_t* dev, int channel, uint8_t heating_program)
{
    /* Record temperature of sensor prior to heating. The humidity should not be output until after
    ** the temperature has settled to previous level */
    heating_state[channel].tempBeforeHeating = mvgMeasurement[channel].temp;

    // After a heating cycle a new conversion is ready.  
    if (sht4x_turn_on_heater(dev, heating_program) != HAL_OK)
    {        
        bsSetError(SHT45_ERROR_Msk(channel));
        return MEASURE_HUMIDITY;
    }
    heating_state[channel].lastHeating = HAL_GetTick();

    return WAIT_FOR_CONVERSION;
}

/*!
** @brief Monitors the temperature of the sensor during burn-in mode. If the temperature exceeds
**        MAX_TEMP_BEFORE_HEATING it stops heating until the temperature drops below the threshold.
*/
static sht_state monitorTempInBurnin(sht4x_handle_t* dev, int channel)
{
    // Moving average of last 10 samples.
    static const int M_AVG_COUNT = 10; 

    if (reset_avg_filter[channel])
    {
        /* After re-establishing connection set the outputs directly and use moving averaging
        ** filter from next iteration. */
        reset_avg_filter[channel] = 0;
        mvgMeasurement[channel].temp = dev->data.temperature;
        mvgMeasurement[channel].rh = dev->data.relative_humidity;
        mvgMeasurement[channel].ah = dev->data.absolute_humidity;
    }
    else
    {
        /* IIR low pass filtering. Normal operation. */
        mvgMeasurement[channel].temp += (dev->data.temperature - mvgMeasurement[channel].temp)/M_AVG_COUNT;
        mvgMeasurement[channel].rh += (dev->data.relative_humidity - mvgMeasurement[channel].rh)/M_AVG_COUNT;
        mvgMeasurement[channel].ah += (dev->data.absolute_humidity - mvgMeasurement[channel].ah)/M_AVG_COUNT;
    }

    if ((HAL_GetTick() - decontaminationStartTime) >= BURN_IN_TIME)
    {
        inDecontaminationMode = 0;
    }

    if (mvgMeasurement[channel].temp >= MAX_TEMP_BEFORE_HEATING)
    {
        return MEASURE_HUMIDITY;
    }
    return START_HEATING;
}

/*!
**  @brief State machine for humidity measurements.
*/
static void humidityStateMachine()
{
    static sht_state state[NUM_SENSORS] = {MEASURE_HUMIDITY, MEASURE_HUMIDITY};
    
    /* If the board is in decontamination mode, the sensor is heated to 110C for 80 minutes.
    ** The humidity readings are not valid in decontamination mode */
    humidityRead = (inDecontaminationMode) ? monitorTempInBurnin : updateHumidity;
    uint8_t heaterProfile = (inDecontaminationMode) ? SHT4X_HEATER_110mW_1s : SHT4X_HEATER_200mW_100ms;

    for (int i = 0; i < NUM_SENSORS; i++)
    {
        switch (state[i])
        {
            case MEASURE_HUMIDITY: {
                state[i] = startConversion(&humiditySensors[i], i);
                break;
            }
            case START_HEATING: {
                state[i] = startHeating(&humiditySensors[i], i, heaterProfile);
                break;
            }
            case WAIT_FOR_CONVERSION: {
                state[i] = getConversion(&humiditySensors[i], i);
                break;
            }
            case UPDATE_HUMIDITY: {
                state[i] = humidityRead(&humiditySensors[i], i);
                break;
            }
            default: {
                state[i] = MEASURE_HUMIDITY;
                break;
            }
        }
    }
}

/*!
** @brief Assigns SPI handles to sensor handles and tries to read the SHT45 serial
**        information to determine whether it has a valid connection.
*/
static void initHumiditySensors(I2C_HandleTypeDef* hi2c1, I2C_HandleTypeDef* hi2c2)
{
    humiditySensors[0].hi2c = hi2c1;
    humiditySensors[1].hi2c = hi2c2;

    // Read serial to ensure connection can be established
    for (int i = 0; i < NUM_SENSORS; i++)
    {
        if (sht4x_get_serial(&humiditySensors[i]) != HAL_OK)
        {
            bsSetError(SHT45_ERROR_Msk(i));
        }
        heating_state[i].hum_state = NORMAL;
        heating_state[i].isFirstHeatingCycle = 1;
        heating_state[i].tempBeforeHeating = 0;
        heating_state[i].lastHeating = 0;
    }
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

/*!
** @brief Timer interrupt with an interrupt frequency of 10 Hz. The function is responsible for 
**        printing measurement data and board status over USB.
*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    HAL_WWDG_Refresh(hwwdg_);
    if (!isUsbPortOpen()) { return; }

    if (bsGetField(BS_VERSION_ERROR_Msk))
    {
        USBnprintf("0x%08" PRIx32, bsGetStatus());
        return;
    }

    USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, 0x%08" PRIx32, 
                mvgMeasurement[0].temp, mvgMeasurement[0].rh, mvgMeasurement[0].ah,
                mvgMeasurement[1].temp, mvgMeasurement[1].rh, mvgMeasurement[1].ah,
                bsGetStatus());
}


/*!
** @brief Loop function called repeatedly in main loop
** 
** * Responds to user input
** * Initiates measurements, waits in a non-blocking manner for conversions 
**   to finish and gets new measurements depending on current sensor state.
*/
void LoopHumidity(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
    humidityStateMachine();
}

/*!
** @brief Setup function called at startup
**
** Checks the hardware matches this FW version, starts the USB communication and establishes 
** communication with the humidity sensors. 
*/
void InitHumidity(I2C_HandleTypeDef* hi2c1, I2C_HandleTypeDef* hi2c2, WWDG_HandleTypeDef* hwwdg)
{
    initCAProtocol(&caProto, usbRx);
    hwwdg_ = hwwdg;

    // PCB V1.6 of the humidity board uses SHT45 i.e. not functional on older PCB versions.
    if(boardSetup(HumidityChip, (pcbVersion) {BREAKING_MAJOR, BREAKING_MINOR}) == -1) {
        return;
    }

    initHumiditySensors(hi2c1, hi2c2);
}
