#include "../include/performance_cluster.h"

#include "../include/units.h"
#include "../include/gauge.h"
#include "../include/constants.h"
#include "../include/engine_sim_application.h"

#include <algorithm>
#include <cmath>

PerformanceCluster::PerformanceCluster() {
    m_simulator = nullptr;

    m_timePerTimestepGauge = nullptr;
    m_fpsGauge = nullptr;
    m_simSpeedGauge = nullptr;
    m_simulationFrequencyGauge = nullptr;
    m_inputSamplesGauge = nullptr;
    m_audioLagGauge = nullptr;

    m_timePerTimestep = 0.0;
    m_filteredSimulationFrequency = 0.0;
    m_inputBufferUsage = 0.0;
    m_audioLatency = 0.0;
}

PerformanceCluster::~PerformanceCluster() {
}

void PerformanceCluster::initialize(
    EngineSimApplication *app)
{
    UiElement::initialize(app);

    constexpr float shortenAngle =
        static_cast<float>(
            units::angle(
                1.0,
                units::deg));

    /*
     * Real Time / delta Time
     */
    m_timePerTimestepGauge =
        addElement<LabeledGauge>();

    m_timePerTimestepGauge->m_title =
        "RT/dT";

    m_timePerTimestepGauge->m_unit =
        "%";

    m_timePerTimestepGauge
        ->m_spaceBeforeUnit =
            false;

    m_timePerTimestepGauge->m_precision =
        1;

    m_timePerTimestepGauge
        ->setLocalPosition(
            { 0, 0 });

    m_timePerTimestepGauge
        ->m_gauge
        ->m_min =
            0;

    m_timePerTimestepGauge
        ->m_gauge
        ->m_max =
            200;

    m_timePerTimestepGauge
        ->m_gauge
        ->m_minorStep =
            5;

    m_timePerTimestepGauge
        ->m_gauge
        ->m_majorStep =
            10;

    m_timePerTimestepGauge
        ->m_gauge
        ->m_maxMinorTick =
            1000000;

    m_timePerTimestepGauge
        ->m_gauge
        ->m_thetaMin =
            static_cast<float>(
                constants::pi)
            * 1.2f;

    m_timePerTimestepGauge
        ->m_gauge
        ->m_thetaMax =
            -static_cast<float>(
                constants::pi)
            * 0.2f;

    m_timePerTimestepGauge
        ->m_gauge
        ->m_needleWidth =
            4.0f;

    m_timePerTimestepGauge
        ->m_gauge
        ->m_gamma =
            1.0f;

    m_timePerTimestepGauge
        ->m_gauge
        ->m_needleKs =
            1000.0f;

    m_timePerTimestepGauge
        ->m_gauge
        ->m_needleKd =
            20.0f;

    m_timePerTimestepGauge
        ->m_gauge
        ->setBandCount(3);

    m_timePerTimestepGauge
        ->m_gauge
        ->setBand(
            {
                m_app->getBlue(),
                0.0f,
                50.0f,
                3.0f,
                6.0f,
                -shortenAngle,
                shortenAngle
            },
            0);

    m_timePerTimestepGauge
        ->m_gauge
        ->setBand(
            {
                m_app->getOrange(),
                50.0f,
                100.0f,
                3.0f,
                6.0f,
                shortenAngle,
                shortenAngle
            },
            1);

    m_timePerTimestepGauge
        ->m_gauge
        ->setBand(
            {
                m_app->getRed(),
                100.0f,
                200.0f,
                3.0f,
                6.0f,
                shortenAngle,
                -shortenAngle
            },
            2);

    /*
     * FPS
     */
    m_fpsGauge =
        addElement<LabeledGauge>();

    m_fpsGauge->m_title =
        "FPS";

    m_fpsGauge->m_unit =
        "";

    m_fpsGauge->m_precision =
        1;

    m_fpsGauge->setLocalPosition(
        { 0, 0 });

    m_fpsGauge->m_gauge->m_min =
        0;

    m_fpsGauge->m_gauge->m_max =
        120;

    m_fpsGauge->m_gauge->m_minorStep =
        1;

    m_fpsGauge->m_gauge->m_majorStep =
        15;

    m_fpsGauge
        ->m_gauge
        ->m_maxMinorTick =
            120;

    m_fpsGauge->m_gauge->m_thetaMin =
        static_cast<float>(
            constants::pi)
        * 1.2f;

    m_fpsGauge->m_gauge->m_thetaMax =
        -static_cast<float>(
            constants::pi)
        * 0.2f;

    m_fpsGauge
        ->m_gauge
        ->m_needleWidth =
            4.0f;

    m_fpsGauge->m_gauge->m_gamma =
        0.6f;

    m_fpsGauge
        ->m_gauge
        ->m_needleKs =
            1000.0f;

    m_fpsGauge
        ->m_gauge
        ->m_needleKd =
            20.0f;

    m_fpsGauge
        ->m_gauge
        ->setBandCount(4);

    m_fpsGauge
        ->m_gauge
        ->setBand(
            {
                m_app->getRed(),
                0,
                30,
                3.0f,
                6.0f,
                -shortenAngle,
                shortenAngle
            },
            0);

    m_fpsGauge
        ->m_gauge
        ->setBand(
            {
                m_app->getOrange(),
                30,
                58,
                3.0f,
                6.0f,
                shortenAngle,
                shortenAngle
            },
            1);

    m_fpsGauge
        ->m_gauge
        ->setBand(
            {
                m_app->getGreen(),
                58,
                62,
                3.0f,
                6.0f,
                shortenAngle,
                shortenAngle
            },
            2);

    m_fpsGauge
        ->m_gauge
        ->setBand(
            {
                m_app->getBlue(),
                62,
                120,
                3.0f,
                6.0f,
                shortenAngle,
                -shortenAngle
            },
            3);

    /*
     * Effective simulation speed.
     *
     * This is measured from actual physics work rather than reading
     * Simulator::m_simulationSpeed, which normally remains 1.0.
     */
    m_simSpeedGauge =
        addElement<LabeledGauge>();

    m_simSpeedGauge->m_title =
        "1 / SPEED";

    m_simSpeedGauge->m_unit =
        "";

    m_simSpeedGauge
        ->m_spaceBeforeUnit =
            false;

    m_simSpeedGauge->m_precision =
        1;

    m_simSpeedGauge
        ->setLocalPosition(
            { 0, 0 });

    m_simSpeedGauge
        ->m_gauge
        ->m_min =
            0;

    m_simSpeedGauge
        ->m_gauge
        ->m_max =
            10;

    m_simSpeedGauge
        ->m_gauge
        ->m_minorStep =
            0.5f;

    m_simSpeedGauge
        ->m_gauge
        ->m_majorStep =
            1.0f;

    m_simSpeedGauge
        ->m_gauge
        ->m_maxMinorTick =
            20;

    m_simSpeedGauge
        ->m_gauge
        ->m_thetaMin =
            static_cast<float>(
                constants::pi)
            * 1.2f;

    m_simSpeedGauge
        ->m_gauge
        ->m_thetaMax =
            -static_cast<float>(
                constants::pi)
            * 0.2f;

    m_simSpeedGauge
        ->m_gauge
        ->m_needleWidth =
            4.0f;

    m_simSpeedGauge
        ->m_gauge
        ->m_gamma =
            1.0f;

    m_simSpeedGauge
        ->m_gauge
        ->m_needleKs =
            1000.0f;

    m_simSpeedGauge
        ->m_gauge
        ->m_needleKd =
            20.0f;

    m_simSpeedGauge
        ->m_gauge
        ->setBandCount(0);

    /*
     * Audio latency
     */
    m_audioLagGauge =
        addElement<LabeledGauge>();

    m_audioLagGauge->m_title =
        "LATENCY";

    m_audioLagGauge->m_unit =
        "%";

    m_audioLagGauge
        ->m_spaceBeforeUnit =
            false;

    m_audioLagGauge->m_precision =
        1;

    m_audioLagGauge
        ->setLocalPosition(
            { 0, 0 });

    m_audioLagGauge
        ->m_gauge
        ->m_min =
            0;

    m_audioLagGauge
        ->m_gauge
        ->m_max =
            200;

    m_audioLagGauge
        ->m_gauge
        ->m_minorStep =
            5;

    m_audioLagGauge
        ->m_gauge
        ->m_majorStep =
            10;

    m_audioLagGauge
        ->m_gauge
        ->m_maxMinorTick =
            1000;

    m_audioLagGauge
        ->m_gauge
        ->m_thetaMin =
            static_cast<float>(
                constants::pi)
            * 0.8f;

    m_audioLagGauge
        ->m_gauge
        ->m_thetaMax =
            static_cast<float>(
                constants::pi)
            * 0.2f;

    m_audioLagGauge
        ->m_gauge
        ->m_needleWidth =
            4.0f;

    m_audioLagGauge
        ->m_gauge
        ->m_gamma =
            1.0f;

    m_audioLagGauge
        ->m_gauge
        ->m_needleKs =
            1000.0f;

    m_audioLagGauge
        ->m_gauge
        ->m_needleKd =
            20.0f;

    m_audioLagGauge
        ->m_gauge
        ->setBandCount(0);

    /*
     * Synth input buffer
     */
    m_inputSamplesGauge =
        addElement<LabeledGauge>();

    m_inputSamplesGauge->m_title =
        "IN. BUFFER";

    m_inputSamplesGauge->m_unit =
        "%";

    m_inputSamplesGauge
        ->m_spaceBeforeUnit =
            false;

    m_inputSamplesGauge->m_precision =
        1;

    m_inputSamplesGauge
        ->setLocalPosition(
            { 0, 0 });

    m_inputSamplesGauge
        ->m_gauge
        ->m_min =
            0;

    m_inputSamplesGauge
        ->m_gauge
        ->m_max =
            200;

    m_inputSamplesGauge
        ->m_gauge
        ->m_minorStep =
            5;

    m_inputSamplesGauge
        ->m_gauge
        ->m_majorStep =
            10;

    m_inputSamplesGauge
        ->m_gauge
        ->m_maxMinorTick =
            1000;

    m_inputSamplesGauge
        ->m_gauge
        ->m_thetaMin =
            static_cast<float>(
                constants::pi)
            * 1.2f;

    m_inputSamplesGauge
        ->m_gauge
        ->m_thetaMax =
            -static_cast<float>(
                constants::pi)
            * 0.2f;

    m_inputSamplesGauge
        ->m_gauge
        ->m_needleWidth =
            4.0f;

    m_inputSamplesGauge
        ->m_gauge
        ->m_gamma =
            1.0f;

    m_inputSamplesGauge
        ->m_gauge
        ->m_needleKs =
            1000.0f;

    m_inputSamplesGauge
        ->m_gauge
        ->m_needleKd =
            20.0f;

    m_inputSamplesGauge
        ->m_gauge
        ->setBandCount(0);

    /*
     * Simulation frequency
     */
    m_simulationFrequencyGauge =
        addElement<LabeledGauge>();

    m_simulationFrequencyGauge->m_title =
        "FREQUENCY";

    m_simulationFrequencyGauge->m_unit =
        "hz";

    m_simulationFrequencyGauge->m_precision =
        0;

    m_simulationFrequencyGauge
        ->setLocalPosition(
            { 0, 0 });

    m_simulationFrequencyGauge
        ->m_gauge
        ->m_min =
            1000;

    m_simulationFrequencyGauge
        ->m_gauge
        ->m_max =
            51000;

    m_simulationFrequencyGauge
        ->m_gauge
        ->m_minorStep =
            1000;

    m_simulationFrequencyGauge
        ->m_gauge
        ->m_majorStep =
            10000;

    m_simulationFrequencyGauge
        ->m_gauge
        ->m_maxMinorTick =
            50000;

    m_simulationFrequencyGauge
        ->m_gauge
        ->m_thetaMin =
            static_cast<float>(
                constants::pi)
            * 1.2f;

    m_simulationFrequencyGauge
        ->m_gauge
        ->m_thetaMax =
            -static_cast<float>(
                constants::pi)
            * 0.2f;

    m_simulationFrequencyGauge
        ->m_gauge
        ->m_needleWidth =
            4.0f;

    m_simulationFrequencyGauge
        ->m_gauge
        ->m_gamma =
            0.9f;

    m_simulationFrequencyGauge
        ->m_gauge
        ->m_needleKs =
            1000.0f;

    m_simulationFrequencyGauge
        ->m_gauge
        ->m_needleKd =
            20.0f;

    m_simulationFrequencyGauge
        ->m_gauge
        ->setBandCount(1);

    m_simulationFrequencyGauge
        ->m_gauge
        ->setBand(
            {
                m_app->getForegroundColor(),
                11025,
                44100,
                3.0f,
                6.0f,
                shortenAngle,
                shortenAngle
            },
            0);
}

void PerformanceCluster::destroy() {
    UiElement::destroy();
}

void PerformanceCluster::update(float dt) {
    UiElement::update(dt);

    if (m_simulator == nullptr) {
        return;
    }

    const double simulationFrequency =
        m_simulator
            ->getSimulationFrequency();

    const double configuredSimulationSpeed =
        m_simulator
            ->getSimulationSpeed();

    m_filteredSimulationFrequency =
        0.9
            * m_filteredSimulationFrequency
        + 0.1
            * simulationFrequency
            * configuredSimulationSpeed;

    const int steps =
        m_simulator
            ->getFrameIterationCount();

    if (steps > 0) {
        const double frameProcessingSeconds =
            m_simulator
                ->getAverageProcessingTime()
            / 1000000.0;

        const double actualTimePerStep =
            frameProcessingSeconds
            / static_cast<double>(
                steps);

        if (
            std::isfinite(
                actualTimePerStep)
            && actualTimePerStep >= 0.0)
        {
            addTimePerTimestepSample(
                actualTimePerStep);
        }
    }

    const double inputLatency =
        m_simulator
            ->getSynthesizerInputLatency();

    const double targetLatency =
        m_simulator
            ->getTargetSynthesizerLatency();

    if (targetLatency > 0.0) {
        addInputBufferUsageSample(
            inputLatency
            / targetLatency);
    }

    const double outputLatency =
        m_simulator
            ->getSynthesizerOutputLatency();

    addAudioLatencySample(
        outputLatency);
}

void PerformanceCluster::render() {
    if (m_simulator == nullptr) {
        return;
    }

    Grid grid;
    grid.h_cells = 3;
    grid.v_cells = 2;

    double rtDtPercent =
        0.0;

    if (
        m_filteredSimulationFrequency
        > 1.0)
    {
        const double idealTimePerStep =
            1.0
            / m_filteredSimulationFrequency;

        rtDtPercent =
            (
                m_timePerTimestep
                / idealTimePerStep)
            * 100.0;
    }

    if (
        !std::isfinite(
            rtDtPercent)
        || rtDtPercent < 0.0)
    {
        rtDtPercent =
            0.0;
    }

    m_timePerTimestepGauge->m_bounds =
        grid.get(
            m_bounds,
            1,
            0);

    m_timePerTimestepGauge
        ->m_gauge
        ->m_value =
            static_cast<float>(
                std::min(
                    rtDtPercent,
                    200.0));

    m_fpsGauge->m_bounds =
        grid.get(
            m_bounds,
            0,
            0);

    m_fpsGauge
        ->m_gauge
        ->m_value =
            m_app
                ->getAverageFramerate();

    m_simSpeedGauge->m_bounds =
        grid.get(
            m_bounds,
            2,
            0);

    /*
     * Measure the effective simulation rate.
     *
     * Each completed physics step represents one simulator timestep.
     * Comparing that simulated duration against the host frame duration
     * tells us how quickly simulation time is actually advancing.
     *
     * 1 / SPEED:
     *
     *     1.0 = realtime
     *     2.0 = simulation advancing at half realtime
     *     0.5 = simulation advancing at twice realtime
     */
    double effectiveSimulationSpeed =
        0.0;

    const int completedSteps =
        m_simulator
            ->getFrameIterationCount();

    const double timestep =
        m_simulator
            ->getTimestep();

    /*
     * UI update dt is clamped by the application to 1/30 on large
     * scheduling hitches, but under ordinary operation it represents
     * the host-frame cadence closely enough for this diagnostic gauge.
     */
    const double frameDt =
        1.0
        / std::max(
            1.0f,
            m_app
                ->getAverageFramerate());

    if (
        completedSteps > 0
        && timestep > 0.0
        && frameDt > 0.0)
    {
        effectiveSimulationSpeed =
            (
                static_cast<double>(
                    completedSteps)
                * timestep)
            / frameDt;
    }

    float reciprocalSpeed =
        0.0f;

    if (
        std::isfinite(
            effectiveSimulationSpeed)
        && effectiveSimulationSpeed
            > 0.000001)
    {
        reciprocalSpeed =
            static_cast<float>(
                1.0
                / effectiveSimulationSpeed);
    }

    m_simSpeedGauge
        ->m_gauge
        ->m_value =
            std::clamp(
                reciprocalSpeed,
                0.0f,
                10.0f);

    m_audioLagGauge->m_bounds =
        grid.get(
            m_bounds,
            0,
            1);

    m_audioLagGauge
        ->m_gauge
        ->m_value =
            static_cast<float>(
                m_audioLatency
                * 1000.0);

    m_inputSamplesGauge->m_bounds =
        grid.get(
            m_bounds,
            1,
            1);

    m_inputSamplesGauge
        ->m_gauge
        ->m_value =
            static_cast<float>(
                m_inputBufferUsage
                * 100.0);

    m_simulationFrequencyGauge->m_bounds =
        grid.get(
            m_bounds,
            2,
            1);

    m_simulationFrequencyGauge
        ->m_gauge
        ->m_value =
            static_cast<float>(
                m_simulator
                    ->getSimulationFrequency());

    UiElement::render();
}

void PerformanceCluster::addTimePerTimestepSample(
    double sample)
{
    constexpr double r =
        0.95;

    m_timePerTimestep =
        r * m_timePerTimestep
        + (1.0 - r)
            * sample;
}

void PerformanceCluster::addAudioLatencySample(
    double sample)
{
    constexpr double r =
        0.95;

    m_audioLatency =
        r * m_audioLatency
        + (1.0 - r)
            * sample;
}

void PerformanceCluster::addInputBufferUsageSample(
    double sample)
{
    constexpr double r =
        0.95;

    m_inputBufferUsage =
        r * m_inputBufferUsage
        + (1.0 - r)
            * sample;
}
