#ifndef ATG_ENGINE_SIM_VEHICLE_NODE_H
#define ATG_ENGINE_SIM_VEHICLE_NODE_H

#include "object_reference_node.h"
#include "engine_sim.h"

namespace es_script {

class VehicleNode
    : public ObjectReferenceNode<VehicleNode>
{
public:
    VehicleNode() {
    }

    virtual ~VehicleNode() {
    }

    void generate(
        Vehicle *vehicle) const
    {
        vehicle->initialize(
            m_parameters);
    }

protected:
    virtual void registerInputs() {
        addInput(
            "mass",
            &m_parameters.mass);

        addInput(
            "drag_coefficient",
            &m_parameters.dragCoefficient);

        addInput(
            "cross_sectional_area",
            &m_parameters.crossSectionArea);

        addInput(
            "diff_ratio",
            &m_parameters.diffRatio);

        addInput(
            "tire_radius",
            &m_parameters.tireRadius);

        addInput(
            "rolling_resistance",
            &m_parameters.rollingResistance);

        /*
         * Community Edition vehicle interface.
         */
        addInput(
            "stiffness",
            &m_parameters.stiffness);

        addInput(
            "damping",
            &m_parameters.damping);

        addInput(
            "max_flex",
            &m_parameters.maxFlex);

        addInput(
            "limit_flex",
            &m_parameters.limitFlex);

        addInput(
            "simulate_flex",
            &m_parameters.simulateFlex);

        addInput(
            "max_brake_force",
            &m_parameters.maxBrakeForce);

        ObjectReferenceNode<
            VehicleNode>
            ::registerInputs();
    }

    virtual void _evaluate() {
        setOutput(this);

        readAllInputs();
    }

    Vehicle::Parameters
        m_parameters;
};

}

#endif
