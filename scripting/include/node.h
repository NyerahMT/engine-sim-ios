#ifndef ATG_ENGINE_SIM_NODE_H
#define ATG_ENGINE_SIM_NODE_H

#include "piranha.h"

#include <map>
#include <string>

namespace es_script {

class Node : public piranha::Node {
protected:
    struct InputTarget {
        enum class Type {
            Object,
            Atomic
        };

        piranha::pNodeInput *input =
            nullptr;

        void *memoryTarget =
            nullptr;

        Type type =
            Type::Atomic;
    };

public:
    Node() {
        /* void */
    }

    virtual ~Node() {
        for (
            auto &entry
                : m_inputMap)
        {
            delete
                entry.second.input;

            entry.second.input =
                nullptr;
        }
    }

    template <typename T_Out>
    T_Out readAtomicInput(
        const std::string &name)
    {
        T_Out out{};

        auto it =
            m_inputMap.find(
                name);

        if (
            it
            == m_inputMap.end())
        {
            return out;
        }

        InputTarget &target =
            it->second;

        if (
            target.input
                == nullptr
            || *target.input
                == nullptr)
        {
            return out;
        }

        (*target.input)
            ->fullCompute(
                &out);

        return out;
    }

    void readAllInputs() {
        for (
            auto &entry
                : m_inputMap)
        {
            InputTarget &target =
                entry.second;

            /*
             * An EngineNode may expose a newer native input while an
             * older .mr wrapper does not connect that input yet.
             *
             * Previously this became:
             *
             *     (*nullptr)->fullCompute(...)
             *
             * and caused an immediate EXC_BAD_ACCESS on launch.
             *
             * Leave the native parameter's initialized/default value
             * untouched when there is no script-side connection.
             */
            if (
                target.input
                    == nullptr
                || *target.input
                    == nullptr)
            {
                continue;
            }

            if (
                target.memoryTarget
                == nullptr)
            {
                continue;
            }

            switch (
                target.type)
            {
            case InputTarget::Type::Atomic:
            case InputTarget::Type::Object:
                (*target.input)
                    ->fullCompute(
                        target.memoryTarget);

                break;
            }
        }
    }

    void addInput(
        const std::string &name,
        void *target,
        InputTarget::Type type =
            InputTarget::Type::Atomic)
    {
        /*
         * If a derived node accidentally registers the same input twice,
         * clean up the old pNodeInput instead of leaking it.
         */
        auto existing =
            m_inputMap.find(
                name);

        if (
            existing
            != m_inputMap.end())
        {
            delete
                existing
                    ->second
                    .input;

            existing
                ->second
                .input =
                    nullptr;
        }

        InputTarget targetInfo;

        targetInfo.input =
            new piranha::pNodeInput;

        targetInfo.memoryTarget =
            target;

        targetInfo.type =
            type;

        m_inputMap[name] =
            targetInfo;
    }

    virtual void registerInputs() {
        for (
            auto &entry
                : m_inputMap)
        {
            InputTarget &target =
                entry.second;

            if (
                target.input
                == nullptr)
            {
                continue;
            }

            registerInput(
                target.input,
                entry.first);
        }
    }

private:
    std::map<
        std::string,
        InputTarget>
        m_inputMap;
};

} /* namespace es_script */

#endif /* ATG_ENGINE_SIM_NODE_H */
