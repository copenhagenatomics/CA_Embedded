/*
 * serialRead.c
 *
 *  Created on: May 3, 2021
 *      Author: alema
 */


#include "handleGenericMessages.h"

void handleGenericMessages(char * inputBuffer){

	  if(!(strcmp(inputBuffer, "Serial"))){
		  printHeader();
	  }
	  else if(!(strcmp(inputBuffer, "DFU"))){
		  USBprintf("s", "Entering bootloader mode");
		  HAL_Delay(200);
		  JumpToBootloader();
	  }
	  else if((strcmp(inputBuffer, "\0"))){
		  USBprintf("ss", "MISREAD: ", inputBuffer); // respond with misread and serialBuffer contents if all checks have failed
	  }
}
