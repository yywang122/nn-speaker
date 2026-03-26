#ifndef __audio_record_playback_h__
#define __audio_record_playback_h__

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include "SampleSource.h"

class I2SSampler;
class I2SOutput;

class RecordedAudioSampleSource;

class AudioRecordPlayback
{
private:
    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr uint8_t CHANNELS = 1;
    static constexpr uint8_t BIT_DEPTH = 16;
    static constexpr uint32_t MAX_DURATION_MS = 60000;
    static constexpr uint32_t CAPTURE_POLL_INTERVAL_MS = 10;
    static constexpr uint32_t BACKEND_READY_TIMEOUT_MS = 600;
    static constexpr uint32_t CAPTURE_EXTRA_TIMEOUT_MS = 2000;

    I2SSampler *m_sampler;
    I2SOutput *m_output;
    int16_t *m_recorded_samples;
    size_t m_recorded_sample_count;
    RecordedAudioSampleSource *m_recorded_source;

    void freeRecordingBuffer();

public:
    AudioRecordPlayback(I2SSampler *sampler, I2SOutput *output);
    ~AudioRecordPlayback();

    bool record(uint32_t duration_ms);
    bool playRecorded();
    void clearRecording();
    size_t getRecordedBytes() const;
    bool hasValidRecording() const;
};

class RecordedAudioSampleSource : public SampleSource
{
private:
    const int16_t *m_samples;
    size_t m_total_samples;
    size_t m_position;

public:
    RecordedAudioSampleSource();
    void reset(const int16_t *samples, size_t total_samples);
    int getFrames(Frame_t *frames, int number_frames) override;
    bool available() override;
};

#endif
