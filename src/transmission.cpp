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

    m_clutchPressure =
        0.0;

    /*
     * Compatibility values only.
     */
    m_maxClutchFlex =
        0.0;

    m_limitClutchFlex =
        false;

    m_clutchStiffness =
        10.0;

    m_clutchDamping =
        1.0;
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

    /*
     * Preserve CE values for compatibility, but do not feed them into
     * the physics constraint. The iOS port intentionally retains the
     * original Engine Simulator clutch behavior.
     */
    m_maxClutchFlex =
        params.MaxClutchFlex;

    m_limitClutchFlex =
        params.LimitClutchFlex;

    m_clutchStiffness =
        params.ClutchStiffness;

    m_clutchDamping =
        params.ClutchDamping;

    if (
        m_gearCount > 0
        && params.GearRatios
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
     * Begin disconnected exactly like upstream.
     */
    m_gear =
        -1;

    m_newGear =
        -1;

    m_clutchPressure =
        0.0;

    m_clutchConstraint.m_minTorque =
        0.0;

    m_clutchConstraint.m_maxTorque =
        0.0;
}

void Transmission::update(
    double dt)
{
    (void)dt;

    /*
     * Original Engine Simulator behavior:
     *
     * Neutral means zero allowed clutch torque. There is no spring,
     * torsional-flex, or damping torque connecting the crankshaft to the
     * driveline while neutral is selected.
     */
    if (m_gear == -1) {
        m_clutchConstraint.m_minTorque =
            0.0;

        m_clutchConstraint.m_maxTorque =
            0.0;
    }
    else {
        m_clutchConstraint.m_minTorque =
            -m_maxClutchTorque
            * m_clutchPressure;

        m_clutchConstraint.m_maxTorque =
            m_maxClutchTorque
            * m_clutchPressure;
    }
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

    m_clutchConstraint
        .setBody1(
            &engine
                ->getOutputCrankshaft()
                ->m_body);

    m_clutchConstraint
        .setBody2(
            m_rotatingMass);

    system
        ->addConstraint(
            &m_clutchConstraint);
}

void Transmission::changeGear(
    int newGear)
{
    if (
        newGear < -1
        || newGear >= m_gearCount)
    {
        return;
    }

    if (
        newGear != -1
        && m_vehicle != nullptr
        && m_rotatingMass != nullptr
        && m_gearRatios != nullptr)
    {
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

        if (
            carMass > 0.0
            && gearRatio != 0.0
            && diffRatio != 0.0
            && tireRadius > 0.0)
        {
            const double f =
                tireRadius
                / (
                    diffRatio
                    * gearRatio
                );

            const double newInertia =
                carMass
                * f
                * f;

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
        }
    }

    m_gear =
        newGear;
}
