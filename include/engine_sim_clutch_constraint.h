#ifndef ATG_ENGINE_SIM_CLUTCH_CONSTRAINT_H
#define ATG_ENGINE_SIM_CLUTCH_CONSTRAINT_H

#include "scs.h"

/*
 * Engine Simulator-specific clutch constraint.
 *
 * The original SCS ClutchConstraint synchronizes rotational velocity
 * between two rigid bodies but deliberately uses C = 0, so there is
 * no torsional displacement state.
 *
 * CE-style clutch flex needs:
 *
 *   crank angle
 *      ↕ spring + damper
 *   driveline angle
 *
 * while retaining the original torque-limited clutch behavior.
 */
class EngineSimClutchConstraint
    : public atg_scs::Constraint
{
public:
    EngineSimClutchConstraint();
    virtual ~EngineSimClutchConstraint();

    void setBody1(
        atg_scs::RigidBody *body)
    {
        m_bodies[0] = body;
    }

    void setBody2(
        atg_scs::RigidBody *body)
    {
        m_bodies[1] = body;
    }

    /*
     * Establish the current relative angle as zero clutch twist.
     *
     * This must happen when the transmission connects or changes gear,
     * otherwise two bodies with unrelated absolute theta values would
     * instantly produce a huge spring load.
     */
    void resetFlexReference();

    void setFlexEnabled(
        bool enabled)
    {
        m_flexEnabled = enabled;
    }

    void setFlexLimitEnabled(
        bool enabled)
    {
        m_limitFlex = enabled;
    }

    void setMaxFlex(
        double maxFlex)
    {
        m_maxFlex =
            maxFlex > 0.0
                ? maxFlex
                : 0.0;
    }

    double getFlex() const;

    virtual void calculate(
        Output *output,
        atg_scs::SystemState *system)
        override;

public:
    /*
     * Kept intentionally compatible with SCS ClutchConstraint naming.
     */
    double m_maxTorque;
    double m_minTorque;

    double m_ks;
    double m_kd;

private:
    double m_referenceAngle;

    double m_maxFlex;

    bool m_referenceValid;
    bool m_flexEnabled;
    bool m_limitFlex;
};

#endif
