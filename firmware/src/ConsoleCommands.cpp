#include "ConsoleCommands.h"

#include "AudioRecordPlayback.h"

ConsoleCommands::ConsoleCommands(AudioRecordPlayback *audio_record_playback)
{
    m_audio_record_playback = audio_record_playback;
    m_line_buffer.reserve(96);
}

bool ConsoleCommands::parseDurationMs(const String &value, uint32_t &duration_ms) const
{
    if (value.length() == 0)
    {
        return false;
    }

    uint64_t parsed = 0;
    for (size_t i = 0; i < value.length(); i++)
    {
        const char c = value.charAt(i);
        if (c < '0' || c > '9')
        {
            return false;
        }
        parsed = (parsed * 10ULL) + static_cast<uint64_t>(c - '0');
        if (parsed > static_cast<uint64_t>(UINT32_MAX))
        {
            return false;
        }
    }

    duration_ms = static_cast<uint32_t>(parsed);
    return true;
}

void ConsoleCommands::handleLine(const String &line)
{
    String cmd = line;
    cmd.trim();
    if (cmd.length() == 0)
    {
        return;
    }

    const int first_space = cmd.indexOf(' ');
    const String first = (first_space < 0) ? cmd : cmd.substring(0, first_space);

    if (first == "record")
    {
        const String arg = (first_space < 0) ? String("") : cmd.substring(first_space + 1);
        uint32_t duration_ms = 0;
        if (!parseDurationMs(arg, duration_ms))
        {
            Serial.println("[RECORD] ERROR: usage: record <duration_ms>");
            return;
        }

        if (!m_audio_record_playback)
        {
            Serial.println("[RECORD] ERROR: audio record/playback service is NULL");
            return;
        }

        m_audio_record_playback->record(duration_ms);
        return;
    }

    if (first == "play")
    {
        const String arg = (first_space < 0) ? String("") : cmd.substring(first_space + 1);
        if (arg == "record")
        {
            if (!m_audio_record_playback)
            {
                Serial.println("[PLAY] ERROR: audio record/playback service is NULL");
                return;
            }
            m_audio_record_playback->playRecorded();
            return;
        }

        Serial.println("[PLAY] ERROR: usage: play record");
        return;
    }

    Serial.printf("[CMD] Unknown command: %s\n", cmd.c_str());
}

void ConsoleCommands::poll()
{
    while (Serial.available() > 0)
    {
        const char c = static_cast<char>(Serial.read());

        if (c == '\r')
        {
            continue;
        }

        if (c == '\n')
        {
            handleLine(m_line_buffer);
            m_line_buffer = "";
            continue;
        }

        // bound check to avoid unbounded String growth
        if (m_line_buffer.length() < 95)
        {
            m_line_buffer += c;
        }
    }
}
