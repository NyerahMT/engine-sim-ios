#ifndef ATG_ENGINE_SIM_TRANSMISSION_H
#define ATG_ENGINE_SIM_TRANSMISSION_H

#include "vehicle.h"
#include "engine.h"
#include "scs.h"

class Transmission {
public:
    struct Parameters {
        int GearCount =
            0;

        const double *GearRatios =
            nullptr;

        double MaxClutchTorque =
            0.0;

        /*
         * Kept for Community Edition script compatibility.
         *
         * The iOS port intentionally uses the original Engine Simulator
         * clutch physics. These values are accepted and stored so newer
         * scripts still load, but they do not replace the stock clutch
         * constraint.
         */
        double MaxClutchFlex =
            0.0;

        bool LimitClutchFlex =
            false;

        double ClutchStiffness =
            10.0;

        double ClutchDamping =
            1.0;
    };

public:
    Transmission();
    ~Transmission();

    void initialize(
        const Parameters &params);

    void update(
        double dt);

    void addToSystem(
        atg_scs::RigidBodySystem *system,
        atg_scs::RigidBody *rotatingMass,
        Vehicle *vehicle,
        Engine *engine);

    void changeGear(
        int newGear);

    inline int getGear() const {
        return m_gear;
    }

    inline void setClutchPressure(
        double pressure)
    {
        if (pressure < 0.0) {
            pressure = 0.0;
        }
        else if (pressure > 1.0) {
            pressure = 1.0;
        }

        m_clutchPressure =
            pressure;
    }

    inline double
    getClutchPressure() const
    {
        return m_clutchPressure;
    }

    /*
     * Compatibility accessors.
     */
    inline double
    getMaxClutchFlex() const
    {
        return m_maxClutchFlex;
    }

    inline bool
    getLimitClutchFlex() const
    {
        return m_limitClutchFlex;
    }

    inline double
    getClutchStiffness() const
    {
        return m_clutchStiffness;
    }

    inline double
    getClutchDamping() const
    {
        return m_clutchDamping;
    }

    /*
     * Stock Engine Simulator clutch has no modeled torsional flex.
     */
    inline double
    getClutchFlex() const
    {
        return 0.0;
    }

protected:
    /*
     * Original Engine Simulator clutch constraint.
     *
     * Do not replace this with the CE spring/flex clutch in the iOS port.
     * The stock constraint is velocity-coupled and torque-limited only.
     */
    atg_scs::ClutchConstraint
        m_clutchConstraint;

    atg_scs::RigidBody
        *m_rotatingMass;

    Vehicle *m_vehicle;

    int m_gear;
    int m_newGear;
    int m_gearCount;

    double *m_gearRatios;

    double m_maxClutchTorque;
    double m_clutchPressure;

    /*
     * Stored only for script/API compatibility.
     */
    double m_maxClutchFlex;
    bool m_limitClutchFlex;
    double m_clutchStiffness;
    double m_clutchDamping;
};

#endif /* ATG_ENGINE_SIM_TRANSMISSION_H */
