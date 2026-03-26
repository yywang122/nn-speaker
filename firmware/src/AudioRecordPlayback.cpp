#include "AudioRecordPlayback.h"

#include <algorithm>
#include <esp_heap_caps.h>
#include <stdlib.h>

#include "I2SSampler.h"
#include "I2SOutput.h"
#include "RingBuffer.h"

RecordedAudioSampleSource::RecordedAudioSampleSource()
{
    m_samples = NULL;
    m_total_samples = 0;
    m_position = 0;
}

void RecordedAudioSampleSource::reset(const int16_t *samples, size_t total_samples)
{
    m_samples = samples;
    m_total_samples = total_samples;
    m_position = 0;
}

int RecordedAudioSampleSource::getFrames(Frame_t *frames, int number_frames)
{
    if (!frames || number_frames <= 0 || !m_samples)
    {
        return 0;
    }

    int copied = 0;
    while (copied < number_frames && m_position < m_total_samples)
    {
        const int16_t sample = m_samples[m_position++];
        frames[copied].left = sample;
        frames[copied].right = sample;
        copied++;
    }
    return copied;
}

bool RecordedAudioSampleSource::available()
{
    return (m_samples != NULL) && (m_position < m_total_samples);
}

static void *allocateRecordingBuffer(size_t requested_bytes)
{
#ifdef BOARD_HAS_PSRAM
    void *buffer = heap_caps_malloc(requested_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buffer)
    {
        return buffer;
    }
#endif
    return malloc(requested_bytes);
}

AudioRecordPlayback::AudioRecordPlayback(I2SSampler *sampler, I2SOutput *output)
{
    m_sampler = sampler;
    m_output = output;
    m_recorded_samples = NULL;
    m_recorded_sample_count = 0;
    m_recorded_source = new RecordedAudioSampleSource();
}

AudioRecordPlayback::~AudioRecordPlayback()
{
    freeRecordingBuffer();
    delete m_recorded_source;
    m_recorded_source = NULL;
}

void AudioRecordPlayback::freeRecordingBuffer()
{
    if (m_recorded_samples != NULL)
    {
        free(m_recorded_samples);
        m_recorded_samples = NULL;
    }
    m_recorded_sample_count = 0;
}

bool AudioRecordPlayback::record(uint32_t duration_ms)
{
    if (!m_sampler)
    {
        Serial.println("[RECORD] ERROR: sampler is NULL");
        return false;
    }

    if (duration_ms == 0 || duration_ms > MAX_DURATION_MS)
    {
        Serial.printf("[RECORD] ERROR: invalid duration_ms=%u (allowed: 1..%u)\n",
                      static_cast<unsigned>(duration_ms),
                      static_cast<unsigned>(MAX_DURATION_MS));
        return false;
    }

    const uint64_t samples64 = (static_cast<uint64_t>(SAMPLE_RATE) * duration_ms) / 1000ULL;
    if (samples64 == 0 || samples64 > static_cast<uint64_t>(SIZE_MAX))
    {
        Serial.println("[RECORD] ERROR: sample count overflow");
        return false;
    }

    const size_t requested_samples = static_cast<size_t>(samples64);

    if (requested_samples > (SIZE_MAX / sizeof(int16_t)))
    {
        Serial.println("[RECORD] ERROR: byte size overflow");
        return false;
    }

    const size_t requested_bytes = requested_samples * sizeof(int16_t);

    freeRecordingBuffer();
    m_recorded_samples = static_cast<int16_t *>(allocateRecordingBuffer(requested_bytes));
    const bool allocation_ok = (m_recorded_samples != NULL);

    Serial.println("[RECORD] Start recording...");
    Serial.printf("[RECORD] sample_rate=%u Hz\n", static_cast<unsigned>(SAMPLE_RATE));
    Serial.printf("[RECORD] channels=%u\n", static_cast<unsigned>(CHANNELS));
    Serial.printf("[RECORD] bit_depth=%u\n", static_cast<unsigned>(BIT_DEPTH));
    Serial.printf("[RECORD] requested_bytes=%u\n", static_cast<unsigned>(requested_bytes));
    Serial.printf("[RECORD] allocation=%s\n", allocation_ok ? "success" : "failed");

    if (!allocation_ok)
    {
        return false;
    }

    const int ring_size = m_sampler->getRingBufferSize();
    if (ring_size <= 0)
    {
        Serial.println("[RECORD] ERROR: invalid ring buffer size");
        freeRecordingBuffer();
        return false;
    }

    int previous_write_position = m_sampler->getCurrentWritePosition();
    const uint32_t backend_wait_start = millis();
    while (previous_write_position == m_sampler->getCurrentWritePosition())
    {
        if ((millis() - backend_wait_start) > BACKEND_READY_TIMEOUT_MS)
        {
            Serial.println("[RECORD] ERROR: audio backend not ready (no incoming samples)");
            freeRecordingBuffer();
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(CAPTURE_POLL_INTERVAL_MS));
    }

    const uint32_t capture_start_ms = millis();
    const uint32_t capture_deadline_ms = capture_start_ms + duration_ms + CAPTURE_EXTRA_TIMEOUT_MS;
    size_t write_offset = 0;

    while (write_offset < requested_samples)
    {
        const int current_write_position = m_sampler->getCurrentWritePosition();
        int delta = current_write_position - previous_write_position;
        if (delta < 0)
        {
            delta += ring_size;
        }

        if (delta > 0)
        {
            RingBufferAccessor *reader = m_sampler->getRingBufferReader();
            if (!reader)
            {
                Serial.println("[RECORD] ERROR: failed to create ring buffer reader");
                break;
            }

            reader->setIndex(previous_write_position);
            const size_t remaining_samples = requested_samples - write_offset;
            const size_t chunk_samples = std::min(static_cast<size_t>(delta), remaining_samples);

            for (size_t i = 0; i < chunk_samples; i++)
            {
                m_recorded_samples[write_offset++] = reader->getCurrentSample();
                reader->moveToNextSample();
            }

            delete reader;
            previous_write_position = current_write_position;
        }

        if (millis() > capture_deadline_ms)
        {
            Serial.println("[RECORD] WARN: capture timeout, storing partial recording");
            break;
        }

        if (write_offset < requested_samples)
        {
            vTaskDelay(pdMS_TO_TICKS(CAPTURE_POLL_INTERVAL_MS));
        }
    }

    m_recorded_sample_count = write_offset;
    Serial.printf("[RECORD] total_recorded_bytes=%u\n", static_cast<unsigned>(m_recorded_sample_count * sizeof(int16_t)));

    if (m_recorded_sample_count == 0)
    {
        Serial.println("[RECORD] ERROR: captured 0 bytes");
        freeRecordingBuffer();
        return false;
    }

    if (m_recorded_sample_count < requested_samples)
    {
        Serial.printf("[RECORD] WARN: captured less than requested (captured=%u samples, requested=%u samples)\n",
                      static_cast<unsigned>(m_recorded_sample_count),
                      static_cast<unsigned>(requested_samples));
    }

    return true;
}

bool AudioRecordPlayback::playRecorded()
{
    if (!m_output)
    {
        Serial.println("[PLAY] ERROR: output is NULL");
        return false;
    }

    if (!m_recorded_source)
    {
        Serial.println("[PLAY] ERROR: sample source is NULL");
        return false;
    }

    if (!m_recorded_samples || m_recorded_sample_count == 0)
    {
        Serial.println("[PLAY] ERROR: no recording available");
        return false;
    }

    m_recorded_source->reset(m_recorded_samples, m_recorded_sample_count);
    m_output->setSampleGenerator(m_recorded_source);

    Serial.printf("[PLAY] playing recorded audio (%u bytes)\n",
                  static_cast<unsigned>(m_recorded_sample_count * sizeof(int16_t)));
    return true;
}

void AudioRecordPlayback::clearRecording()
{
    freeRecordingBuffer();
    Serial.println("[RECORD] cleared recording buffer");
}

size_t AudioRecordPlayback::getRecordedBytes() const
{
    return m_recorded_sample_count * sizeof(int16_t);
}

uint32_t AudioRecordPlayback::getRecordedDurationMs() const
{
    if (m_recorded_sample_count == 0)
    {
        return 0;
    }
    return static_cast<uint32_t>((static_cast<uint64_t>(m_recorded_sample_count) * 1000ULL) / SAMPLE_RATE);
}

bool AudioRecordPlayback::hasValidRecording() const
{
    return (m_recorded_samples != NULL) && (m_recorded_sample_count > 0);
}
