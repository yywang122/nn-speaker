#ifndef __console_commands_h__
#define __console_commands_h__

#include <Arduino.h>

class AudioRecordPlayback;

class ConsoleCommands
{
private:
    AudioRecordPlayback *m_audio_record_playback;
    String m_line_buffer;

    void handleLine(const String &line);
    bool parseDurationMs(const String &value, uint32_t &duration_ms) const;

public:
    explicit ConsoleCommands(AudioRecordPlayback *audio_record_playback);
    void poll();
};

#endif
