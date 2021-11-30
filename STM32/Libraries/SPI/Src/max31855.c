#include "max31855.h"

void Max31855_Read_Temp(SPI_HandleTypeDef *hspi, float *temp_probe, float *temp_junction)
{
    int  temperature=0;

    uint8_t DATARX[4];
    uint32_t rawData_bitString_temp;
    uint16_t rawData_bitString_junc;
    //read 4 bytes from SPI bus
    //Note that chip selecting is handled elsewhere
    HAL_SPI_Receive(hspi,DATARX,4,1000);

    //Merge 4 bytes into 1
    rawData_bitString_temp = DATARX[0]<<24 | DATARX[1]<<16 | DATARX[2] << 8 | DATARX[3];

     // Checks for errors in the probe
    if(DATARX[3] & 0x01){
        *temp_probe = FAULT_OPEN;
    }
    else if(DATARX[3] & 0x02){
        *temp_probe = FAULT_SHORT_GND;
    }
    else if(DATARX[3] & 0x04){
        *temp_probe = FAULT_SHORT_VCC;
    }
    else{ //No errors
        //reading temperature
        temperature = rawData_bitString_temp >> 18;

        // Check for negative temperature
        if ((DATARX[0]&(0x80))>>7)
        {
            temperature = (DATARX[0] << 6) | (DATARX[1] >> 2);
            temperature&=0b01111111111111;
            temperature^=0b01111111111111;
        }

        // Convert to Degree Celsius
        *temp_probe = temperature * 0.25;
    }

    //calculate juntion temperature
    rawData_bitString_junc = DATARX[2] << 8 | DATARX[3];
    rawData_bitString_junc >>= 4;
    *temp_junction = (rawData_bitString_junc & 0x000007FF);

    if (rawData_bitString_junc & 0x00000800)
    {
        // 2's complement operation
        // Invert
        rawData_bitString_junc = ~rawData_bitString_junc;
        // Ensure operation involves lower 11-bit only
        *temp_junction = rawData_bitString_junc & 0x000007FF;
        // Add 1 to obtain the positive number
        *temp_junction += 1;
        // Make temperature negative
        *temp_junction *= -1;
    }
    *temp_junction = *temp_junction * 0.0625;
}
