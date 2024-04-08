/*!
** @file   fake_USBprint.h
** @author Luke W
** @date   22/01/2024
*/

#ifndef FAKE_USBPRINT_H_
#define FAKE_USBPRINT_H_

#include <vector>
#include <string>

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* Allow a range of single line container tests */
#define EXPECT_READ_USB(x) { \
    vector<string>* ss = hostUSBread(); \
    EXPECT_THAT(*ss, (x)); \
    delete ss; \
}

/* Allow a range of single line container tests */
#define EXPECT_FLUSH_USB(x) { \
    vector<string>* ss = hostUSBread(true); \
    EXPECT_THAT(*ss, (x)); \
    delete ss; \
}


void hostUSBprintf(const char * format, ...);
std::vector<std::string>* hostUSBread(bool flush=false);

void hostUSBConnect();
void hostUSBDisconnect();

#endif /* FAKE_USBPRINT_H_ */
