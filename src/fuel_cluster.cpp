#include "../include/fuel_cluster.h"

#include "../include/engine_sim_application.h"

#include <cmath>
#include <iomanip>
#include <sstream>

FuelCluster::FuelCluster() {
    m_engine = nullptr;
    m_simulator = nullptr;
}

FuelCluster::~FuelCluster() {
    /* void */
}

void FuelCluster::initialize(EngineSimApplication *app) {
    UiElement::initialize(app);
}

void FuelCluster::destroy() {
    /*
     * Engine switching rebuilds the UI and simulator.
     *
     * Explicitly forget both borrowed pointers so a partially
     * rebuilt UI can never retain references to the previous engine.
     */
    m_engine = nullptr;
    m_simulator = nullptr;

    UiElement::destroy();
}

void FuelCluster::update(float dt) {
    UiElement::update(dt);
}

void FuelCluster::render() {
    const Bounds bounds = m_bounds.inset(10.0f);
    const Bounds title = bounds.verticalSplit(1.0f, 0.9f);
    const Bounds bodyBounds = bounds.verticalSplit(0.0f, 0.9f);

    drawCenteredText(
        "FUEL",
        title.inset(10.0f),
        24.0f);

    Grid grid;
    grid.h_cells = 1;
    grid.v_cells = 10;

    /*
     * Fuel data belongs to the engine.
     *
     * During an engine hot-swap the new UI can receive its first
     * render before RightGaugeCluster::update() propagates the new
     * simulator pointer to this child.
     *
     * Never assume either borrowed pointer is ready on that frame.
     */

    const double fuelConsumed =
        (m_engine != nullptr)
            ? m_engine->getTotalVolumeFuelConsumed()
            : 0.0;

    const double fuelConsumedLiters =
        units::convert(
            fuelConsumed,
            units::L);

    const double fuelConsumedGallons =
        units::convert(
            fuelConsumed,
            units::gal);

    /*
     * Total fuel - liters.
     */

    std::stringstream ss;

    ss
        << std::setprecision(3)
        << std::fixed
        << fuelConsumedLiters
        << " L";

    const Bounds totalFuelLiters =
        grid.get(
            bodyBounds,
            0,
            1,
            1,
            2);

    drawText(
        ss.str(),
        totalFuelLiters,
        32.0f,
        Bounds::lm);

    /*
     * Total fuel - gallons.
     */

    ss.str("");
    ss.clear();

    ss
        << std::setprecision(3)
        << std::fixed
        << fuelConsumedGallons
        << " gal";

    const Bounds totalFuelGallons =
        grid.get(
            bodyBounds,
            0,
            3,
            1,
            1);

    drawText(
        ss.str(),
        totalFuelGallons,
        16.0f,
        Bounds::lm);

    /*
     * Fuel cost.
     */

    ss.str("");
    ss.clear();

    ss
        << std::setprecision(2)
        << std::fixed
        << "$"
        << 4.761 * fuelConsumedGallons
        << " USD";

    const Bounds costUSD =
        grid.get(
            bodyBounds,
            0,
            4);

    drawText(
        ss.str(),
        costUSD,
        16.0f,
        Bounds::lm);

    /*
     * Distance is simulator-owned.
     *
     * This null check is the important engine-switch crash fix.
     */

    double travelledDistance =
        0.0;

    if (m_simulator != nullptr) {
        Vehicle *vehicle =
            m_simulator->getVehicle();

        if (vehicle != nullptr) {
            travelledDistance =
                vehicle->getTravelledDistance();
        }
    }

    /*
     * MPG.
     *
     * Avoid NaN / infinity when no fuel has been consumed yet.
     */

    const double distanceMiles =
        units::convert(
            travelledDistance,
            units::mile);

    double mpg =
        0.0;

    if (fuelConsumedGallons > 1.0e-9) {
        mpg =
            distanceMiles
            / fuelConsumedGallons;

        if (!std::isfinite(mpg)) {
            mpg = 0.0;
        }
    }

    ss.str("");
    ss.clear();

    ss
        << std::setprecision(2)
        << std::fixed
        << mpg
        << " MPG";

    const Bounds mpgBounds =
        grid.get(
            bodyBounds,
            0,
            6);

    drawText(
        ss.str(),
        mpgBounds,
        16.0f,
        Bounds::lm);

    /*
     * L / 100 km.
     */

    const double distanceKm =
        units::convert(
            travelledDistance,
            units::km);

    double lp100km =
        0.0;

    if (distanceKm > 1.0e-9) {
        lp100km =
            fuelConsumedLiters
            / (distanceKm / 100.0);

        if (!std::isfinite(lp100km)) {
            lp100km = 0.0;
        }
    }

    lp100km =
        std::min(
            lp100km,
            100.0);

    ss.str("");
    ss.clear();

    ss
        << std::setprecision(2)
        << std::fixed
        << lp100km
        << " L/100 KM";

    const Bounds lp100kmBounds =
        grid.get(
            bodyBounds,
            0,
            7);

    drawText(
        ss.str(),
        lp100kmBounds,
        12.0f,
        Bounds::lm);

    UiElement::render();
}

double FuelCluster::getTotalVolumeFuelConsumed() const {
    return (m_engine != nullptr)
        ? m_engine->getTotalVolumeFuelConsumed()
        : 0.0;
}
