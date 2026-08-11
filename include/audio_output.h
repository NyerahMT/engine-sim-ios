#ifndef ATG_ENGINE_SIM_AUDIO_OUTPUT_H
#define ATG_ENGINE_SIM_AUDIO_OUTPUT_H

#include <string>

class Simulator;
class Synthesizer;

class AudioOutput {
public:
    virtual ~AudioOutput() = default;
    virtual bool start(Simulator *simulator) = 0;
    virtual void pump() = 0;
    virtual bool loadImpulseResponse(Synthesizer &synthesizer, const std::string &path, float volume, int index) = 0;
    virtual void stop() = 0;
};

#endif
