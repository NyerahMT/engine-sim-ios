#include "../include/vehicle.h"

#include <algorithm>
#include <cmath>

Vehicle::Vehicle() {
    m_rotatingMass = nullptr;

    m_mass = 0.0;
    m_dragCoefficient = 0.0;
    m_crossSectionArea = 0.0;
    m_diffRatio = 0.0;
    m_tireRadius = 0.0;
    m_travelledDistance = 0.0;
    m_rollingResistance = 0.0;

    m_stiffness = 0.0;
    m_damping = 0.0;
    m_maxFlex = 0.0;

    m_limitFlex = false;
    m_simulateFlex = false;

    m_maxBrakeForce = 0.0;
    m_brake = 0.0;
}

Vehicle::~Vehicle() {
}

void Vehicle::initialize(
    const Parameters &params)
{
    m_mass =
        params.mass;

    m_dragCoefficient =
        params.dragCoefficient;

    m_crossSectionArea =
        params.crossSectionArea;

    m_diffRatio =
        params.diffRatio;

    m_tireRadius =
        params.tireRadius;

    m_rollingResistance =
        params.rollingResistance;

    m_stiffness =
        params.stiffness;

    m_damping =
        params.damping;

    m_maxFlex =
        params.maxFlex;

    m_limitFlex =
        params.limitFlex;

    m_simulateFlex =
        params.simulateFlex;

    m_maxBrakeForce =
        params.maxBrakeForce;

    m_brake =
        0.0;
}

void Vehicle::update(double dt) {
    if (m_rotatingMass == nullptr) {
        return;
    }

    m_travelledDistance +=
        getSpeed() * dt;

    /*
     * CE braking surface.
     *
     * Brake force opposes wheel rotation rather than injecting an
     * instantaneous velocity change.
     */
    if (
        m_maxBrakeForce > 0.0 &&
        m_brake > 0.0)
    {
        const double speed =
            getSpeed();

        if (
            speed > 0.00001 &&
            m_rotatingMass->I > 0.0)
        {
            const double force =
                m_maxBrakeForce
                * m_brake;

            const double torque =
                linearForceToVirtualTorque(
                    force);

            const double direction =
                m_rotatingMass->v_theta >= 0.0
                    ? -1.0
                    : 1.0;

            const double acceleration =
                direction
                * torque
                / m_rotatingMass->I;

            const double previous =
                m_rotatingMass->v_theta;

            m_rotatingMass->v_theta +=
                acceleration * dt;

            /*
             * Brakes stop the vehicle instead of accelerating through
             * zero in the opposite direction.
             */
            if (
                previous
                * m_rotatingMass->v_theta
                < 0.0)
            {
                m_rotatingMass->v_theta =
                    0.0;
            }
        }
    }
}

void Vehicle::addToSystem(
    atg_scs::RigidBodySystem *,
    atg_scs::RigidBody *rotatingMass)
{
    m_rotatingMass =
        rotatingMass;
}

double Vehicle::getSpeed() const {
    if (
        m_rotatingMass == nullptr ||
        m_mass <= 0.0)
    {
        return 0.0;
    }

    const double rotationalEnergy =
        0.5
        * m_rotatingMass->I
        * m_rotatingMass->v_theta
        * m_rotatingMass->v_theta;

    const double vehicleSpeed =
        std::sqrt(
            2.0
            * rotationalEnergy
            / m_mass);

    return vehicleSpeed;
}

double Vehicle::linearForceToVirtualTorque(
    double force) const
{
    if (
        m_rotatingMass == nullptr ||
        m_mass <= 0.0)
    {
        return 0.0;
    }

    const double rotationToKineticRatio =
        std::sqrt(
            m_rotatingMass->I
            / m_mass);

    return
        rotationToKineticRatio
        * force;
}
