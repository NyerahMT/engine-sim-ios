#include "../include/right_gauge_cluster.h"

#include "../include/units.h"
#include "../include/gauge.h"
#include "../include/constants.h"
#include "../include/engine_sim_application.h"

#include <algorithm>
#include <cmath>
#include <climits>
#include <sstream>

RightGaugeCluster::RightGaugeCluster() {
    m_engine = nullptr;
    m_simulator = nullptr;

    m_afrCluster = nullptr;
    m_tachometer = nullptr;
    m_speedometer = nullptr;
    m_manifoldVacuumGauge = nullptr;
    m_volumetricEffGauge = nullptr;
    m_intakeCfmGauge = nullptr;
    m_combusionChamberStatus = nullptr;
    m_throttleDisplay = nullptr;
    m_fuelCluster = nullptr;

    m_isAbsolute = false;
    m_checkMouse = true;
}

RightGaugeCluster::~RightGaugeCluster() {
    /* void */
}

void RightGaugeCluster::initialize(EngineSimApplication *app) {
    UiElement::initialize(app);

    m_tachometer = addElement<LabeledGauge>();
    m_speedometer = addElement<LabeledGauge>();
    m_manifoldVacuumGauge = addElement<LabeledGauge>();
    m_intakeCfmGauge = addElement<LabeledGauge>();
    m_volumetricEffGauge = addElement<LabeledGauge>();
    m_combusionChamberStatus = addElement<FiringOrderDisplay>();
    m_throttleDisplay = addElement<ThrottleDisplay>();
    m_afrCluster = addElement<AfrCluster>();
    m_fuelCluster = addElement<FuelCluster>();

    m_speedUnits =
        app->getAppSettings()->speedUnits;

    m_pressureUnits =
        app->getAppSettings()->pressureUnits;

    constexpr float shortenAngle =
        (float)units::angle(
            1.0,
            units::deg);

    /*
     * Tachometer
     */
    m_tachometer->m_title = "ENGINE SPEED";
    m_tachometer->m_unit = "rpm";
    m_tachometer->m_precision = 0;
    m_tachometer->setLocalPosition({ 0, 0 });

    m_tachometer->m_gauge->m_min = 0;
    m_tachometer->m_gauge->m_max = 7000;
    m_tachometer->m_gauge->m_minorStep = 100;
    m_tachometer->m_gauge->m_majorStep = 1000;
    m_tachometer->m_gauge->m_maxMinorTick = INT_MAX;
    m_tachometer->m_gauge->m_thetaMin =
        (float)constants::pi * 1.2f;
    m_tachometer->m_gauge->m_thetaMax =
        -(float)constants::pi * 0.2f;
    m_tachometer->m_gauge->m_needleWidth = 4.0f;
    m_tachometer->m_gauge->m_gamma = 1.0f;
    m_tachometer->m_gauge->m_needleKs = 1000.0f;
    m_tachometer->m_gauge->m_needleKd = 20.0f;

    m_tachometer->m_gauge->setBandCount(3);

    m_tachometer->m_gauge->setBand(
        {
            m_app->getForegroundColor(),
            400,
            1000,
            3.0f,
            6.0f
        },
        0);

    m_tachometer->m_gauge->setBand(
        {
            m_app->getOrange(),
            5000,
            5500,
            3.0f,
            6.0f,
            -shortenAngle,
            shortenAngle
        },
        1);

    m_tachometer->m_gauge->setBand(
        {
            m_app->getRed(),
            5500,
            7000,
            3.0f,
            6.0f,
            shortenAngle,
            -shortenAngle
        },
        2);

    /*
     * Vehicle speed
     */
    m_speedometer->m_title = "VEHICLE SPEED";
    m_speedometer->m_unit = "MPH";
    m_speedometer->m_precision = 0;
    m_speedometer->setLocalPosition({ 0, 0 });

    m_speedometer->m_gauge->m_min = 0;
    m_speedometer->m_gauge->m_max = 200;
    m_speedometer->m_gauge->m_minorStep = 5;
    m_speedometer->m_gauge->m_majorStep = 10;
    m_speedometer->m_gauge->m_maxMinorTick = 200;
    m_speedometer->m_gauge->m_thetaMin =
        (float)constants::pi * 1.2f;
    m_speedometer->m_gauge->m_thetaMax =
        -(float)constants::pi * 0.2f;
    m_speedometer->m_gauge->m_needleWidth = 4.0f;
    m_speedometer->m_gauge->m_gamma = 1.0f;
    m_speedometer->m_gauge->m_needleKs = 1000.0f;
    m_speedometer->m_gauge->m_needleKd = 20.0f;
    m_speedometer->m_gauge->setBandCount(0);

    /*
     * Manifold pressure
     */
    m_manifoldVacuumGauge->m_title =
        "MANIFOLD PRESSURE";

    m_manifoldVacuumGauge->m_unit = "inHg";
    m_manifoldVacuumGauge->m_precision = 1;
    m_manifoldVacuumGauge->setLocalPosition({ 0, 0 });

    m_manifoldVacuumGauge->m_gauge->m_min = -30;
    m_manifoldVacuumGauge->m_gauge->m_max = 5;
    m_manifoldVacuumGauge->m_gauge->m_minorStep = 1;
    m_manifoldVacuumGauge->m_gauge->m_majorStep = 5;
    m_manifoldVacuumGauge->m_gauge->m_maxMinorTick = 200;
    m_manifoldVacuumGauge->m_gauge->m_thetaMin =
        (float)constants::pi * 1.2f;
    m_manifoldVacuumGauge->m_gauge->m_thetaMax =
        -(float)constants::pi * 0.2f;
    m_manifoldVacuumGauge->m_gauge->m_needleWidth = 4.0f;
    m_manifoldVacuumGauge->m_gauge->m_gamma = 1.0f;
    m_manifoldVacuumGauge->m_gauge->m_needleKs = 1000.0f;
    m_manifoldVacuumGauge->m_gauge->m_needleKd = 50.0f;

    m_manifoldVacuumGauge->m_gauge->setBandCount(5);

    m_manifoldVacuumGauge->m_gauge->setBand(
        { m_app->getRed(), -5, -1, 3.0f, 6.0f, shortenAngle, shortenAngle },
        0);

    m_manifoldVacuumGauge->m_gauge->setBand(
        { m_app->getForegroundColor(), -1, 1, 3.0f, 6.0f, shortenAngle, shortenAngle },
        1);

    m_manifoldVacuumGauge->m_gauge->setBand(
        { m_app->getOrange(), -10, -5, 3.0f, 6.0f, shortenAngle, shortenAngle },
        2);

    m_manifoldVacuumGauge->m_gauge->setBand(
        { m_app->getBlue(), -22, -10, 3.0f, 6.0f, shortenAngle, shortenAngle },
        3);

    m_manifoldVacuumGauge->m_gauge->setBand(
        { m_app->getForegroundColor(), -30, -22, 3.0f, 6.0f, shortenAngle, shortenAngle },
        4);

    /*
     * VE
     */
    m_volumetricEffGauge->m_title =
        "VOLUMETRIC EFF.";

    m_volumetricEffGauge->m_unit = "%";
    m_volumetricEffGauge->m_spaceBeforeUnit = false;
    m_volumetricEffGauge->m_precision = 1;
    m_volumetricEffGauge->setLocalPosition({ 0, 0 });

    m_volumetricEffGauge->m_gauge->m_min = 0;
    m_volumetricEffGauge->m_gauge->m_max = 150;
    m_volumetricEffGauge->m_gauge->m_minorStep = 5;
    m_volumetricEffGauge->m_gauge->m_majorStep = 10;
    m_volumetricEffGauge->m_gauge->m_maxMinorTick = 200;
    m_volumetricEffGauge->m_gauge->m_thetaMin =
        (float)constants::pi * 1.2f;
    m_volumetricEffGauge->m_gauge->m_thetaMax =
        -(float)constants::pi * 0.2f;
    m_volumetricEffGauge->m_gauge->m_needleWidth = 4.0f;
    m_volumetricEffGauge->m_gauge->m_gamma = 1.0f;
    m_volumetricEffGauge->m_gauge->m_needleKs = 1000.0f;
    m_volumetricEffGauge->m_gauge->m_needleKd = 50.0f;

    m_volumetricEffGauge->m_gauge->setBandCount(3);

    m_volumetricEffGauge->m_gauge->setBand(
        { m_app->getBlue(), 30, 80, 3.0f, 6.0f, 0.0f, shortenAngle },
        0);

    m_volumetricEffGauge->m_gauge->setBand(
        { m_app->getGreen(), 80, 100, 3.0f, 6.0f, shortenAngle, shortenAngle },
        1);

    m_volumetricEffGauge->m_gauge->setBand(
        { m_app->getRed(), 100, 150, 3.0f, 6.0f, shortenAngle, -shortenAngle },
        2);

    /*
     * Intake airflow
     */
    m_intakeCfmGauge->m_title = "AIR SCFM";
    m_intakeCfmGauge->m_unit = "";
    m_intakeCfmGauge->m_precision = 1;
    m_intakeCfmGauge->setLocalPosition({ 0, 0 });

    m_intakeCfmGauge->m_gauge->m_min = 0;
    m_intakeCfmGauge->m_gauge->m_max = 2000;
    m_intakeCfmGauge->m_gauge->m_minorStep = 50;
    m_intakeCfmGauge->m_gauge->m_majorStep = 250;
    m_intakeCfmGauge->m_gauge->m_maxMinorTick = 2000;
    m_intakeCfmGauge->m_gauge->m_thetaMin =
        (float)constants::pi * 1.2f;
    m_intakeCfmGauge->m_gauge->m_thetaMax =
        -(float)constants::pi * 0.2f;
    m_intakeCfmGauge->m_gauge->m_needleWidth = 4.0f;
    m_intakeCfmGauge->m_gauge->m_gamma = 1.0f;
    m_intakeCfmGauge->m_gauge->m_needleKs = 1000.0f;
    m_intakeCfmGauge->m_gauge->m_needleKd = 50.0f;
    m_intakeCfmGauge->m_gauge->setBandCount(0);

    setUnits();
}

void RightGaugeCluster::destroy() {
    m_engine = nullptr;
    m_simulator = nullptr;

    UiElement::destroy();
}

void RightGaugeCluster::update(float dt) {
    m_mouseBounds = m_bounds;

    if (m_combusionChamberStatus != nullptr) {
        m_combusionChamberStatus->m_engine =
            m_engine;
    }

    if (m_throttleDisplay != nullptr) {
        m_throttleDisplay->m_engine =
            m_engine;
    }

    if (m_afrCluster != nullptr) {
        m_afrCluster->m_engine =
            m_engine;
    }

    if (m_fuelCluster != nullptr) {
        m_fuelCluster->m_engine =
            m_engine;

        m_fuelCluster->m_simulator =
            m_simulator;
    }

    UiElement::update(dt);
}

void RightGaugeCluster::onMouseDown(
    const Point &mouseLocal)
{
    UiElement::onMouseDown(
        mouseLocal);

    if (
        throttleControlBounds()
            .overlaps(
                mouseLocal))
    {
        m_throttleHeld = true;

        setTouchThrottleFromPoint(
            mouseLocal);
    }
}

void RightGaugeCluster::onMouseUp(
    const Point &mouseLocal)
{
    UiElement::onMouseUp(
        mouseLocal);

    if (m_throttleHeld) {
        m_throttleHeld = false;

        m_app->setTouchThrottle(
            0.0,
            false);
    }
}

void RightGaugeCluster::onDrag(
    const Point &,
    const Point &,
    const Point &mouse)
{
    if (m_throttleHeld) {
        setTouchThrottleFromPoint(
            mouse);
    }
}

void RightGaugeCluster::render() {
    drawFrame(
        m_bounds,
        1.0,
        m_app->getForegroundColor(),
        m_app->getBackgroundColor());

    const Bounds tachSpeedCluster =
        m_bounds.verticalSplit(
            0.5f,
            1.0f);

    renderTachSpeedCluster(
        tachSpeedCluster);

    const Bounds fuelAirCluster =
        m_bounds.verticalSplit(
            0.0f,
            0.5f);

    renderFuelAirCluster(
        fuelAirCluster);

    UiElement::render();

    renderTouchThrottleControl();
}

void RightGaugeCluster::setEngine(
    Engine *engine)
{
    m_engine = engine;
}

void RightGaugeCluster::renderTachSpeedCluster(
    const Bounds &bounds)
{
    const Bounds left =
        bounds.horizontalSplit(
            0.0f,
            0.5f);

    const Bounds right =
        bounds.horizontalSplit(
            0.5f,
            1.0f);

    /*
     * Tach
     */
    const Bounds tach =
        left.verticalSplit(
            0.5f,
            1.0f);

    m_tachometer->m_bounds =
        tach;

    m_tachometer->m_gauge->m_value =
        static_cast<float>(
            std::abs(
                getRpm()));

    constexpr float shortenAngle =
        (float)units::angle(
            1.0,
            units::deg);

    const double engineRedline =
        getRedline();

    const float maxRpm =
        engineRedline > 0.0
            ? static_cast<float>(
                std::ceil(
                    units::toRpm(
                        engineRedline
                        * 1.25)
                    / 1000.0)
                * 1000.0)
            : 7000.0f;

    const float redline =
        engineRedline > 0.0
            ? static_cast<float>(
                std::ceil(
                    units::toRpm(
                        engineRedline)
                    / 500.0)
                * 500.0)
            : 5500.0f;

    const float redlineWarning =
        engineRedline > 0.0
            ? static_cast<float>(
                std::floor(
                    units::toRpm(
                        engineRedline
                        * 0.9)
                    / 500.0)
                * 500.0)
            : 5000.0f;

    m_tachometer->m_gauge->m_max =
        std::max(
            1000.0f,
            maxRpm);

    m_tachometer->m_gauge->setBandCount(
        3);

    m_tachometer->m_gauge->setBand(
        {
            m_app->getForegroundColor(),
            400,
            1000,
            3.0f,
            6.0f
        },
        0);

    m_tachometer->m_gauge->setBand(
        {
            m_app->getOrange(),
            redlineWarning,
            redline,
            3.0f,
            6.0f,
            -shortenAngle,
            shortenAngle
        },
        1);

    m_tachometer->m_gauge->setBand(
        {
            m_app->getRed(),
            redline,
            maxRpm,
            3.0f,
            6.0f,
            shortenAngle,
            -shortenAngle
        },
        2);

    /*
     * Vehicle speed
     */
    const Bounds speed =
        left.verticalSplit(
            0.0f,
            0.5f);

    m_speedometer->m_bounds =
        speed;

    const double vehicleSpeed =
        std::abs(
            getSpeed());

    m_speedometer->m_gauge->m_value =
        m_speedUnits == "mph"
            ? static_cast<float>(
                units::convert(
                    vehicleSpeed,
                    units::mile
                    / units::hour))
            : static_cast<float>(
                units::convert(
                    vehicleSpeed,
                    units::km
                    / units::hour));

    m_combusionChamberStatus->m_bounds =
        right;
}

void RightGaugeCluster::renderFuelAirCluster(
    const Bounds &bounds)
{
    const Bounds left =
        bounds.horizontalSplit(
            0.0f,
            0.5f);

    const Bounds right =
        bounds.horizontalSplit(
            0.5f,
            1.0f);

    const Bounds throttle =
        left.verticalSplit(
            0.5f,
            1.0f);

    m_throttleDisplay->m_bounds =
        throttle;

    const Bounds fuelSection =
        left.verticalSplit(
            0.0f,
            0.5f);

    const Bounds afr =
        fuelSection.horizontalSplit(
            0.0f,
            0.5f);

    m_afrCluster->m_bounds =
        afr;

    const Bounds fuelConsumption =
        fuelSection.horizontalSplit(
            0.5f,
            1.0f);

    m_fuelCluster->m_bounds =
        fuelConsumption;

    constexpr double ambientPressure =
        units::pressure(
            1.0,
            units::atm);

    constexpr double ambientTemperature =
        units::celcius(
            25.0);

    Grid grid =
        { 1, 3 };

    /*
     * Manifold pressure.
     *
     * Do not snap readings close to atmospheric pressure to zero.
     * That was making a legitimately moving gauge appear dead.
     */
    const Bounds manifoldPressure =
        grid.get(
            right,
            0,
            0,
            1,
            1);

    m_manifoldVacuumGauge->m_bounds =
        manifoldPressure;

    m_manifoldVacuumGauge->m_gauge->m_value =
        static_cast<float>(
            getManifoldPressureWithUnits(
                ambientPressure));

    /*
     * Airflow + VE.
     */
    const double rpm =
        std::max(
            std::abs(
                getRpm()),
            0.0);

    const double displacement =
        m_engine != nullptr
            ? std::max(
                0.0,
                m_engine->getDisplacement())
            : 0.0;

    const double theoreticalAirPerRevolution =
        0.5
        * (
            ambientPressure
            * displacement)
        / (
            constants::R
            * ambientTemperature);

    const double theoreticalAirPerSecond =
        theoreticalAirPerRevolution
        * rpm
        / 60.0;

    /*
     * Intake flow can briefly cross zero due to pressure pulsation.
     * A dashboard airflow gauge should display forward net flow,
     * not fling its needle below zero.
     */
    const double actualAirPerSecond =
        m_engine != nullptr
            ? std::max(
                0.0,
                m_engine->getIntakeFlowRate())
            : 0.0;

    double volumetricEfficiency =
        0.0;

    if (
        theoreticalAirPerSecond
        > 1.0e-6)
    {
        volumetricEfficiency =
            actualAirPerSecond
            / theoreticalAirPerSecond;
    }

    if (
        !std::isfinite(
            volumetricEfficiency)
        || volumetricEfficiency < 0.0)
    {
        volumetricEfficiency =
            0.0;
    }

    const Bounds cfmBounds =
        grid.get(
            right,
            0,
            1,
            1,
            1);

    m_intakeCfmGauge->m_bounds =
        cfmBounds;

    const double scfm =
        units::convert(
            actualAirPerSecond,
            units::scfm);

    m_intakeCfmGauge->m_gauge->m_value =
        static_cast<float>(
            std::max(
                0.0,
                scfm));

    const Bounds volumetricEfficiencyBounds =
        grid.get(
            right,
            0,
            2,
            1,
            1);

    m_volumetricEffGauge->m_bounds =
        volumetricEfficiencyBounds;

    m_volumetricEffGauge->m_gauge->m_value =
        static_cast<float>(
            100.0
            * volumetricEfficiency);
}

Bounds RightGaugeCluster::throttleControlBounds() const {
    const Bounds fuelAir =
        m_bounds.verticalSplit(
            0.0f,
            0.5f);

    const Bounds left =
        fuelAir.horizontalSplit(
            0.0f,
            0.5f);

    const Bounds throttle =
        left.verticalSplit(
            0.5f,
            1.0f);

    return
        throttle
            .horizontalSplit(
                0.84f,
                0.96f)
            .verticalSplit(
                0.12f,
                0.82f);
}

void RightGaugeCluster::setTouchThrottleFromPoint(
    const Point &mouseLocal)
{
    const Bounds control =
        throttleControlBounds();

    const float value =
        clamp(
            (mouseLocal.y
                - control.bottom())
            / control.height());

    m_app->setTouchThrottle(
        value,
        true);
}

void RightGaugeCluster::renderTouchThrottleControl() {
    const Bounds control =
        throttleControlBounds();

    Engine *engine =
        m_engine;

    const float value =
        engine == nullptr
            ? 0.0f
            : static_cast<float>(
                engine->getSpeedControl());

    const Bounds fill(
        control.width() - 4.0f,
        (control.height() - 4.0f)
            * value,
        {
            control.center_h(),
            control.bottom() + 2.0f
        },
        Bounds::bm);

    drawFrame(
        control,
        1.0f,
        m_app->getForegroundColor(),
        m_app->getBackgroundColor());

    if (value > 0.0f) {
        drawBox(
            fill,
            m_app->getRed());
    }
}

double RightGaugeCluster::getManifoldPressureWithUnits(
    double ambientPressure)
{
    const double manifoldPressure =
        getManifoldPressure();

    const double gaugePressure =
        manifoldPressure
        - ambientPressure;

    if (m_pressureUnits == "inHg") {
        return units::convert(
            gaugePressure,
            units::inHg);
    }
    else if (m_pressureUnits == "kPa") {
        return units::convert(
            manifoldPressure,
            units::kPa);
    }
    else if (m_pressureUnits == "mbar") {
        return units::convert(
            manifoldPressure,
            units::mbar);
    }
    else if (m_pressureUnits == "bar") {
        return units::convert(
            manifoldPressure,
            units::bar);
    }
    else if (m_pressureUnits == "psi") {
        return units::convert(
            gaugePressure,
            units::psi);
    }

    return units::convert(
        gaugePressure,
        units::inHg);
}

double RightGaugeCluster::getRpm() const {
    return m_engine != nullptr
        ? m_engine->getRpm()
        : 0.0;
}

double RightGaugeCluster::getRedline() const {
    return m_engine != nullptr
        ? m_engine->getRedline()
        : 0.0;
}

double RightGaugeCluster::getSpeed() const {
    if (m_simulator == nullptr) {
        return 0.0;
    }

    Vehicle *vehicle =
        m_simulator->getVehicle();

    return vehicle != nullptr
        ? vehicle->getSpeed()
        : 0.0;
}

double RightGaugeCluster::getManifoldPressure() const {
    return m_engine != nullptr
        ? m_engine->getManifoldPressure()
        : units::pressure(
            1.0,
            units::atm);
}

void RightGaugeCluster::setUnits() {
    constexpr float shortenAngle =
        (float)units::angle(
            1.0,
            units::deg);

    m_speedometer->m_unit =
        m_speedUnits == "mph"
            ? "mph"
            : "kph";

    if (m_pressureUnits == "kPa") {
        m_isAbsolute = true;

        m_manifoldVacuumGauge->m_unit = "kPa";
        m_manifoldVacuumGauge->m_precision = 1;
        m_manifoldVacuumGauge->m_gauge->m_min = 0;
        m_manifoldVacuumGauge->m_gauge->m_max = 110;
        m_manifoldVacuumGauge->m_gauge->m_minorStep = 5;
        m_manifoldVacuumGauge->m_gauge->m_majorStep = 10;

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getRed(), 90, 110, 3.0f, 6.0f, shortenAngle, shortenAngle },
            0);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getForegroundColor(), 90, 105, 3.0f, 6.0f, shortenAngle, shortenAngle },
            1);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getOrange(), 30, 40, 3.0f, 6.0f, shortenAngle, shortenAngle },
            2);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getBlue(), 15, 29, 3.0f, 6.0f, shortenAngle, shortenAngle },
            3);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getForegroundColor(), 0, 14, 3.0f, 6.0f, shortenAngle, shortenAngle },
            4);
    }
    else if (m_pressureUnits == "mbar") {
        m_isAbsolute = true;

        m_manifoldVacuumGauge->m_unit = "mbar";
        m_manifoldVacuumGauge->m_precision = 0;
        m_manifoldVacuumGauge->m_gauge->m_min = 0;
        m_manifoldVacuumGauge->m_gauge->m_max = 1100;
        m_manifoldVacuumGauge->m_gauge->m_minorStep = 50;
        m_manifoldVacuumGauge->m_gauge->m_majorStep = 100;

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getRed(), 900, 1100, 3.0f, 6.0f, shortenAngle, shortenAngle },
            0);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getForegroundColor(), 900, 1050, 3.0f, 6.0f, shortenAngle, shortenAngle },
            1);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getOrange(), 300, 400, 3.0f, 6.0f, shortenAngle, shortenAngle },
            2);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getBlue(), 150, 290, 3.0f, 6.0f, shortenAngle, shortenAngle },
            3);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getForegroundColor(), 0, 140, 3.0f, 6.0f, shortenAngle, shortenAngle },
            4);
    }
    else if (m_pressureUnits == "bar") {
        m_isAbsolute = true;

        m_manifoldVacuumGauge->m_unit = "bar";
        m_manifoldVacuumGauge->m_precision = 2;
        m_manifoldVacuumGauge->m_gauge->m_min = 0;
        m_manifoldVacuumGauge->m_gauge->m_max = 1.1f;

        /*
         * Gauge tick steps are integers in upstream EngineSim, so
         * decimal-bar displays rely primarily on the numeric readout.
         */
        m_manifoldVacuumGauge->m_gauge->m_minorStep = 1;
        m_manifoldVacuumGauge->m_gauge->m_majorStep = 1;

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getRed(), 0.9f, 1.1f, 3.0f, 6.0f, shortenAngle, shortenAngle },
            0);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getForegroundColor(), 0.9f, 1.05f, 3.0f, 6.0f, shortenAngle, shortenAngle },
            1);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getOrange(), 0.3f, 0.5f, 3.0f, 6.0f, shortenAngle, shortenAngle },
            2);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getBlue(), 0.15f, 0.29f, 3.0f, 6.0f, shortenAngle, shortenAngle },
            3);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getForegroundColor(), 0, 0.14f, 3.0f, 6.0f, shortenAngle, shortenAngle },
            4);
    }
    else if (m_pressureUnits == "psi") {
        m_isAbsolute = false;

        m_manifoldVacuumGauge->m_unit = "psi";
        m_manifoldVacuumGauge->m_precision = 1;
        m_manifoldVacuumGauge->m_gauge->m_min = -15;
        m_manifoldVacuumGauge->m_gauge->m_max = 5;
        m_manifoldVacuumGauge->m_gauge->m_minorStep = 1;
        m_manifoldVacuumGauge->m_gauge->m_majorStep = 5;

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getRed(), -4, 1, 3.0f, 6.0f, shortenAngle, shortenAngle },
            0);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getForegroundColor(), -1, 1, 3.0f, 6.0f, shortenAngle, shortenAngle },
            1);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getOrange(), -7, -4, 3.0f, 6.0f, shortenAngle, shortenAngle },
            2);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getBlue(), -12, -7, 3.0f, 6.0f, shortenAngle, shortenAngle },
            3);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getForegroundColor(), -15, -12, 3.0f, 6.0f, shortenAngle, shortenAngle },
            4);
    }
    else {
        m_isAbsolute = false;

        m_manifoldVacuumGauge->m_unit = "inHg";
        m_manifoldVacuumGauge->m_precision = 1;
        m_manifoldVacuumGauge->m_gauge->m_min = -30;
        m_manifoldVacuumGauge->m_gauge->m_max = 10;
        m_manifoldVacuumGauge->m_gauge->m_minorStep = 1;
        m_manifoldVacuumGauge->m_gauge->m_majorStep = 5;

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getRed(), -5, -1, 3.0f, 6.0f, shortenAngle, shortenAngle },
            0);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getForegroundColor(), -1, 1, 3.0f, 6.0f, shortenAngle, shortenAngle },
            1);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getOrange(), -10, -5, 3.0f, 6.0f, shortenAngle, shortenAngle },
            2);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getBlue(), -22, -10, 3.0f, 6.0f, shortenAngle, shortenAngle },
            3);

        m_manifoldVacuumGauge->m_gauge->setBand(
            { m_app->getForegroundColor(), -30, -22, 3.0f, 6.0f, shortenAngle, shortenAngle },
            4);
    }
}
