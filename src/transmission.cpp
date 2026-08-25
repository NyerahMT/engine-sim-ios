#include "../include/transmission.h"

#include "../include/units.h"

#include <cmath>
#include <cstring>

Transmission::Transmission() {
    m_gear =
        -1;

    m_newGear =
        -1;

    m_gearCount =
        0;

    m_gearRatios =
        nullptr;

    m_maxClutchTorque =
        units::torque(
            1000.0,
            units::ft_lb);

    m_rotatingMass =
        nullptr;

    m_vehicle =
        nullptr;

    m_engine =
        nullptr;

    m_clutchPressure =
        0.0;

    m_maxClutchFlex =
        0.0;

    m_limitClutchFlex =
        false;

    m_clutchStiffness =
        10.0;

    m_clutchDamping =
        1.0;

    m_clutchWasEngaged =
        false;
}

Transmission::~Transmission() {
    if (
        m_gearRatios
        != nullptr)
    {
        delete[]
            m_gearRatios;
    }

    m_gearRatios =
        nullptr;
}

void Transmission::initialize(
    const Parameters &params)
{
    if (
        m_gearRatios
        != nullptr)
    {
        delete[]
            m_gearRatios;

        m_gearRatios =
            nullptr;
    }

    m_gearCount =
        params.GearCount;

    m_maxClutchTorque =
        params.MaxClutchTorque;

    m_maxClutchFlex =
        params.MaxClutchFlex;

    m_limitClutchFlex =
        params.LimitClutchFlex;

    m_clutchStiffness =
        params.ClutchStiffness;

    m_clutchDamping =
        params.ClutchDamping;

    if (
        m_gearCount > 0 &&
        params.GearRatios
            != nullptr)
    {
        m_gearRatios =
            new double[
                m_gearCount];

        std::memcpy(
            m_gearRatios,
            params.GearRatios,
            sizeof(double)
                * m_gearCount);
    }

    /*
     * CE clutch configuration goes directly into the real
     * physics constraint.
     */
    m_clutchConstraint.m_ks =
        m_clutchStiffness;

    m_clutchConstraint.m_kd =
        m_clutchDamping;

    m_clutchConstraint
        .setMaxFlex(
            m_maxClutchFlex);

    m_clutchConstraint
        .setFlexLimitEnabled(
            m_limitClutchFlex);

    /*
     * A positive max flex is our indication that this transmission
     * uses the compliant clutch model.
     *
     * Old bundled transmissions therefore retain their old behavior.
     */
    m_clutchConstraint
        .setFlexEnabled(
            m_maxClutchFlex > 0.0);

    m_clutchWasEngaged =
        false;
}

void Transmission::update(
    double dt)
{
    (void)dt;

    const bool engaged =
        (
            m_gear != -1 &&
            m_clutchPressure > 0.0
        );

    if (!engaged) {
        m_clutchConstraint.m_minTorque =
            0.0;

        m_clutchConstraint.m_maxTorque =
            0.0;

        m_clutchWasEngaged =
            false;

        return;
    }

    /*
     * When the clutch first begins transmitting torque, establish
     * the existing crank/driveline relative position as zero flex.
     *
     * Otherwise re-engaging the clutch after free rotation would
     * inject an artificial spring impulse.
     */
    if (!m_clutchWasEngaged) {
        m_clutchConstraint
            .resetFlexReference();

        m_clutchWasEngaged =
            true;
    }

    m_clutchConstraint.m_ks =
        m_clutchStiffness;

    m_clutchConstraint.m_kd =
        m_clutchDamping;

    m_clutchConstraint.m_minTorque =
        -m_maxClutchTorque
        * m_clutchPressure;

    m_clutchConstraint.m_maxTorque =
        m_maxClutchTorque
        * m_clutchPressure;
}

void Transmission::addToSystem(
    atg_scs::RigidBodySystem *system,
    atg_scs::RigidBody *rotatingMass,
    Vehicle *vehicle,
    Engine *engine)
{
    m_rotatingMass =
        rotatingMass;

    m_vehicle =
        vehicle;

    m_engine =
        engine;

    m_clutchConstraint
        .setBody1(
            &engine
                ->getOutputCrankshaft()
                ->m_body);

    m_clutchConstraint
        .setBody2(
            m_rotatingMass);

    m_clutchConstraint
        .resetFlexReference();

    system->addConstraint(
        &m_clutchConstraint);
}

void Transmission::changeGear(
    int newGear)
{
    if (
        newGear < -1 ||
        newGear >= m_gearCount)
    {
        return;
    }

    /*
     * Neutral.
     */
    if (newGear == -1) {
        m_gear =
            -1;

        m_clutchWasEngaged =
            false;

        return;
    }

    if (
        m_vehicle == nullptr ||
        m_rotatingMass == nullptr)
    {
        m_gear =
            newGear;

        return;
    }

    const double carMass =
        m_vehicle
            ->getMass();

    const double gearRatio =
        m_gearRatios[
            newGear];

    const double diffRatio =
        m_vehicle
            ->getDiffRatio();

    const double tireRadius =
        m_vehicle
            ->getTireRadius();

    /*
     * Protect the virtual-inertia conversion from invalid script data.
     */
    if (
        carMass <= 0.0 ||
        gearRatio == 0.0 ||
        diffRatio == 0.0 ||
        tireRadius <= 0.0)
    {
        m_gear =
            newGear;

        m_clutchWasEngaged =
            false;

        return;
    }

    const double conversion =
        tireRadius
        / (
            diffRatio
            * gearRatio
        );

    const double newInertia =
        carMass
        * conversion
        * conversion;

    const double rotationalEnergy =
        0.5
        * m_rotatingMass->I
        * m_rotatingMass->v_theta
        * m_rotatingMass->v_theta;

    double newAngularVelocity =
        0.0;

    if (newInertia > 0.0) {
        newAngularVelocity =
            std::sqrt(
                rotationalEnergy
                * 2.0
                / newInertia);

        if (
            m_rotatingMass
                ->v_theta
            < 0.0)
        {
            newAngularVelocity =
                -newAngularVelocity;
        }
    }

    m_rotatingMass->I =
        newInertia;

    m_rotatingMass->p_x =
        0.0;

    m_rotatingMass->p_y =
        0.0;

    m_rotatingMass->m =
        carMass;

    m_rotatingMass->v_theta =
        newAngularVelocity;

    m_gear =
        newGear;

    /*
     * A gear change changes the effective driveline inertia and
     * angular velocity. Re-zero clutch twist when it reconnects
     * instead of carrying the old gear's torsional displacement
     * into the new ratio.
     */
    m_clutchWasEngaged =
        false;

    m_clutchConstraint
        .resetFlexReference();
}
