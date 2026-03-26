#ifndef __led_state_controller_h__
#define __led_state_controller_h__

#include <Arduino.h>

class IndicatorLight;

class LedStateController
{
private:
    IndicatorLight *m_indicator_light;

public:
    explicit LedStateController(IndicatorLight *indicator_light);
    void setIdle();
    void setRecording();
    void setPlaying();
    void showErrorBrief(uint32_t duration_ms = 350);
};

#endif
