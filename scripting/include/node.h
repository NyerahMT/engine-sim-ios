#ifndef ATG_ENGINE_SIM_NODE_H
#define ATG_ENGINE_SIM_NODE_H

#include "piranha.h"

#include <cstdio>
#include <cstdint>
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
            /*
             * TEMPORARY DIAGNOSTIC TRACE.
             *
             * Intentionally extremely noisy.
             * Revert this diagnostic commit once the offending
             * community-engine input has been identified.
             */

            std::fprintf(
                stderr,
                "[ES-NODE-TRACE] BEGIN this=%p node='%s' builtin='%s' inputs=%zu\n",
                static_cast<void *>(this),
                getName().c_str(),
                getBuiltinName().c_str(),
                m_inputMap.size());
            std::fflush(stderr);

            std::size_t inputIndex = 0;

            for (const auto &entry : m_inputMap) {
                const std::string &name = entry.first;
                const InputTarget &target = entry.second;

                std::fprintf(
                    stderr,
                    "[ES-NODE-TRACE] PRE this=%p idx=%zu input='%s' slot=%p mem=%p type=%d\n",
                    static_cast<void *>(this),
                    inputIndex,
                    name.c_str(),
                    static_cast<void *>(target.input),
                    target.memoryTarget,
                    static_cast<int>(target.type));
                std::fflush(stderr);

                if (
                    target.type != InputTarget::Type::Atomic
                    && target.type != InputTarget::Type::Object)
                {
                    std::fprintf(
                        stderr,
                        "[ES-NODE-TRACE] SKIP this=%p idx=%zu input='%s' reason=bad-type\n",
                        static_cast<void *>(this),
                        inputIndex,
                        name.c_str());
                    std::fflush(stderr);

                    ++inputIndex;
                    continue;
                }

                if (target.input == nullptr) {
                    std::fprintf(
                        stderr,
                        "[ES-NODE-TRACE] SKIP this=%p idx=%zu input='%s' reason=null-slot\n",
                        static_cast<void *>(this),
                        inputIndex,
                        name.c_str());
                    std::fflush(stderr);

                    ++inputIndex;
                    continue;
                }

                piranha::pNodeInput input = *target.input;

                std::fprintf(
                    stderr,
                    "[ES-NODE-TRACE] SLOT this=%p idx=%zu input='%s' value=%p\n",
                    static_cast<void *>(this),
                    inputIndex,
                    name.c_str(),
                    static_cast<void *>(input));
                std::fflush(stderr);

                if (input == nullptr) {
                    std::fprintf(
                        stderr,
                        "[ES-NODE-TRACE] SKIP this=%p idx=%zu input='%s' reason=unbound\n",
                        static_cast<void *>(this),
                        inputIndex,
                        name.c_str());
                    std::fflush(stderr);

                    ++inputIndex;
                    continue;
                }

                if (target.memoryTarget == nullptr) {
                    std::fprintf(
                        stderr,
                        "[ES-NODE-TRACE] SKIP this=%p idx=%zu input='%s' reason=null-memory-target\n",
                        static_cast<void *>(this),
                        inputIndex,
                        name.c_str());
                    std::fflush(stderr);

                    ++inputIndex;
                    continue;
                }

                std::fprintf(
                    stderr,
                    "[ES-NODE-TRACE] COMPUTE-BEGIN this=%p idx=%zu input='%s' output=%p target=%p\n",
                    static_cast<void *>(this),
                    inputIndex,
                    name.c_str(),
                    static_cast<void *>(input),
                    target.memoryTarget);
                std::fflush(stderr);

                input->fullCompute(target.memoryTarget);

                std::fprintf(
                    stderr,
                    "[ES-NODE-TRACE] COMPUTE-END this=%p idx=%zu input='%s'\n",
                    static_cast<void *>(this),
                    inputIndex,
                    name.c_str());
                std::fflush(stderr);

                ++inputIndex;
            }

            std::fprintf(
                stderr,
                "[ES-NODE-TRACE] END this=%p node='%s' builtin='%s'\n",
                static_cast<void *>(this),
                getName().c_str(),
                getBuiltinName().c_str());
            std::fflush(stderr);
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
