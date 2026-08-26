set -e

echo "Restoring clean renderer..."
curl -L \
  "https://raw.githubusercontent.com/NyerahMT/engine-sim-ios/f3f8032cad7899482e593f2f1b64a4995ddcca3d/src/sdl_gpu_renderer.cpp" \
  -o src/sdl_gpu_renderer.cpp

echo "Restoring clean UI element..."
curl -L \
  "https://raw.githubusercontent.com/NyerahMT/engine-sim-ios/534f78d7473fc494a74b72eaa388a4a4dd6f446e/src/ui_element.cpp" \
  -o src/ui_element.cpp

echo "Restoring clean desktop application..."
curl -L \
  "https://raw.githubusercontent.com/NyerahMT/engine-sim-ios/f12d9efdd70cac4c72b65bcd09bc9c5e9d69d9a3/src/desktop_application.cpp" \
  -o src/desktop_application.cpp

echo "Removing the iOS 30 FPS running-engine cap..."

python3 <<'PY'
from pathlib import Path

p = Path("src/desktop_application.cpp")
s = p.read_text()

old = '''#elif defined(ENGINE_SIM_IOS)
    // Keep the beautiful 60 FPS dashboard when the engine isn't doing work,
    // then deliberately reserve CPU for simulation/audio while it is running.
    if (m_iceEngine != nullptr && std::abs(m_iceEngine->getRpm()) > 100.0) {
        renderIntervalMs = 33; // ~30 FPS
    }
    else {
        renderIntervalMs = 16; // ~60 FPS
    }
#endif'''

new = '''#elif defined(ENGINE_SIM_IOS)
    // iOS renders continuously at approximately 60 FPS.
    // Physics/audio remain tied to elapsed time independently.
    renderIntervalMs = 16;
#endif'''

if old not in s:
    raise SystemExit("ERROR: expected iOS render throttle block not found")

p.write_text(s.replace(old, new))
PY

echo
echo "Checking that diagnostic spam is gone..."

if grep -R -n -E '\[UI-TREE\]|\[RENDER \]|\[GPU-LOG \]|rendererDiagnosticLog|uiRenderDiagnosticLog|renderDiagnosticLog' \
    src/ui_element.cpp src/desktop_application.cpp src/sdl_gpu_renderer.cpp
then
    echo "ERROR: diagnostic logging still found"
    exit 1
fi

echo
echo "Checking permanent gauge fix is still present..."

grep -n "float m_minorStep" include/gauge.h
grep -n 'm_unit =.*"ms"' src/performance_cluster.cpp || true

echo
echo "DONE."
echo
echo "Files replaced:"
echo "  src/sdl_gpu_renderer.cpp"
echo "  src/ui_element.cpp"
echo "  src/desktop_application.cpp"
echo
echo "Permanent files LEFT ALONE:"
echo "  include/gauge.h"
echo "  src/gauge.cpp"
echo "  src/performance_cluster.cpp"
