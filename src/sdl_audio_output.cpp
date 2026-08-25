#include "../include/sdl_audio_output.h"
#include "../include/sdl_audio_util.h"
#include "../include/simulator.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>

bool SdlAudioOutput::start(Simulator *simulator) {
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);
    stopLocked();

    if (simulator == nullptr) return false;

    // EngineSim synthesizes natively at 44.1 kHz mono S16.
    // Keep the stream in that clock domain and let SDL perform
    // any final conversion required by the iOS audio device.
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
                &destination)) {

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
        stop();
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
                    "audio: pcm=%llu silence=%llu input=%.3fs output=%.3fs stream=%.3fs peak-queue=%dB\n",
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
     * IMPORTANT:
     *
     * EngineSim's native Synthesizer currently maintains an
     * internal rendered-output reservoir of 1024 samples.
     *
     * Our previous iOS experiment asked SDL to maintain 4096
     * samples. That meant the consumer could drain EngineSim's
     * entire 1024-sample reservoir and immediately request more
     * before the synthesis worker had replenished it.
     *
     * readAudioOutput() intentionally zero-fills short reads.
     *
     * Result:
     *
     *     real PCM
     *     silence
     *     real PCM
     *     silence
     *
     * ...which sounds exactly like clicking and popping.
     *
     * Keep the SDL lead synchronized with the actual producer
     * capacity until we intentionally enlarge both reservoirs.
     */

    constexpr int chunkFrames = 512;
    constexpr int targetFrames = 1024;

    constexpr int targetBytes =
        targetFrames
        * static_cast<int>(
            sizeof(std::int16_t));

    int queuedBytes =
        SDL_GetAudioStreamQueued(
            m_stream);

    if (queuedBytes < 0) return;

    while (queuedBytes < targetBytes) {
        std::array<
            std::int16_t,
            chunkFrames> samples{};

        const int frames =
            std::min(
                chunkFrames,
                (targetBytes - queuedBytes)
                    / static_cast<int>(
                        sizeof(std::int16_t)));

        const int pcmFrames =
            m_simulator->readAudioOutput(
                frames,
                samples.data());

        const int validFrames =
            std::max(
                0,
                pcmFrames);

        m_pcmFrames += validFrames;
        m_silenceFrames += frames - validFrames;

        /*
         * If the synthesizer genuinely ran dry, don't repeatedly
         * consume another artificial silent chunk in the same
         * fill loop.
         *
         * Queue this chunk, return to the 1 ms audio worker loop,
         * and give EngineSim's synthesis thread an opportunity to
         * replenish its reservoir.
         */

        const int bytes =
            frames
            * static_cast<int>(
                sizeof(std::int16_t));

        if (!SDL_PutAudioStreamData(
                m_stream,
                samples.data(),
                bytes)) {

            return;
        }

        queuedBytes += bytes;

        m_peakQueuedBytes =
            std::max(
                m_peakQueuedBytes,
                queuedBytes);

        if (validFrames < frames) {
            break;
        }
    }
}

bool SdlAudioOutput::loadImpulseResponse(
    Synthesizer &synthesizer,
    const std::string &path,
    float volume,
    int index) {

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
