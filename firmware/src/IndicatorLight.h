#ifndef _indicator_light_h_
#define _indicator_light_h_

enum IndicatorState
{
    OFF,
    ON,
    PULSING,
    IDLE,
    RECORDING,
    PLAYING,
    ERROR
};

class IndicatorLight
{
private:
    IndicatorState m_state;
    TaskHandle_t m_taskHandle;

public:
    IndicatorLight();
    void setState(IndicatorState state);
    IndicatorState getState();
};

#endif
