#include "../include/sdl_audio_output.h"
#include "../include/sdl_audio_util.h"
#include "../include/simulator.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>

#if defined(__APPLE__)
#include <pthread.h>
#include <pthread/qos.h>
#endif

bool SdlAudioOutput::start(Simulator *simulator) {
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    stopLocked();

    if (simulator == nullptr) return false;

    /*
     * High-load iOS engines can briefly produce audio input faster
     * than the synthesizer/convolution worker can consume it.
     *
     * The old iOS value was 30 ms. That was low enough that useful
     * engine waveform data could be discarded during a transient
     * synth stall.
     *
     * Give the producer 120 ms of input history. This matches the
     * spirit of our ~93 ms output reservoir without changing any
     * actual EngineSim synthesis or physics.
     */
    simulator->setMaximumSynthesizerInputLatency(0.120);

    const SDL_AudioSpec spec = {
        SDL_AUDIO_S16,
        1,
        44100
    };

    m_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &spec,
        nullptr,
        nullptr);

    if (m_stream == nullptr) return false;

    m_simulator = simulator;

    m_diagnostics =
        SDL_GetHintBoolean(
            "ENGINE_SIM_AUDIO_DIAGNOSTICS",
            false);

    m_lastDiagnosticTick = SDL_GetTicks();
    m_pcmFrames = 0;
    m_silenceFrames = 0;
    m_peakQueuedBytes = 0;

    if (m_diagnostics) {
        SDL_AudioSpec source = {};
        SDL_AudioSpec destination = {};

        if (SDL_GetAudioStreamFormat(
                m_stream,
                &source,
                &destination))
        {
            std::fprintf(
                stderr,
                "audio: stream=%dHz/%dch -> device=%dHz/%dch\n",
                source.freq,
                source.channels,
                destination.freq,
                destination.channels);
        }
    }

    if (!SDL_ResumeAudioStreamDevice(m_stream)) {
        stopLocked();
        return false;
    }

    m_running = true;

    m_thread =
        std::thread(
            &SdlAudioOutput::audioThread,
            this);

    return true;
}

void SdlAudioOutput::audioThread() {
#if defined(__APPLE__)
    /*
     * Keep the PCM feeder out of ordinary utility scheduling.
     *
     * Audio dropouts are much more noticeable than a frame arriving
     * a millisecond late, so give this worker user-interactive QoS.
     */
    pthread_set_qos_class_self_np(
        QOS_CLASS_USER_INTERACTIVE,
        0);
#endif

    while (m_running) {
        fillStream();

        if (m_diagnostics) {
            const std::uint64_t now =
                SDL_GetTicks();

            if (now - m_lastDiagnosticTick >= 1000) {
                const int queuedBytes =
                    SDL_GetAudioStreamQueued(
                        m_stream);

                std::fprintf(
                    stderr,
                    "audio: pcm=%llu short=%llu input=%.3fs output=%.3fs stream=%.3fs peak-queue=%dB\n",
                    static_cast<unsigned long long>(
                        m_pcmFrames),
                    static_cast<unsigned long long>(
                        m_silenceFrames),
                    m_simulator != nullptr
                        ? m_simulator->getSynthesizerInputLatency()
                        : 0.0,
                    m_simulator != nullptr
                        ? m_simulator->getSynthesizerOutputLatency()
                        : 0.0,
                    std::max(0, queuedBytes)
                        / (44100.0 * sizeof(std::int16_t)),
                    m_peakQueuedBytes);

                m_pcmFrames = 0;
                m_silenceFrames = 0;
                m_peakQueuedBytes = 0;
                m_lastDiagnosticTick = now;
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(1));
    }
}

void SdlAudioOutput::fillStream() {
    if (m_stream == nullptr || m_simulator == nullptr) {
        return;
    }

    /*
     * Matched to the 4096-sample Synthesizer reservoir.
     *
     * 4096 samples @ 44.1 kHz ~= 92.9 ms.
     *
     * Read in 512-frame pieces so the synthesizer worker gets
     * opportunities to refill between consumer requests.
     */
    constexpr int chunkFrames = 512;
    constexpr int targetFrames = 4096;

    constexpr int targetBytes =
        targetFrames
        * static_cast<int>(
            sizeof(std::int16_t));

    int queuedBytes =
        SDL_GetAudioStreamQueued(
            m_stream);

    if (queuedBytes < 0) return;

    while (
        m_running
        && queuedBytes < targetBytes)
    {
        std::array<
            std::int16_t,
            chunkFrames> samples{};

        const int missingFrames =
            (targetBytes - queuedBytes)
            / static_cast<int>(
                sizeof(std::int16_t));

        const int requestedFrames =
            std::min(
                chunkFrames,
                missingFrames);

        if (requestedFrames <= 0) break;

        const int producedFrames =
            m_simulator->readAudioOutput(
                requestedFrames,
                samples.data());

        const int validFrames =
            std::clamp(
                producedFrames,
                0,
                requestedFrames);

        m_pcmFrames += validFrames;
        m_silenceFrames +=
            requestedFrames - validFrames;

        /*
         * readAudioOutput() zero-fills its unused tail.
         *
         * Never forward that artificial silence to SDL. A hard
         * transition to digital zero is exactly the kind of edge
         * that turns a minor underrun into an audible click.
         */
        if (validFrames <= 0) {
            break;
        }

        const int validBytes =
            validFrames
            * static_cast<int>(
                sizeof(std::int16_t));

        if (!SDL_PutAudioStreamData(
                m_stream,
                samples.data(),
                validBytes))
        {
            return;
        }

        queuedBytes += validBytes;

        m_peakQueuedBytes =
            std::max(
                m_peakQueuedBytes,
                queuedBytes);

        if (validFrames < requestedFrames) {
            break;
        }
    }
}

bool SdlAudioOutput::loadImpulseResponse(
    Synthesizer &synthesizer,
    const std::string &path,
    float volume,
    int index)
{
    return loadSdlImpulseResponse(
        synthesizer,
        path,
        volume,
        index);
}

void SdlAudioOutput::stop() {
    std::lock_guard<std::mutex> lock(
        m_lifecycleMutex);

    stopLocked();
}

void SdlAudioOutput::stopLocked() {
    m_running = false;

    if (m_thread.joinable()) {
        m_thread.join();
    }

    if (m_stream != nullptr) {
        SDL_DestroyAudioStream(
            m_stream);
    }

    m_stream = nullptr;
    m_simulator = nullptr;
}
