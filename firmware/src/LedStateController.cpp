#include "LedStateController.h"

#include "IndicatorLight.h"

LedStateController::LedStateController(IndicatorLight *indicator_light)
{
    m_indicator_light = indicator_light;
}

void LedStateController::setIdle()
{
    if (!m_indicator_light)
    {
        return;
    }
    m_indicator_light->setState(IDLE);
}

void LedStateController::setRecording()
{
    if (!m_indicator_light)
    {
        return;
    }
    m_indicator_light->setState(RECORDING);
}

void LedStateController::setPlaying()
{
    if (!m_indicator_light)
    {
        return;
    }
    m_indicator_light->setState(PLAYING);
}

void LedStateController::showErrorBrief(uint32_t duration_ms)
{
    if (!m_indicator_light)
    {
        return;
    }
    m_indicator_light->setState(ERROR);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    m_indicator_light->setState(IDLE);
}
