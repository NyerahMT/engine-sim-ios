#include "../include/synthesizer.h"

#include "../include/utilities.h"

#include <algorithm>
#include <cassert>
#include <cmath>

Synthesizer::Synthesizer() {
    m_inputChannels = nullptr;
    m_inputChannelCount = 0;
    m_inputBufferSize = 0;
    m_inputWriteOffset = 0.0;
    m_inputSamplesRead = 0;

    m_audioBufferSize = 0;

    m_inputSampleRate = 0.0;
    m_audioSampleRate = 0.0;
    m_maximumInputLatency = -1.0;

    m_lastInputSampleOffset = 0.0;

    m_run = true;
    m_audioBufferedSamples = 0;
    m_thread = nullptr;
    m_filters = nullptr;
}

Synthesizer::~Synthesizer() {
    assert(m_inputChannels == nullptr);
    assert(m_thread == nullptr);
    assert(m_filters == nullptr);
}

void Synthesizer::initialize(const Parameters &p) {
    m_inputChannelCount = p.inputChannelCount;
    m_inputBufferSize = p.inputBufferSize;
    m_inputWriteOffset = p.inputBufferSize;
    m_audioBufferSize = p.audioBufferSize;
    m_inputSampleRate = p.inputSampleRate;
    m_audioSampleRate = p.audioSampleRate;
    m_audioParameters = p.initialAudioParameters;

    m_inputSamplesRead = 0;

    m_inputWriteOffset = 0;
    m_processed = true;
    m_audioBufferedSamples = 0;

    m_audioBuffer.initialize(p.audioBufferSize);
    m_inputChannels = new InputChannel[p.inputChannelCount];
    for (int i = 0; i < p.inputChannelCount; ++i) {
        m_inputChannels[i].transferBuffer = new float[p.inputBufferSize];
        m_inputChannels[i].data.initialize(p.inputBufferSize);
    }

    m_filters = new ProcessingFilters[p.inputChannelCount];
    for (int i = 0; i < p.inputChannelCount; ++i) {
        m_filters[i].airNoiseLowPass.setCutoffFrequency(
            m_audioParameters.airNoiseFrequencyCutoff, m_audioSampleRate);

        m_filters[i].derivative.m_dt = 1 / m_audioSampleRate;

        m_filters[i].inputDcFilter.setCutoffFrequency(10.0);
        m_filters[i].inputDcFilter.m_dt = 1 / m_audioSampleRate;

        m_filters[i].jitterFilter.initialize(
            10,
            m_audioParameters.inputSampleNoiseFrequencyCutoff,
            m_audioSampleRate);

        m_filters[i].antialiasing.setCutoffFrequency(1900.0f, m_audioSampleRate);
    }

    m_levelingFilter.p_target = m_audioParameters.levelerTarget;
    m_levelingFilter.p_maxLevel = m_audioParameters.levelerMaxGain;
    m_levelingFilter.p_minLevel = m_audioParameters.levelerMinGain;
    m_antialiasing.setCutoffFrequency(m_audioSampleRate * 0.45f, m_audioSampleRate);

    for (int i = 0; i < m_audioBufferSize; ++i) {
        m_audioBuffer.write(0);
    }
}

void Synthesizer::initializeImpulseResponse(
    const int16_t *impulseResponse,
    unsigned int samples,
    float volume,
    int index)
{
    unsigned int clippedLength = 0;
    for (unsigned int i = 0; i < samples; ++i) {
        if (std::abs(impulseResponse[i]) > 100) {
            clippedLength = i + 1;
        }
    }

    const unsigned int sampleCount = std::min(10000U, clippedLength);
    m_filters[index].convolution.initialize(sampleCount);
    for (unsigned int i = 0; i < sampleCount; ++i) {
        m_filters[index].convolution.getImpulseResponse()[i] =
            volume * impulseResponse[i] / INT16_MAX;
    }
}

void Synthesizer::startAudioRenderingThread() {
    m_run = true;
    m_thread = new std::thread(&Synthesizer::audioRenderingThread, this);
}

void Synthesizer::endAudioRenderingThread() {
    if (m_thread != nullptr) {
        m_run = false;
        endInputBlock();

        m_thread->join();
        delete m_thread;

        m_thread = nullptr;
    }
}

void Synthesizer::destroy() {
    m_audioBuffer.destroy();

    for (int i = 0; i < m_inputChannelCount; ++i) {
        m_inputChannels[i].data.destroy();
        m_filters[i].convolution.destroy();
    }

    delete[] m_inputChannels;
    delete[] m_filters;

    m_inputChannels = nullptr;
    m_filters = nullptr;

    m_inputChannelCount = 0;
}

int Synthesizer::readAudioOutput(int samples, int16_t *buffer) {
    int samplesConsumed = 0;
    {
        std::lock_guard<std::mutex> lock(m_lock0);

        const int newDataLength = m_audioBuffer.size();
        if (newDataLength >= samples) {
            m_audioBuffer.readAndRemove(samples, buffer);
        }
        else {
            m_audioBuffer.readAndRemove(newDataLength, buffer);
            memset(
                buffer + newDataLength,
                0,
                sizeof(int16_t) * ((size_t)samples - newDataLength));
        }

        samplesConsumed = std::min(samples, newDataLength);
        m_audioBufferedSamples = static_cast<int>(m_audioBuffer.size());
    }

    // The renderer waits while its small output reservoir is full. A consumer
    // draining it is the event that makes additional rendering possible.
    if (samplesConsumed > 0) m_cv0.notify_one();
    return samplesConsumed;
}

void Synthesizer::waitProcessed() {
    {
        std::unique_lock<std::mutex> lk(m_inputLock);
        m_cv0.wait(lk, [this] { return m_processed; });
    }
}

void Synthesizer::writeInput(const double *data) {
    std::lock_guard<std::mutex> lock(m_inputLock);
    m_inputWriteOffset += (double)m_audioSampleRate / m_inputSampleRate;
    if (m_inputWriteOffset >= (double)m_inputBufferSize) {
        m_inputWriteOffset -= (double)m_inputBufferSize;
    }

    for (int i = 0; i < m_inputChannelCount; ++i) {
        RingBuffer<float> &buffer = m_inputChannels[i].data;
        const double lastInputSample = m_inputChannels[i].lastInputSample;
        const size_t baseIndex = buffer.writeIndex();
        const double distance =
            inputDistance(m_inputWriteOffset, m_lastInputSampleOffset);
        double s =
            inputDistance(baseIndex, m_lastInputSampleOffset);
        for (; s <= distance; s += 1.0) {
            if (s >= m_inputBufferSize) s -= m_inputBufferSize;

            const double f = s / distance;
            const double sample = lastInputSample * (1 - f) + data[i] * f;

            buffer.write(m_filters[i].antialiasing.fast_f(static_cast<float>(sample)));
        }

        m_inputChannels[i].lastInputSample = data[i];
    }

    m_lastInputSampleOffset = m_inputWriteOffset;
}

void Synthesizer::endInputBlock() {
    std::unique_lock<std::mutex> lk(m_inputLock);

    for (int i = 0; i < m_inputChannelCount; ++i) {
        RingBuffer<float> &buffer = m_inputChannels[i].data;
        buffer.removeBeginning(m_inputSamplesRead);
        if (m_maximumInputLatency >= 0.0) {
            const int maximumSamples = static_cast<int>(
                std::round(m_maximumInputLatency * m_audioSampleRate));
            const int staleSamples = static_cast<int>(buffer.size()) - maximumSamples;
            if (staleSamples > 0) buffer.removeBeginning(staleSamples);
        }
    }

    if (m_inputChannelCount != 0) m_latency = m_inputChannels[0].data.size();
    
    m_inputSamplesRead = 0;
    m_processed = false;

    lk.unlock();
    m_cv0.notify_one();
}

void Synthesizer::audioRenderingThread() {
    while (m_run) {
        renderAudio();
    }
}

void Synthesizer::renderAudio() {
    std::unique_lock<std::mutex> inputLock(m_inputLock);
    // A second, modest reservoir decouples the render worker from the device
    // queue without adding a perceptible control-to-sound delay.
    constexpr int outputLeadSamples = 1024;
    const int targetOutputSamples = std::min(outputLeadSamples, m_audioBufferSize - 1);

    m_cv0.wait(inputLock, [this, outputLeadSamples] {
        const bool inputAvailable =
            m_inputChannels[0].data.size() > 0
            && m_audioBufferedSamples.load() < std::min(outputLeadSamples, m_audioBufferSize - 1);
        return !m_run || (inputAvailable && !m_processed);
    });

    const int n = std::min(
        std::max(0, targetOutputSamples - m_audioBufferedSamples.load()),
        (int)m_inputChannels[0].data.size());

    for (int i = 0; i < m_inputChannelCount; ++i) {
        m_inputChannels[i].data.read(n, m_inputChannels[i].transferBuffer);
    }
    
    m_inputSamplesRead = n;
    m_processed = true;

    inputLock.unlock();

    AudioParameters parameters;
    {
        std::lock_guard<std::mutex> lock(m_parameterLock);
        parameters = m_audioParameters;
    }
    std::lock_guard<std::mutex> renderLock(m_renderLock);
    for (int i = 0; i < m_inputChannelCount; ++i) {
        m_filters[i].airNoiseLowPass.setCutoffFrequency(
            static_cast<float>(parameters.airNoiseFrequencyCutoff), m_audioSampleRate);
        m_filters[i].jitterFilter.setJitterScale(parameters.inputSampleNoise);
    }

    {
        std::lock_guard<std::mutex> outputLock(m_lock0);
        for (int i = 0; i < n; ++i) {
            m_audioBuffer.write(renderAudio(i, parameters));
        }
        m_audioBufferedSamples = static_cast<int>(m_audioBuffer.size());
    }

    m_cv0.notify_one();
}

double Synthesizer::getLatency() const {
    std::lock_guard<std::mutex> lock(m_inputLock);
    return (double)m_latency / m_audioSampleRate;
}

int Synthesizer::inputDelta(int s1, int s0) const {
    return (s1 < s0)
        ? m_inputBufferSize - s0 + s1
        : s1 - s0;
}

double Synthesizer::inputDistance(double s1, double s0) const {
    return (s1 < s0)
        ? (double)m_inputBufferSize - s0 + s1
        : s1 - s0;
}

void Synthesizer::setInputSampleRate(double sampleRate) {
    if (sampleRate != m_inputSampleRate) {
        std::lock_guard<std::mutex> lock(m_inputLock);
        m_inputSampleRate = sampleRate;
    }
}

int16_t Synthesizer::renderAudio(int inputSample, const AudioParameters &parameters) {
    const float airNoise = parameters.airNoise;
    const float dF_F_mix = parameters.dF_F_mix;
    const float convAmount = parameters.convolution;

    float signal = 0;
    for (int i = 0; i < m_inputChannelCount; ++i) {
        const float r_0 = 2.0 * ((double)rand() / RAND_MAX) - 1.0;

        const float jitteredSample =
            m_filters[i].jitterFilter.fast_f(m_inputChannels[i].transferBuffer[inputSample]);

        const float f_in = jitteredSample;
        const float f_dc = m_filters[i].inputDcFilter.fast_f(f_in);
        const float f = f_in - f_dc;
        const float f_p = m_filters[i].derivative.f(f_in);

        const float noise = 2.0 * ((double)rand() / RAND_MAX) - 1.0;
        const float r =
            m_filters->airNoiseLowPass.fast_f(noise);
        const float r_mixed =
            airNoise * r + (1 - airNoise);

        float v_in =
            f_p * dF_F_mix
            + f * r_mixed * (1 - dF_F_mix);
        if (std::fpclassify(v_in) == FP_SUBNORMAL) {
            v_in = 0;
        }

        // Some callers intentionally run dry (and tests may do so before an
        // impulse response is installed). Avoid touching an uninitialized
        // convolution state in that case.
        const float v = (convAmount > 0.0f && m_filters[i].convolution.getSampleCount() > 0)
            ? convAmount * m_filters[i].convolution.f(v_in) + (1 - convAmount) * v_in
            : v_in;

        signal += v;
    }

    signal = m_antialiasing.fast_f(signal);

    m_levelingFilter.p_target = parameters.levelerTarget;
    const float v_leveled = m_levelingFilter.f(signal) * parameters.volume;
    int r_int = std::lround(v_leveled);
    if (r_int > INT16_MAX) {
        r_int = INT16_MAX;
    }
    else if (r_int < INT16_MIN) {
        r_int = INT16_MIN;
    }

    return static_cast<int16_t>(r_int);
}

double Synthesizer::getLevelerGain() {
    std::lock_guard<std::mutex> lock(m_renderLock);
    return m_levelingFilter.getAttenuation();
}

Synthesizer::AudioParameters Synthesizer::getAudioParameters() {
    std::lock_guard<std::mutex> lock(m_parameterLock);
    return m_audioParameters;
}

void Synthesizer::setAudioParameters(const AudioParameters &params) {
    std::lock_guard<std::mutex> lock(m_parameterLock);
    m_audioParameters = params;
}
