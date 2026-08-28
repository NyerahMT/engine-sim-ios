#include "../include/sdl_audio_output.h"
#include "../include/sdl_audio_util.h"
#include "../include/simulator.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace {

constexpr std::uint32_t WaveformBufferSize = 4096;

std::array<std::int16_t, WaveformBufferSize> g_waveformSamples{};
std::atomic<std::uint32_t> g_waveformWrite{0};
std::atomic<std::uint32_t> g_waveformRead{0};

void resetWaveformTap() {
    g_waveformWrite.store(
        0,
        std::memory_order_relaxed);

    g_waveformRead.store(
        0,
        std::memory_order_relaxed);
}

void pushWaveformSample(std::int16_t sample) {
    const std::uint32_t write =
        g_waveformWrite.load(
            std::memory_order_relaxed);

    const std::uint32_t next =
        (write + 1)
        % WaveformBufferSize;

    const std::uint32_t read =
        g_waveformRead.load(
            std::memory_order_acquire);

    /*
     * The waveform display is cosmetic.
     *
     * Never allow it to stall the real-time audio thread.
     * If the UI falls behind, simply discard the newest
     * visualization sample.
     */
    if (next == read) {
        return;
    }

    g_waveformSamples[write] = sample;

    g_waveformWrite.store(
        next,
        std::memory_order_release);
}

}

/*
 * iOS waveform bridge.
 *
 * Original Engine Simulator fed the waveform graph using
 * rendered PCM audio samples.
 *
 * The SDL audio port moved PCM consumption into its own audio
 * thread, so the oscilloscope no longer had access to those
 * samples.
 *
 * This exposes a small non-blocking visualization tap.
 *
 * Audio thread = producer
 * UI thread    = consumer
 */
int EngineSimReadIosWaveformSamples(
    std::int16_t *destination,
    int maxSamples)
{
    if (
        destination == nullptr
        || maxSamples <= 0)
    {
        return 0;
    }

    int copied = 0;

    std::uint32_t read =
        g_waveformRead.load(
            std::memory_order_relaxed);

    const std::uint32_t write =
        g_waveformWrite.load(
            std::memory_order_acquire);

    while (
        read != write
        && copied < maxSamples)
    {
        destination[copied++] =
            g_waveformSamples[read];

        read =
            (read + 1)
            % WaveformBufferSize;
    }

    g_waveformRead.store(
        read,
        std::memory_order_release);

    return copied;
}

bool SdlAudioOutput::start(Simulator *simulator) {
    std::lock_guard<std::mutex> lock(m_lifecycleMutex);

    stopLocked();

    if (simulator == nullptr) {
        return false;
    }

    /*
     * Engine Simulator's synthesizer produces 44.1 kHz PCM.
     *
     * Keep the SDL stream in that native clock domain and let
     * SDL perform final conversion to whatever the physical
     * device uses.
     */
    const SDL_AudioSpec spec = {
        SDL_AUDIO_S16,
        1,
        44100
    };

    m_stream =
        SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
            &spec,
            nullptr,
            nullptr);

    if (m_stream == nullptr) {
        return false;
    }

    m_simulator = simulator;

    m_diagnostics =
        SDL_GetHintBoolean(
            "ENGINE_SIM_AUDIO_DIAGNOSTICS",
            false);

    m_lastDiagnosticTick =
        SDL_GetTicks();

    m_pcmFrames = 0;
    m_silenceFrames = 0;
    m_peakQueuedBytes = 0;

    resetWaveformTap();

    if (m_diagnostics) {
        SDL_AudioSpec source = {};
        SDL_AudioSpec destination = {};

        if (
            SDL_GetAudioStreamFormat(
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

            if (
                now - m_lastDiagnosticTick
                >= 1000)
            {
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
                        ? m_simulator
                            ->getSynthesizerInputLatency()
                        : 0.0,
                    m_simulator != nullptr
                        ? m_simulator
                            ->getSynthesizerOutputLatency()
                        : 0.0,
                    std::max(
                        0,
                        queuedBytes)
                        / (
                            44100.0
                            * sizeof(
                                std::int16_t)),
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
    if (
        m_stream == nullptr
        || m_simulator == nullptr)
    {
        return;
    }

    constexpr int chunkFrames = 512;

    /*
     * Keep roughly 46 ms of PCM queued.
     *
     * 2048 / 44100 ~= 0.0464 seconds.
     *
     * This is the queue size currently giving the iOS port
     * reliable underrun protection while retaining responsive
     * throttle/audio behavior.
     */
    constexpr int targetFrames = 2048;

    constexpr int targetBytes =
        targetFrames
        * static_cast<int>(
            sizeof(std::int16_t));

    int queuedBytes =
        SDL_GetAudioStreamQueued(
            m_stream);

    if (queuedBytes < 0) {
        return;
    }

    while (queuedBytes < targetBytes) {
        std::array<
            std::int16_t,
            chunkFrames> samples{};

        const int frames =
            std::min(
                chunkFrames,
                (
                    targetBytes
                    - queuedBytes)
                    / static_cast<int>(
                        sizeof(
                            std::int16_t)));

        /*
         * readAudioOutput zero-fills short reads.
         *
         * Queueing the complete chunk preserves a stable
         * device lead during startup and transient
         * synthesizer underruns.
         */
        const int pcmFrames =
            m_simulator->readAudioOutput(
                frames,
                samples.data());

        m_pcmFrames +=
            std::max(
                0,
                pcmFrames);

        m_silenceFrames +=
            frames
            - std::max(
                0,
                pcmFrames);

        /*
         * Restore the original Engine Simulator waveform
         * behavior.
         *
         * Ange's desktop implementation plotted the actual
         * rendered PCM stream and kept one sample out of
         * every four.
         *
         * Capture the samples here BEFORE SDL performs any
         * final device-rate conversion.
         *
         * Only real synthesized frames are captured. Any
         * zero-filled underrun protection is ignored.
         */
        const int renderedFrames =
            std::min(
                frames,
                std::max(
                    0,
                    pcmFrames));

        for (
            int i = 0;
            i < renderedFrames;
            i += 4)
        {
            pushWaveformSample(
                samples[i]);
        }

        const int bytes =
            frames
            * static_cast<int>(
                sizeof(std::int16_t));

        if (
            !SDL_PutAudioStreamData(
                m_stream,
                samples.data(),
                bytes))
        {
            return;
        }

        queuedBytes += bytes;

        m_peakQueuedBytes =
            std::max(
                m_peakQueuedBytes,
                queuedBytes);
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

    resetWaveformTap();
}
