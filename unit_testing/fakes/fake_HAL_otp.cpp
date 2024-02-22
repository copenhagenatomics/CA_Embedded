/*!
** @file    fake_CAProtocolStm.cpp
** @author  Luke W
** @date    25/01/2024
**/

#include "HAL_otp.h"

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static BoardInfo _fake_board_info = {0};

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

int HAL_otpRead(BoardInfo *boardInfo)
{
    *boardInfo = _fake_board_info;
    return OTP_SUCCESS;
}

int HAL_otpWrite(const BoardInfo *boardInfo)
{
    _fake_board_info = (BoardInfo)(*boardInfo);
    return OTP_SUCCESS;
}
