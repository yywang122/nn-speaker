#include "ConsoleCommands.h"

#include <ctype.h>
#include <stdint.h>

#include "AudioRecordPlayback.h"

ConsoleCommands::ConsoleCommands(AudioRecordPlayback *audio_record_playback)
{
    m_audio_record_playback = audio_record_playback;
    m_rx_length = 0;
    m_rx_buffer[0] = '\0';
}

void ConsoleCommands::printHelp() const
{
    Serial.println("[CMD] Available commands:");
    Serial.println("[CMD]   help");
    Serial.println("[CMD]   record <duration_ms>");
    Serial.println("[CMD]   record status");
    Serial.println("[CMD]   record clear");
    Serial.println("[CMD]   play record");
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
    String cleaned = line;
    cleaned.replace("\r", "");
    cleaned.replace("\n", "");
    cleaned.trim();

    if (cleaned.length() == 0)
    {
        return;
    }

    String command;
    String arg1;
    String arg2;

    int token_index = 0;
    size_t i = 0;
    while (i < cleaned.length())
    {
        while (i < cleaned.length() && isspace(static_cast<unsigned char>(cleaned.charAt(i))))
        {
            i++;
        }
        if (i >= cleaned.length())
        {
            break;
        }

        size_t start = i;
        while (i < cleaned.length() && !isspace(static_cast<unsigned char>(cleaned.charAt(i))))
        {
            i++;
        }
        const String token = cleaned.substring(start, i);

        if (token_index == 0)
        {
            command = token;
        }
        else if (token_index == 1)
        {
            arg1 = token;
        }
        else if (token_index == 2)
        {
            arg2 = token;
        }
        token_index++;
    }

    if (command == "help")
    {
        if (token_index != 1)
        {
            Serial.println("[CMD] ERROR: usage: help");
            return;
        }
        printHelp();
        return;
    }

    if (command == "record")
    {
        if (!m_audio_record_playback)
        {
            Serial.println("[RECORD] ERROR: audio record/playback service is NULL");
            return;
        }

        if (token_index == 2 && arg1 == "status")
        {
            Serial.printf("[RECORD] status: recorded_bytes=%u, valid=%s\n",
                          static_cast<unsigned>(m_audio_record_playback->getRecordedBytes()),
                          m_audio_record_playback->hasValidRecording() ? "yes" : "no");
            return;
        }

        if (token_index == 2 && arg1 == "clear")
        {
            m_audio_record_playback->clearRecording();
            return;
        }

        if (token_index != 2)
        {
            Serial.println("[RECORD] ERROR: usage: record <duration_ms>");
            return;
        }

        uint32_t duration_ms = 0;
        if (!parseDurationMs(arg1, duration_ms))
        {
            Serial.println("[RECORD] ERROR: usage: record <duration_ms>");
            return;
        }

        m_audio_record_playback->record(duration_ms);
        return;
    }

    if (command == "play")
    {
        if (token_index == 2 && arg1 == "record")
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

    (void)arg2;
    Serial.printf("[CMD] Unknown command: %s\n", cleaned.c_str());
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
            m_rx_buffer[m_rx_length] = '\0';
            handleLine(String(m_rx_buffer));
            m_rx_length = 0;
            m_rx_buffer[0] = '\0';
            continue;
        }

        if (c >= 32 && c <= 126)
        {
            if (m_rx_length < (RX_BUFFER_SIZE - 1))
            {
                m_rx_buffer[m_rx_length++] = c;
                m_rx_buffer[m_rx_length] = '\0';
            }
            else
            {
                Serial.println("[CMD] ERROR: input line too long");
                m_rx_length = 0;
                m_rx_buffer[0] = '\0';
            }
        }
    }
}
