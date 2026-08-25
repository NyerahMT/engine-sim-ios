#ifndef ATG_ENGINE_SIM_ENGINE_NODE_H
#define ATG_ENGINE_SIM_ENGINE_NODE_H

#include "object_reference_node.h"

#include "crankshaft_node.h"
#include "cylinder_bank_node.h"
#include "ignition_module_node.h"
#include "engine_context.h"
#include "fuel_node.h"
#include "throttle_nodes.h"

#include "engine_sim.h"

#include <map>
#include <set>
#include <vector>

namespace es_script {

class EngineNode
    : public ObjectReferenceNode<EngineNode>
{
public:
    EngineNode() {
    }

    virtual ~EngineNode() {
    }

    void buildEngine(
        Engine *engine)
    {
        int cylinderCount = 0;

        for (
            const CylinderBankNode *bank
                : m_cylinderBanks)
        {
            cylinderCount +=
                bank
                    ->getCylinderCount();
        }

        std::set<ExhaustSystemNode *>
            exhaustSystems;

        std::set<IntakeNode *>
            intakes;

        for (
            const CylinderBankNode *bank
                : m_cylinderBanks)
        {
            const int n =
                bank
                    ->getCylinderCount();

            for (
                int i = 0;
                i < n;
                ++i)
            {
                exhaustSystems.insert(
                    bank
                        ->getCylinder(i)
                        .exhaust);

                intakes.insert(
                    bank
                        ->getCylinder(i)
                        .intake);
            }
        }

        EngineContext context;

        context.setEngine(
            engine);

        Engine::Parameters parameters =
            m_parameters;

        parameters.crankshaftCount =
            static_cast<int>(
                m_crankshafts.size());

        parameters.cylinderBanks =
            static_cast<int>(
                m_cylinderBanks.size());

        parameters.cylinderCount =
            cylinderCount;

        parameters.exhaustSystemCount =
            static_cast<int>(
                exhaustSystems.size());

        parameters.intakeCount =
            static_cast<int>(
                intakes.size());

        parameters.throttle =
            m_throttle
                ->generate();

        engine->initialize(
            parameters);

        {
            int i = 0;

            for (
                ExhaustSystemNode *exhaust
                    : exhaustSystems)
            {
                context.addExhaust(
                    exhaust,
                    engine
                        ->getExhaustSystem(
                            i++));
            }
        }

        {
            int i = 0;

            for (
                IntakeNode *intake
                    : intakes)
            {
                context.addIntake(
                    intake,
                    engine
                        ->getIntake(
                            i++));
            }
        }

        {
            int i = 0;

            for (
                const CylinderBankNode *bank
                    : m_cylinderBanks)
            {
                context.addHead(
                    bank
                        ->getCylinderHead(),
                    engine
                        ->getHead(
                            i++));
            }
        }

        for (
            int i = 0;
            i < parameters.crankshaftCount;
            ++i)
        {
            m_crankshafts[i]
                ->generate(
                    engine
                        ->getCrankshaft(i),
                    &context);
        }

        for (
            int i = 0;
            i < parameters.cylinderBanks;
            ++i)
        {
            m_cylinderBanks[i]
                ->indexSlaveJournals(
                    &context);
        }

        int cylinderIndex =
            0;

        for (
            int i = 0;
            i < parameters.cylinderBanks;
            ++i)
        {
            m_cylinderBanks[i]
                ->generate(
                    i,
                    cylinderIndex,
                    engine
                        ->getCylinderBank(i),
                    engine
                        ->getCrankshaft(0),
                    engine,
                    &context);

            cylinderIndex +=
                m_cylinderBanks[i]
                    ->getCylinderCount();
        }

        for (
            int i = 0;
            i < parameters.cylinderBanks;
            ++i)
        {
            m_cylinderBanks[i]
                ->connectRodAssemblies(
                    &context);
        }

        m_ignitionModule
            ->generate(
                engine,
                &context);

        Function *
        meanPistonSpeedToTurbulence =
            new Function;

        meanPistonSpeedToTurbulence
            ->initialize(
                30,
                1);

        for (
            int i = 0;
            i < 30;
            ++i)
        {
            const double s =
                static_cast<double>(i);

            meanPistonSpeedToTurbulence
                ->addSample(
                    s,
                    s * 0.5);
        }

        Fuel *fuel =
            engine
                ->getFuel();

        m_fuel
            ->generate(
                fuel,
                &context);

        CombustionChamber::Parameters
            ccParams;

        ccParams.CrankcasePressure =
            units::pressure(
                1.0,
                units::atm);

        ccParams.Fuel =
            fuel;

        ccParams.StartingPressure =
            units::pressure(
                1.0,
                units::atm);

        ccParams.StartingTemperature =
            units::celcius(
                25.0);

        ccParams.MeanPistonSpeedToTurbulence =
            meanPistonSpeedToTurbulence;

        ccParams.BlockTemperature =
            parameters
                .blockTemperature;

        ccParams.HeatTransferCoefficient =
            parameters
                .heatTransferCoefficient;

        for (
            int i = 0;
            i < engine
                ->getCylinderCount();
            ++i)
        {
            ccParams.Piston =
                engine
                    ->getPiston(i);

            ccParams.Head =
                engine
                    ->getHead(
                        ccParams
                            .Piston
                            ->getCylinderBank()
                            ->getIndex());

            engine
                ->getChamber(i)
                ->initialize(
                    ccParams);
        }
    }

    void addCrankshaft(
        CrankshaftNode *crankshaft)
    {
        m_crankshafts
            .push_back(
                crankshaft);
    }

    void addCylinderBank(
        CylinderBankNode *bank)
    {
        m_cylinderBanks
            .push_back(
                bank);
    }

    int
    getIgnitionModuleCount() const
    {
        return
            m_ignitionModule
                == nullptr
            ? 0
            : 1;
    }

    void addIgnitionModule(
        IgnitionModuleNode
            *ignitionModule)
    {
        m_ignitionModule =
            ignitionModule;
    }

protected:
    virtual void registerInputs() {
        addInput(
            "name",
            &m_parameters.name);

        addInput(
            "starter_torque",
            &m_parameters
                .starterTorque);

        addInput(
            "starter_speed",
            &m_parameters
                .starterSpeed);

        addInput(
            "dyno_min_speed",
            &m_parameters
                .dynoMinSpeed);

        addInput(
            "dyno_max_speed",
            &m_parameters
                .dynoMaxSpeed);

        addInput(
            "dyno_hold_step",
            &m_parameters
                .dynoHoldStep);

        addInput(
            "redline",
            &m_parameters
                .redline);

        addInput(
            "fuel",
            &m_fuel,
            InputTarget::Type::Object);

        addInput(
            "throttle",
            &m_throttle,
            InputTarget::Type::Object);

        addInput(
            "simulation_frequency",
            &m_parameters
                .initialSimulationFrequency);

        addInput(
            "hf_gain",
            &m_parameters
                .initialHighFrequencyGain);

        addInput(
            "jitter",
            &m_parameters
                .initialJitter);

        addInput(
            "noise",
            &m_parameters
                .initialNoise);

        addInput(
            "fluid_simulation_steps",
            &m_parameters
                .fluidSimulationSteps);

        addInput(
            "block_temperature",
            &m_parameters
                .blockTemperature);

        addInput(
            "heat_transfer_coefficient",
            &m_parameters
                .heatTransferCoefficient);

        ObjectReferenceNode<
            EngineNode>
            ::registerInputs();
    }

    virtual void _evaluate() {
        setOutput(
            this);

        readAllInputs();

        if (
            m_parameters
                .fluidSimulationSteps
            < 1)
        {
            m_parameters
                .fluidSimulationSteps =
                    1;
        }

        if (
            m_parameters
                .heatTransferCoefficient
            < 0.0)
        {
            m_parameters
                .heatTransferCoefficient =
                    0.0;
        }
    }

    ThrottleNode *m_throttle =
        nullptr;

    IgnitionModuleNode
        *m_ignitionModule =
            nullptr;

    FuelNode *m_fuel =
        nullptr;

    Engine::Parameters
        m_parameters;

    std::vector<CrankshaftNode *>
        m_crankshafts;

    std::vector<CylinderBankNode *>
        m_cylinderBanks;
};

}

#endif
