#include <stdio.h>
#include <string.h>
#include <queue>
extern "C" {
    #include "stm32f4xx_hal.h"
    #include <ADCMonitor.h>
    #include <CAProtocol.h>
}
#include <math.h>
#include <assert.h>

// HW depended functions, stub these.
void HAL_ADC_Start_DMA(ADC_HandleTypeDef* hadc, uint32_t* pData, uint32_t Length) {}
void HAL_Delay(uint32_t var) {}
int USBnprintf(const char * format, ... ) {
    return 0;
}
void JumpToBootloader() {};

static void generate4Sine(int16_t* pData, int length, int offset, int freq)
{
    for (int i = 0; i<length; i++)
    {
        pData[4*i+0] = 2041 + (2041.0 * sin( (i*freq + offset      )/180.0 * M_PI));
        pData[4*i+1] = 2041 + (2041.0 * sin( (i*freq + offset + 120)/180.0 * M_PI));
        pData[4*i+2] = 2041 + (2041.0 * sin( (i*freq + offset + 240)/180.0 * M_PI));
        pData[4*i+3] = (42 + i) & 0xFFFF;
    }
}


// Helper function to be used during debug.
static void debugPData(const int16_t* pData, int length, int channel)
{
    for (int i = 0; i<length; i++)
    {
        if ((i) % 10 == 0)
            printf("%4d: ", i);

        printf("%4d ",pData[4*i+channel]);
        if ((i+1) % 10 == 0)
            printf("\n");
    }
}

int testSine()
{
    // Create an array used for buffer data.
    const int noOfSamples = 120;
    int16_t pData[noOfSamples*4*2];
    generate4Sine(pData, noOfSamples, 0, 10);

    ADC_HandleTypeDef dommy = { { 4 } };
    ADCMonitorInit(&dommy, pData, noOfSamples*4*2);

    SineWave s = sineWave(pData, 4, noOfSamples, 0);

    if (s.begin != 9 || s.end != 117)
        return __LINE__;
    s = sineWave(pData, 4, noOfSamples, 1);
    if (s.begin != 15 || s.end != 105)
        return __LINE__;
    s = sineWave(pData, 4, noOfSamples, 2);
    if (s.begin != 3 || s.end != 111)
        return __LINE__;
    return 0;
}

int testCMAverage()
{
    const int noOfSamples = 10;
    int16_t pData[noOfSamples*4*2];

    for (int i = 0; i<noOfSamples; i++)
    {
        pData[4*i] = (i % 10) * 20;
    }
    ADC_HandleTypeDef dommy = { { 4 } };
    ADCMonitorInit(&dommy, pData, noOfSamples*4*2);

    if (cmaAvarage(pData, 0, 85, 5) != 112)
        return __LINE__;

    return 0;
}

static struct {
    CACalibration cal[10];
    int noOfCallibration;
} calData;
void CAClibrationCb(int noOfPorts, const CACalibration *catAr) {
    calData.noOfCallibration = noOfPorts;
    memcpy(calData.cal, catAr, sizeof(calData.cal));
}
int calCompare(int noOfPorts, const CACalibration* catAr)
{
    if (noOfPorts != calData.noOfCallibration)
        return 1;
    for (int i=0; i<noOfPorts; i++)
    {
        if (calData.cal[i].port  != catAr[i]. port ||
            calData.cal[i].alpha != catAr[i].alpha ||
            calData.cal[i].beta  != catAr[i]. beta)
        { return 2; }
    }

    return 0; // All good
}

class PortCfg
{
public:
    PortCfg(bool _state, int _duration, int _percent) : state(_state), duration(_duration), percent(_percent) {};
    PortCfg() : state(false), duration(-1), percent(-1) {};
    inline bool operator==(const PortCfg& rhs)
    {
        return (state == rhs.state && duration == rhs.duration && percent == rhs.percent);
    }

    bool state;
    int duration;
    int percent;
};

class TestCAProtocol
{
public:
    TestCAProtocol()
    {
        caProto.calibration = CAClibrationCb;
        caProto.allOn = TestCAProtocol::allOn;
        caProto.portState = TestCAProtocol::portState;
        caProto.undefined = TestCAProtocol::undefined;
        initCAProtocol(&caProto, testReader);
        reset();
    }
    void reset() {
        while(!testString.empty())
            testString.pop();
        portCtrl.allOn = 0;
        portCtrl.undefCall = 0;
    }

    int testCalibration(const char* input, int noOfPorts, const CACalibration calAray[])
    {
        while(*input != 0) {
            testString.push(*input);
            input++;
        }
        inputCAProtocol(&caProto);
        return calCompare(noOfPorts, calAray);
    }

    int testPortCtrl(const char* input, int port, const PortCfg& cfg)
    {
        while(*input != 0) {
            testString.push(*input);
            input++;
        }
        inputCAProtocol(&caProto);
        return portCtrl.port[port] == cfg;
    }

    static struct PortCtrl{
        int allOn;
        PortCfg port[12 + 1];
        int undefCall;
    } portCtrl;
private:
    CAProtocolCtx caProto;;
    static std::queue<uint8_t> testString;
    static int testReader(uint8_t* rxBuf)
    {
        *rxBuf = testString.front();
        testString.pop();
        return 0;
    }
    static void allOn(bool state) {
        portCtrl.allOn++;
    }
    static void portState(int port, bool state, int percent, int duration)
    {
        if (port > 0 && port <= 12) {
            portCtrl.port[port] = { state, percent, duration };
        }
    }
    static void undefined(const char* input)
    {
        portCtrl.undefCall++;
    }
};
std::queue<uint8_t> TestCAProtocol::testString;
TestCAProtocol::PortCtrl TestCAProtocol::portCtrl;

int testCalibration()
{
    TestCAProtocol caProtocol;
    caProtocol.reset();

    if (caProtocol.testCalibration("CAL 3,0.05,1.56\r", 1, (const CACalibration[]) {{3, 0.05, 1.56}}))
        return __LINE__;
    if (caProtocol.testCalibration("CAL 3,0.05,1.56 2,344,36\n\r", 2, (const CACalibration[]) {{3, 0.05, 1.56},{2, 344, 36}}))
        return __LINE__;
    if (caProtocol.testCalibration("CAL 3,0.05,1.56 2,0.04,.36\n", 2, (const CACalibration[]) {{3, 0.05, 1.56},{2, 0.04, 0.36}}))
        return __LINE__;
    return 0; // All good.
}

int testPortCtrl()
{
    TestCAProtocol caProtocol;
    caProtocol.reset();

    caProtocol.testPortCtrl("all on\r\n", -1, PortCfg());
    if (caProtocol.portCtrl.allOn != 1) return __LINE__;
    caProtocol.testPortCtrl("all off\r\n", -1, PortCfg());
    if (caProtocol.portCtrl.allOn != 2) return __LINE__;

    caProtocol.reset();
    if (!caProtocol.testPortCtrl("p10 off\r\n", 10, PortCfg(false, 0, -1))) return __LINE__;
    if (!caProtocol.testPortCtrl("p10 on\r\n", 10, PortCfg(true, 100, -1))) return __LINE__;
    if (!caProtocol.testPortCtrl("p9 off\r\n", 9, PortCfg(false, 0, -1))) return __LINE__;
    if (!caProtocol.testPortCtrl("p9 on\r\n", 9, PortCfg(true, 100, -1))) return __LINE__;
    if (!caProtocol.testPortCtrl("p8 on 50\r\n", 8, PortCfg(true, 100, 50))) return __LINE__;
    if (!caProtocol.testPortCtrl("p8 on 50%\r\n", 8, PortCfg(true, 50, -1))) return __LINE__;
    if (!caProtocol.testPortCtrl("p8 off 50%\r\n", 8, PortCfg(false, 0, -1))) return __LINE__;
    if (!caProtocol.testPortCtrl("p8 on 22 60%\r\n", 8, PortCfg(true, 60, 22))) return __LINE__;
    if (!caProtocol.testPortCtrl("p11 on 50%\r\n", 11, PortCfg(true, 50, -1))) return __LINE__;
    if (!caProtocol.testPortCtrl("p11 off 50%\r\n", 11, PortCfg(false, 0, -1))) return __LINE__;

    // Test some error cases
    if (!caProtocol.testPortCtrl("p7 60\r\n", 7, PortCfg())) return __LINE__;
    if (!caProtocol.testPortCtrl("p7 60%\r\n", 7, PortCfg())) return __LINE__;
    if (!caProtocol.testPortCtrl("p7 52 60%\r\n", 7, PortCfg())) return __LINE__;
    if (!caProtocol.testPortCtrl("p7 on 60e\r\n", 7, PortCfg())) return __LINE__;
    if (!caProtocol.testPortCtrl("p7 on 52 60\r\n", 7, PortCfg())) return __LINE__;
    if (!caProtocol.testPortCtrl("p7 sdfs 52 60%\r\n", 7, PortCfg())) return __LINE__;
    if (caProtocol.portCtrl.undefCall != 6)  return __LINE__;
    return 0;
}

int main(int argc, char *argv[])
{
    int line = 0;
    if (line = testSine()) { printf("TestSine failed at line %d\n", line); }
    if (line = testCMAverage()) { printf("TestSine failed at line %d\n", line); }
    if (line = testCalibration()) { printf("testCAProtocol failed at line %d\n", line); }
    if (line = testPortCtrl()) { printf("testPortCtrl failed at line %d\n", line); }
    printf("All tests run successfully\n");

    return 0;
}
