/*!
** @file    fake_CAProtocolStm.cpp
** @author  Luke W
** @date    25/01/2024
**/

#include "HAL_otp.h"

int HAL_otpRead(BoardInfo *boardInfo)
{
    return OTP_SUCCESS;
}

int HAL_otpWrite(const BoardInfo *boardInfo)
{
    return OTP_SUCCESS;
}
