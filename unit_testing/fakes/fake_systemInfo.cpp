/*!
** @file    fake_CAProtocol.cpp
** @author  Luke W
** @date    25/01/2024
**/

#include "systemInfo.h"

static struct BS
{
    uint32_t boardStatus;
    float temp;
    float underVoltage;
    float overVoltage;
    float overCurrent;
    BoardType boardType;
    pcbVersion pcb_version;
} BS = {0};

const char* systemInfo()
{
    return " ";
}

const char* statusInfo(bool printStart)
{
    return " ";
}

int getBoardInfo(BoardType *bdt, SubBoardType *sbdt)
{
    return 0;
}

int getPcbVersion(pcbVersion* ver)
{
    return 0;
}

// Functions updating board status
void bsSetErrorRange(uint32_t field, uint32_t range)
{
    // For ranges of bits where only a subset of them are being set,
    // then setting a new subset will result in the union of the two subset.
    // Since, for a range it is assumed that the bits in the range
    // are related i.e. each combination meaning a unique state, then
    // we need to reset previous state before setting the new state.
    BS.boardStatus &= ~range;
    BS.boardStatus |= (BS_ERROR_Msk | field);
}

void bsSetFieldRange(uint32_t field, uint32_t range)
{
    // Reset bits before setting the new value of the range for the same
    // reason as described in the bsSetErrorRange function.
    BS.boardStatus &= ~range;
    BS.boardStatus |= field;
}

/*!
** @brief Clears the error bit, if none of the bits in the field are set
*/
void bsClearError(uint32_t field) {     
    if (!(BS.boardStatus & field))
    {
        bsClearField(BS_ERROR_Msk);
    } 
}

void bsSetError(uint32_t field) { BS.boardStatus |= (BS_ERROR_Msk | field); }
void bsSetField(uint32_t field){ BS.boardStatus |= field; }
void bsClearField(uint32_t field){ BS.boardStatus &= ~field; }
uint32_t bsGetStatus(){ return BS.boardStatus; }
void setBoardTemp(float temp){ BS.temp = temp; }
void setBoardUnderVoltage(float voltage){ BS.underVoltage = voltage; }
void setBoardOverVoltage(float voltage){ BS.overVoltage = voltage; }
void setBoardOverCurrent(float current){ BS.overCurrent = current; }
void setFirmwareBoardType(BoardType type){ BS.boardType = type; }
void setFirmwareBoardVersion(pcbVersion version){ BS.pcb_version = version; }