#include "../include/engine_sim_clutch_constraint.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

EngineSimClutchConstraint::
EngineSimClutchConstraint()
    : atg_scs::Constraint(1, 2)
{
    m_ks =
        10.0;

    m_kd =
        1.0;

    m_maxTorque =
        DBL_MAX;

    m_minTorque =
        -DBL_MAX;

    m_referenceAngle =
        0.0;

    m_maxFlex =
        0.0;

    m_referenceValid =
        false;

    m_flexEnabled =
        false;

    m_limitFlex =
        false;
}

EngineSimClutchConstraint::
~EngineSimClutchConstraint()
{
}

void
EngineSimClutchConstraint::
resetFlexReference()
{
    if (
        m_bodies[0] == nullptr ||
        m_bodies[1] == nullptr)
    {
        m_referenceValid =
            false;

        return;
    }

    m_referenceAngle =
        m_bodies[1]->theta
        - m_bodies[0]->theta;

    m_referenceValid =
        true;
}

double
EngineSimClutchConstraint::
getFlex() const
{
    if (
        !m_referenceValid ||
        m_bodies[0] == nullptr ||
        m_bodies[1] == nullptr)
    {
        return 0.0;
    }

    return
        (
            m_bodies[1]->theta
            - m_bodies[0]->theta
        )
        - m_referenceAngle;
}

void
EngineSimClutchConstraint::
calculate(
    Output *output,
    atg_scs::SystemState *system)
{
    (void)system;

    /*
     * Same rotational constraint Jacobian used by the original
     * SCS clutch:
     *
     *     -theta_1 + theta_2
     */
    output->J[0][0] =
        0.0;

    output->J[0][1] =
        0.0;

    output->J[0][2] =
        -1.0;

    output->J[0][3] =
        0.0;

    output->J[0][4] =
        0.0;

    output->J[0][5] =
        1.0;

    for (
        int i = 0;
        i < 6;
        ++i)
    {
        output->J_dot[0][i] =
            0.0;
    }

    /*
     * No reference exists until both bodies are attached.
     */
    if (!m_referenceValid) {
        resetFlexReference();
    }

    double flex =
        0.0;

    if (m_flexEnabled) {
        flex =
            getFlex();

        /*
         * max_clutch_flex represents the permitted torsional travel.
         *
         * Within the allowed range, the clutch behaves as a normal
         * spring/damper.
         *
         * Once the configured travel is exceeded, keep the constraint
         * error at the boundary instead of allowing an unbounded spring
         * displacement to build up.
         *
         * This preserves stability at the very high simulation
         * frequencies Engine Simulator uses.
         */
        if (
            m_limitFlex &&
            m_maxFlex > 0.0)
        {
            flex =
                std::clamp(
                    flex,
                    -m_maxFlex,
                    m_maxFlex);
        }
    }

    /*
     * THIS is the key difference from upstream ClutchConstraint.
     *
     * Upstream:
     *
     *     C[0] = 0
     *
     * EngineSim CE-style:
     *
     *     C[0] = actual torsional clutch displacement
     */
    output->C[0] =
        m_flexEnabled
            ? flex
            : 0.0;

    /*
     * When flex simulation is disabled, preserve classic OES behavior:
     * velocity coupling only.
     */
    output->ks[0] =
        m_flexEnabled
            ? m_ks
            : 0.0;

    output->kd[0] =
        m_kd;

    output->v_bias[0] =
        0.0;

    /*
     * Preserve original clutch torque limiting.
     */
    output->limits[0][0] =
        m_minTorque;

    output->limits[0][1] =
        m_maxTorque;
}
