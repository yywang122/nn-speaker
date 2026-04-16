#ifndef _detect_record_word_state_h_
#define _detect_record_word_state_h_

#include "States.h"

class I2SSampler;
class NeuralNetwork;
class AudioProcessor;
class AudioRecorder;
class IndicatorLight;
class Speaker;

class DetectRecordWordState : public State
{
public:
    DetectRecordWordState(I2SSampler    *sampler,
                          AudioRecorder *recorder,
                          IndicatorLight *light,
                          Speaker        *speaker);

    void enterState() override;
    bool run()        override;
    void exitState()  override;

private:
 
    I2SSampler     *m_sample_provider;
    AudioRecorder  *m_recorder;
    IndicatorLight *m_indicator_light;
    Speaker        *m_speaker;

    NeuralNetwork  *m_nn;            // loaded with model_recordword in enterState()
    AudioProcessor *m_audio_processor;

    Phase m_phase;
    int   m_detection_count;        // detection confirms word (was >1)
};

#endif // _detect_record_word_state_h_
