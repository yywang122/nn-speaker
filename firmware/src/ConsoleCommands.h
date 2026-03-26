#ifndef __console_commands_h__
#define __console_commands_h__

#include <Arduino.h>

class AudioRecordPlayback;
class LedStateController;

class ConsoleCommands
{
private:
    AudioRecordPlayback *m_audio_record_playback;
    LedStateController *m_led_state;
    static constexpr size_t RX_BUFFER_SIZE = 128;
    char m_rx_buffer[RX_BUFFER_SIZE];
    size_t m_rx_length;

    void handleLine(const String &line);
    bool parseDurationMs(const String &value, uint32_t &duration_ms) const;
    void printHelp() const;

public:
    ConsoleCommands(AudioRecordPlayback *audio_record_playback, LedStateController *led_state);
    void poll();
};

#endif
