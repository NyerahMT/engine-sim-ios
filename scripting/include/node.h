#ifndef ATG_ENGINE_SIM_NODE_H
#define ATG_ENGINE_SIM_NODE_H

#include "piranha.h"

#include <cstdio>
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

            piranha::pNodeInput *input = nullptr;
            void *memoryTarget = nullptr;
            Type type = Type::Atomic;
        };

    public:
        Node() {
            /* void */
        }

        virtual ~Node() {
            for (auto i : m_inputMap) {
                delete i.second.input;
            }
        }

        template <typename T_Out>
        T_Out readAtomicInput(const std::string &name) {
            T_Out out{};

            const auto it = m_inputMap.find(name);
            if (it == m_inputMap.end()) {
                std::fprintf(
                    stderr,
                    "[EngineSim Script] Tried to read unregistered input '%s'\n",
                    name.c_str());
                return out;
            }

            const InputTarget &target = it->second;

            /*
             * pNodeInput is a pointer to a Piranha input pointer.
             *
             * If the C++ builtin registers an input name which the matching
             * .mr builtin declaration does not expose, Piranha leaves the
             * inner pointer null. The old code dereferenced it unconditionally
             * and crashed in Node::readAllInputs().
             */
            if (target.input == nullptr || *target.input == nullptr) {
                std::fprintf(
                    stderr,
                    "[EngineSim Script] Missing/unbound input '%s'; using default value\n",
                    name.c_str());
                return out;
            }

            (*target.input)->fullCompute(&out);
            return out;
        }

        void readAllInputs() {
            for (const auto &entry : m_inputMap) {
                const std::string &name = entry.first;
                const InputTarget &target = entry.second;

                if (
                    target.type != InputTarget::Type::Atomic
                    && target.type != InputTarget::Type::Object)
                {
                    continue;
                }

                /*
                 * Do not let a script/API compatibility mismatch turn into a
                 * native SIGSEGV. This is the exact null dereference shown by
                 * the iOS crash reports in es_script::Node::readAllInputs().
                 *
                 * Leaving memoryTarget untouched preserves the C++ node's
                 * initialized/default value for optional compatibility inputs.
                 */
                if (target.input == nullptr || *target.input == nullptr) {
                    std::fprintf(
                        stderr,
                        "[EngineSim Script] Missing/unbound input '%s'; keeping default value\n",
                        name.c_str());
                    continue;
                }

                if (target.memoryTarget == nullptr) {
                    std::fprintf(
                        stderr,
                        "[EngineSim Script] Input '%s' has no memory target; skipping\n",
                        name.c_str());
                    continue;
                }

                (*target.input)->fullCompute(target.memoryTarget);
            }
        }

        void addInput(
            const std::string &name,
            void *target,
            InputTarget::Type type = InputTarget::Type::Atomic)
        {
            m_inputMap[name] = {
                new piranha::pNodeInput,
                target,
                type
            };
        }

        virtual void registerInputs() {
            for (auto i : m_inputMap) {
                registerInput(i.second.input, i.first);
            }
        }

    private:
        std::map<std::string, InputTarget> m_inputMap;
    };

} /* namespace es_script */

#endif /* ATG_ENGINE_SIM_NODE_H */
