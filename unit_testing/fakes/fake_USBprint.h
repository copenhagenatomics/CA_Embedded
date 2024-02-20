/*!
** @file   fake_USBprint.h
** @author Luke W
** @date   22/01/2024
*/

#ifndef FAKE_USBPRINT_H_
#define FAKE_USBPRINT_H_

#include <vector>
#include <string>

void hostUSBprintf(const char * format, ...);
std::vector<std::string>* hostUSBread(bool flush=false);

void hostUSBConnect();
void hostUSBDisconnect();

#endif /* FAKE_USBPRINT_H_ */
