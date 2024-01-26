/*!
** @file   fake_USBprint.h
** @author Luke W
** @date   22/01/2024
*/

#ifndef FAKE_USBPRINT_H_
#define FAKE_USBPRINT_H_

#include <sstream>

void hostUSBprintf(const char * format, ...);
std::stringstream hostUSBread();

#endif /* FAKE_USBPRINT_H_ */
