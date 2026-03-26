#include "AudioRecordPlayback.h"

#include <algorithm>
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
    const size_t ring_samples = static_cast<size_t>(m_sampler->getRingBufferSize());
    if (requested_samples > ring_samples)
    {
        Serial.printf("[RECORD] ERROR: duration too long for ring buffer (requested_samples=%u, ring_samples=%u)\n",
                      static_cast<unsigned>(requested_samples),
                      static_cast<unsigned>(ring_samples));
        return false;
    }

    if (requested_samples > (SIZE_MAX / sizeof(int16_t)))
    {
        Serial.println("[RECORD] ERROR: byte size overflow");
        return false;
    }

    const size_t requested_bytes = requested_samples * sizeof(int16_t);

    freeRecordingBuffer();
    m_recorded_samples = static_cast<int16_t *>(malloc(requested_bytes));
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

    const int start_index = m_sampler->getCurrentWritePosition();
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    const int end_index = m_sampler->getCurrentWritePosition();

    const size_t captured_samples = static_cast<size_t>((end_index - start_index + m_sampler->getRingBufferSize()) % m_sampler->getRingBufferSize());
    const size_t copy_samples = std::min(requested_samples, captured_samples);

    RingBufferAccessor *reader = m_sampler->getRingBufferReader();
    if (!reader)
    {
        Serial.println("[RECORD] ERROR: failed to create ring buffer reader");
        freeRecordingBuffer();
        return false;
    }

    reader->setIndex(start_index);
    for (size_t i = 0; i < copy_samples; i++)
    {
        m_recorded_samples[i] = reader->getCurrentSample();
        reader->moveToNextSample();
    }
    delete reader;

    m_recorded_sample_count = copy_samples;
    Serial.printf("[RECORD] total_recorded_bytes=%u\n", static_cast<unsigned>(m_recorded_sample_count * sizeof(int16_t)));

    if (copy_samples < requested_samples)
    {
        Serial.printf("[RECORD] WARN: captured less than requested (captured=%u samples, requested=%u samples)\n",
                      static_cast<unsigned>(copy_samples),
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
