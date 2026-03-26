#include <Arduino.h>
#include "IndicatorLight.h"
#include "driver/uart.h"

namespace
{
constexpr int LOCAL_LED_PIN = LED_BUILTIN;

void uart2_send(const char *buf)
{
    while (*buf)
    {
        Serial2.write(*buf);
        vTaskDelay(2);
        buf++;
    }
}

void setLocalLed(bool on)
{
    // Most ESP32 dev boards use active-high for the built-in LED.
    digitalWrite(LOCAL_LED_PIN, on ? HIGH : LOW);
}

void applyStaticState(IndicatorState state)
{
    switch (state)
    {
    case OFF:
        ledcWrite(0, 0);
        uart2_send("{8701fe}");
        setLocalLed(false);
        break;
    case ON:
        ledcWrite(0, 255);
        uart2_send("{8701ff}");
        setLocalLed(true);
        break;
    case IDLE:
        // dim white
        ledcWrite(0, 24);
        uart2_send("{8701ff}");
        setLocalLed(false);
        break;
    case RECORDING:
        // fallback brightness for built-in LED + external color command
        ledcWrite(0, 255);
        uart2_send("{8701f1}");
        setLocalLed(true);
        break;
    case PLAYING:
        ledcWrite(0, 180);
        uart2_send("{8701f4}");
        setLocalLed(true);
        break;
    default:
        break;
    }
}
}

// This task does all the heavy lifting for our application
void indicatorLedTask(void *param)
{
    IndicatorLight *indicator_light = static_cast<IndicatorLight *>(param);
    const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
    while (true)
    {
        // wait for someone to trigger us
        uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
        if (ulNotificationValue > 0)
        {
            switch (indicator_light->getState())
            {
            case OFF:
            case ON:
            case IDLE:
            case RECORDING:
            case PLAYING:
                applyStaticState(indicator_light->getState());
                break;
            case PULSING:
            {
                // do a nice pulsing effect
                float angle = 0;
                while (indicator_light->getState() == PULSING)
                {
                    ledcWrite(0, 255 * (0.5 * cos(angle) + 0.5));
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                    angle += 0.4 * M_PI;
                }
                break;
            }
            case ERROR:
            {
                // blinking red until state changes
                while (indicator_light->getState() == ERROR)
                {
                    ledcWrite(0, 255);
                    uart2_send("{8701f1}");
                    setLocalLed(true);
                    vTaskDelay(pdMS_TO_TICKS(120));
                    if (indicator_light->getState() != ERROR)
                    {
                        break;
                    }
                    ledcWrite(0, 0);
                    uart2_send("{8701fe}");
                    setLocalLed(false);
                    vTaskDelay(pdMS_TO_TICKS(120));
                }
                break;
            }
            default:
            {
                applyStaticState(OFF);
                break;
            }
            }
        }
    }
}

IndicatorLight::IndicatorLight()
{
    pinMode(LOCAL_LED_PIN, OUTPUT);
    setLocalLed(false);

    Serial2.begin(115200, SERIAL_8N1, 15, 19);
    uart2_send("{8701ff}"); // on
    vTaskDelay(100);
    uart2_send("{8701fe}"); // off

    // use the build in LED as an indicator - we'll set it up as a pwm output so we can make it glow nicely
    ledcSetup(0, 10000, 8);
    ledcAttachPin(2, 0);
    ledcWrite(0, 0);
    // start off with the light off
    m_state = IDLE;
    // set up the task for controlling the light
    xTaskCreate(indicatorLedTask, "Indicator LED Task", 4096, this, 1, &m_taskHandle);
    // push initial LED state so it is applied immediately
    setState(IDLE);
}

void IndicatorLight::setState(IndicatorState state)
{
    m_state = state;
    // apply static states immediately so UI does not depend on task scheduling
    if (state == OFF || state == ON || state == IDLE || state == RECORDING || state == PLAYING)
    {
        applyStaticState(state);
    }
    xTaskNotify(m_taskHandle, 1, eSetBits);
}

IndicatorState IndicatorLight::getState()
{
    return m_state;
}
