git checkout 305387cf7cf13b7d4816482a0d0af5a11bd2150e -- \
assets/es/engine_sim.mr \
assets/es/objects/objects.mr \
include/combustion_chamber.h \
include/engine.h \
include/transmission.h \
include/vehicle.h \
scripting/include/engine_node.h \
scripting/include/node.h \
scripting/include/transmission_node.h \
scripting/include/vehicle_node.h \
src/convolution_filter.cpp \
src/engine.cpp \
src/piston_engine_simulator.cpp \
src/sdl_audio_output.cpp \
src/synthesizer.cpp \
src/transmission.cpp \
src/vehicle.cpp

git rm include/engine_sim_clutch_constraint.h src/engine_sim_clutch_constraint.cpp

git add -A
git commit -m "Restore pre-Community Edition simulator runtime"
git push