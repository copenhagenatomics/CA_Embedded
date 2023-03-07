#include <stdio.h>
#include <string.h>
#include <queue>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
extern "C" {
    #include <LightController.h>
	#include <CAProtocol.h>
	#include <CAProtocolStm.h>
	#include <systemInfo.h>
	#include <USBprint.h>
	#include <usb_cdc_fops.h>
}
#include <math.h>
#include <assert.h>

// Stub HW dependent methods
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *htim) {return HAL_OK;}
HAL_StatusTypeDef HAL_TIM_PWM_Start_IT(TIM_HandleTypeDef *htim, uint32_t Channel){return HAL_OK;}
HAL_StatusTypeDef HAL_WWDG_Refresh(WWDG_HandleTypeDef *hwwdg){return HAL_OK;}

char* pChar;
// Stub necessary methods
int USBnprintf(const char * format, ... ) {
    return 0;
}
void HALundefined(const char *input){}
void HALJumpToBootloader(){}
void CAPrintHeader(){}
void CAotpRead(){}
void CAhandleUserInputs(CAProtocolCtx* ctx, const char* startMsg){}
const char* CAonBoot(){return pChar;}
void initCAProtocol(CAProtocolCtx* ctx, ReaderFn fn){}
int getBoardInfo(BoardType *bdt, SubBoardType *sbdt){return 0;}
bool isComPortOpen() {return true;}
int usb_cdc_rx(uint8_t* rxByte){return 1;}

class LightControllerCfg
{
public:
	LightControllerCfg(int _channel, int _red, int _green, int _blue) : channel(_channel), red(_red), green(_green), blue(_blue) {};
	LightControllerCfg() : channel(0), red(0), green(0), blue(0) {};
    inline bool operator==(const LightControllerCfg& rhs)
    {
        return (channel == rhs.channel && red == rhs.red && green == rhs.green && blue == rhs.blue);
    }

    int channel;
    int red;
    int green;
    int blue;
};

class TestCALightController
{
public:
	TestCALightController()
	{
		reset();
	}

	void reset() {
		while(!testString.empty())
			testString.pop();
	}

	int testInputValidation(const char* input, int channel, const bool expectedRet)
	{
		unsigned int rgb = 0x000000;
		bool ret = isInputValid(input, &channel, &rgb);
		return ret == expectedRet;
	}
private:
    static std::queue<uint8_t> testString;
};
std::queue<uint8_t> TestCALightController::testString;

int testInputValidation()
{
	TestCALightController LCTester;
    LCTester.reset();

    if (!LCTester.testInputValidation("p1 FFFFFF", 1, true)) return __LINE__;
    if (!LCTester.testInputValidation("p3 ABAB10", 3, true)) return __LINE__;
    if (!LCTester.testInputValidation("p2 012932", 2, true)) return __LINE__;
    if (!LCTester.testInputValidation("p1 0123FF", 1, true)) return __LINE__;
    if (!LCTester.testInputValidation("p1 139087", 1, true)) return __LINE__;

    // Error cases
    if (!LCTester.testInputValidation("p4 139087", 4, false)) return __LINE__;
    if (!LCTester.testInputValidation("p0 139087", 0, false)) return __LINE__;
    if (!LCTester.testInputValidation("p1 13908", 1, false)) return __LINE__;
    if (!LCTester.testInputValidation("p1 FFAABAC", 1, false)) return __LINE__;
    if (!LCTester.testInputValidation("p1 MLKMLK", 1, false)) return __LINE__;
    if (!LCTester.testInputValidation("p1 13Q087", 1, false)) return __LINE__;
	return 0;
}

int main(int argc, char *argv[])
{
	int line = 0;

	if (line = testInputValidation()) { printf("testInputValidation failed at line %d\n", line); }

	printf("All tests run\n");
    return 0;
}
