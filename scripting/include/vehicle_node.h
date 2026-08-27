#ifndef ATG_ENGINE_SIM_VEHICLE_NODE_H
#define ATG_ENGINE_SIM_VEHICLE_NODE_H

#include "object_reference_node.h"
#include "engine_sim.h"

namespace es_script {

    class VehicleNode : public ObjectReferenceNode<VehicleNode> {
    public:
        VehicleNode() { }
        virtual ~VehicleNode() { }

        void generate(Vehicle *vehicle) const {
            vehicle->initialize(m_parameters);
        }

    protected:
        virtual void registerInputs() {
            addInput("mass", &m_parameters.mass);
            addInput("drag_coefficient", &m_parameters.dragCoefficient);
            addInput("cross_sectional_area", &m_parameters.crossSectionArea);
            addInput("diff_ratio", &m_parameters.diffRatio);
            addInput("tire_radius", &m_parameters.tireRadius);
            addInput("rolling_resistance", &m_parameters.rollingResistance);

            addInput("max_brake_force", &m_compatMaxBrakeForce);
            addInput("stiffness", &m_compatStiffness);
            addInput("damping", &m_compatDamping);
            addInput("max_flex", &m_compatMaxFlex);
            addInput("limit_flex", &m_compatLimitFlex);
            addInput("simulate_flex", &m_compatSimulateFlex);

            ObjectReferenceNode<VehicleNode>::registerInputs();
        }

        virtual void _evaluate() {
            setOutput(this);
            readAllInputs();
        }

        Vehicle::Parameters m_parameters;

        double m_compatMaxBrakeForce = 0.0;
        double m_compatStiffness = 0.0;
        double m_compatDamping = 0.0;
        double m_compatMaxFlex = 0.0;
        bool m_compatLimitFlex = false;
        bool m_compatSimulateFlex = false;
    };

}

#endif
