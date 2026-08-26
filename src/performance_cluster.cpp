#include "../include/performance_cluster.h"

#include "../include/units.h"
#include "../include/gauge.h"
#include "../include/constants.h"
#include "../include/engine_sim_application.h"
#include "../include/geometry_generator.h"

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

    /*
     * This cluster now owns the left/right touch regions
     * around the 1 / SPEED gauge.
     */
    m_checkMouse = true;
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
     *
     * 100% means physics processing consumes exactly as much
     * wall-clock time as the simulated timestep budget.
     *
     * < 100 = faster than real time
     * > 100 = unable to keep up
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
     * Simulation speed
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
            1000;

    m_simSpeedGauge
        ->m_gauge
        ->m_minorStep =
            50;

    m_simSpeedGauge
        ->m_gauge
        ->m_majorStep =
            100;

    m_simSpeedGauge
        ->m_gauge
        ->m_maxMinorTick =
            1000;

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
        "ms";

    m_audioLagGauge
        ->m_spaceBeforeUnit =
            true;

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
    m_mouseBounds = m_bounds;

    UiElement::update(dt);

    if (m_simulator == nullptr) {
        return;
    }

    const double simulationFrequency =
        m_simulator
            ->getSimulationFrequency();

    const double simulationSpeed =
        m_simulator
            ->getSimulationSpeed();

    m_filteredSimulationFrequency =
        0.9
            * m_filteredSimulationFrequency
        + 0.1
            * simulationFrequency
            * simulationSpeed;

    /*
     * Simulator::getAverageProcessingTime() is the filtered
     * frame physics time in MICROSECONDS.
     *
     * Divide it by the number of simulation steps completed in
     * that frame to get actual wall-clock seconds per timestep.
     *
     * This was the missing feed that left RT/dT dead.
     */
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

    /*
     * Feed the two audio health gauges directly too.
     */
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


void PerformanceCluster::onMouseClick(
    const Point &mouseLocal)
{
    UiElement::onMouseClick(
        mouseLocal);

    if (m_simulator == nullptr) {
        return;
    }

    const Bounds cell =
        simulationSpeedCellBounds();

    const Bounds left =
        cell.horizontalSplit(
            0.0f,
            0.18f);

    const Bounds right =
        cell.horizontalSplit(
            0.82f,
            1.0f);

    if (left.overlaps(mouseLocal)) {
        changeSimulationTimeDivision(-1);
    }
    else if (right.overlaps(mouseLocal)) {
        changeSimulationTimeDivision(1);
    }
}

Bounds PerformanceCluster::simulationSpeedCellBounds() const
{
    Grid grid;

    grid.h_cells = 3;
    grid.v_cells = 2;

    return grid.get(
        m_bounds,
        2,
        0);
}

void PerformanceCluster::changeSimulationTimeDivision(
    int direction)
{
    if (m_simulator == nullptr) {
        return;
    }

    const double simulationSpeed =
        m_simulator
            ->getSimulationSpeed();

    /*
     * The gauge displays 1 / SPEED.
     *
     * Engine Simulator's actual simulator setting is the
     * reciprocal:
     *
     * display 1    -> speed 1.0
     * display 10   -> speed 0.1
     * display 100  -> speed 0.01
     * display 1000 -> speed 0.001
     *
     * Keep realtime (1) as the lower stop so the user can
     * always return to normal time.
     */
    double division =
        simulationSpeed > 0.0
            ? 1.0 / simulationSpeed
            : 1.0;

    double nextDivision =
        division;

    if (direction > 0) {
        if (division < 5.0) {
            nextDivision = 10.0;
        }
        else if (division < 50.0) {
            nextDivision = 100.0;
        }
        else {
            nextDivision = 1000.0;
        }
    }
    else if (direction < 0) {
        if (division > 500.0) {
            nextDivision = 100.0;
        }
        else if (division > 50.0) {
            nextDivision = 10.0;
        }
        else {
            nextDivision = 1.0;
        }
    }

    m_simulator
        ->setSimulationSpeed(
            1.0 / nextDivision);
}

void PerformanceCluster::drawTimeChevron(
    const Bounds &bounds,
    bool pointsRight)
{
    const Point center =
        getRenderPoint(
            bounds.getPosition(
                Bounds::center));

    const float size =
        pixelsToUnits(
            std::fmin(
                bounds.width(),
                bounds.height())
            * 0.25f);

    const float halfWidth =
        size * 0.65f;

    const float halfHeight =
        size;

    const float baseX =
        center.x
        + (
            pointsRight
                ? -halfWidth
                : halfWidth
          );

    const float pointX =
        center.x
        + (
            pointsRight
                ? halfWidth
                : -halfWidth
          );

    GeometryGenerator::Line2dParameters line;

    line.lineWidth =
        pixelsToUnits(
            3.0f);

    GeometryGenerator *generator =
        m_app
            ->getGeometryGenerator();

    GeometryGenerator::GeometryIndices
        chevron;

    generator->startShape();

    line.x0 =
        baseX;

    line.y0 =
        center.y
        - halfHeight;

    line.x1 =
        pointX;

    line.y1 =
        center.y;

    generator->generateLine2d(
        line);

    line.x0 =
        pointX;

    line.y0 =
        center.y;

    line.x1 =
        baseX;

    line.y1 =
        center.y
        + halfHeight;

    generator->generateLine2d(
        line);

    generator->endShape(
        &chevron);

    resetShader();

    m_app
        ->getShaders()
        ->SetBaseColor(
            m_app
                ->getForegroundColor());

    m_app->drawGenerated(
        chevron,
        0x11,
        m_app
            ->getShaders()
            ->GetUiFlags());
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

    const Bounds simulationSpeedCell =
        grid.get(
            m_bounds,
            2,
            0);

    /*
     * Leave room on either side of the gauge for the same
     * chevron interaction used by the gear selector, rotated
     * ninety degrees.
     */
    /*
     * Keep the gauge itself at its original full-cell size.
     * The left/right chevrons are overlays, not separate
     * framed sub-controls.
     */
    const Bounds simulationSpeedGaugeBounds =
        simulationSpeedCell;

    const Bounds simulationSpeedLeft =
        simulationSpeedCell
            .horizontalSplit(
                0.0f,
                0.18f);

    const Bounds simulationSpeedRight =
        simulationSpeedCell
            .horizontalSplit(
                0.82f,
                1.0f);

    m_simSpeedGauge->m_bounds =
        simulationSpeedGaugeBounds;

    const double simulationSpeed =
        m_simulator
            ->getSimulationSpeed();

    /*
     * Restore the original Engine Simulator meaning:
     *
     *      displayed value = 1 / configured simulation speed
     *
     * It is NOT a measured realtime-performance ratio.
     */
    m_simSpeedGauge
        ->m_gauge
        ->m_value =
            simulationSpeed > 0.0
                ? 1.0f
                    / static_cast<float>(
                        simulationSpeed)
                : 0.0f;

    drawTimeChevron(
        simulationSpeedLeft,
        false);

    drawTimeChevron(
        simulationSpeedRight,
        true);

    m_audioLagGauge->m_bounds =
        grid.get(
            m_bounds,
            0,
            1);

    /*
     * Display output-buffer latency in milliseconds.
     */
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
