#include "../include/engine_sim_application.h"

#include "../include/combustion_chamber_object.h"
#include "../include/connecting_rod_object.h"
#include "../include/crankshaft_object.h"
#include "../include/cylinder_bank_object.h"
#include "../include/cylinder_head_object.h"
#include "../include/piston_object.h"

#if defined(ATG_ENGINE_SIM_PIRANHA_ENABLED)
#include "../scripting/include/compiler.h"
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

// Compose the engine below the camera origin independently of user pan state.
constexpr float EngineViewCompositionOffsetY = -0.12f;

/*
 * Newer community-engine scripts often end with:
 *
 *     public node main { ... }
 *     main()
 *
 * The old picker always generated a temporary wrapper that imported the file
 * and called main() again. That double-runs self-starting community scripts.
 *
 * This helper strips comments and quoted strings, then looks for a standalone
 * top-level-looking "main(" invocation at the start of a source line.
 */
bool scriptInvokesMainAtFileScope(const std::filesystem::path &path)
{
    std::ifstream file(
        path,
        std::ios::in
            | std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    std::string source(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    std::string cleaned;
    cleaned.reserve(source.size());

    bool lineComment = false;
    bool blockComment = false;
    bool quotedString = false;
    bool escapeNext = false;

    for (
        std::size_t i = 0;
        i < source.size();
        ++i)
    {
        const char c = source[i];

        const char next =
            (i + 1 < source.size())
                ? source[i + 1]
                : '\0';

        if (lineComment) {
            if (c == '\n') {
                lineComment = false;
                cleaned.push_back('\n');
            }

            continue;
        }

        if (blockComment) {
            if (
                c == '*'
                && next == '/')
            {
                blockComment = false;
                ++i;
            }

            continue;
        }

        if (quotedString) {
            if (escapeNext) {
                escapeNext = false;
                continue;
            }

            if (c == '\\') {
                escapeNext = true;
                continue;
            }

            if (c == '"') {
                quotedString = false;
            }

            continue;
        }

        if (
            c == '/'
            && next == '/')
        {
            lineComment = true;
            ++i;
            continue;
        }

        if (
            c == '/'
            && next == '*')
        {
            blockComment = true;
            ++i;
            continue;
        }

        if (c == '"') {
            quotedString = true;
            continue;
        }

        cleaned.push_back(c);
    }

    std::size_t lineStart = 0;

    while (lineStart < cleaned.size()) {
        std::size_t lineEnd =
            cleaned.find(
                '\n',
                lineStart);

        if (lineEnd == std::string::npos) {
            lineEnd = cleaned.size();
        }

        std::size_t p = lineStart;

        while (
            p < lineEnd
            && std::isspace(
                static_cast<unsigned char>(
                    cleaned[p])))
        {
            ++p;
        }

        /*
         * This intentionally does NOT match:
         *
         *     public node main {
         *
         * because that line begins with "public", not "main".
         */
        if (
            cleaned.compare(
                p,
                4,
                "main")
            == 0)
        {
            p += 4;

            while (
                p < lineEnd
                && std::isspace(
                    static_cast<unsigned char>(
                        cleaned[p])))
            {
                ++p;
            }

            if (
                p < lineEnd
                && cleaned[p] == '(')
            {
                return true;
            }
        }

        lineStart =
            lineEnd + 1;
    }

    return false;
}

}

std::string EngineSimApplication::s_buildVersion = "0.2.2";

EngineSimApplication::EngineSimApplication()
    : m_platform(nullptr), m_renderer(nullptr), m_audioOutput(nullptr), m_iceEngine(nullptr),
      m_vehicle(nullptr), m_transmission(nullptr), m_simulator(nullptr),
      m_engineView(nullptr), m_rightGaugeCluster(nullptr), m_oscCluster(nullptr),
      m_temperatureGauge(nullptr), m_performanceCluster(nullptr),
      m_loadSimulationCluster(nullptr), m_mixerCluster(nullptr), m_infoCluster(nullptr),
      m_paused(false), m_screen(0)
{
    m_background = ysColor::srgbiToLinear(0x0E1012);
    m_foreground = ysColor::srgbiToLinear(0xFFFFFF);
    m_shadow = ysColor::srgbiToLinear(0x0E1012);
    m_highlight1 = ysColor::srgbiToLinear(0xEF4545);
    m_highlight2 = ysColor::srgbiToLinear(0xFFFFFF);
    m_pink = ysColor::srgbiToLinear(0xF394BE);
    m_red = ysColor::srgbiToLinear(0xEE4445);
    m_orange = ysColor::srgbiToLinear(0xF4802A);
    m_yellow = ysColor::srgbiToLinear(0xFDBD2E);
    m_blue = ysColor::srgbiToLinear(0x77CEE0);
    m_green = ysColor::srgbiToLinear(0xBDD869);
    m_displayHeight = 0.6096f;
    m_screenWidth = m_screenHeight = 256;
    m_viewParameters = { 0, 0, 0 };
}

EngineSimApplication::~EngineSimApplication() { destroy(); }

void EngineSimApplication::initialize(
    DesktopPlatform *platform,
    Renderer *renderer,
    AudioOutput *audioOutput,
    const RuntimePaths &runtimePaths)
{
    m_platform = platform;
    m_renderer = renderer;
    m_audioOutput = audioOutput;
    m_assetPath = runtimePaths.assetDirectory.string();
    m_authoredMeshes.load((runtimePaths.assetDirectory / "authored_meshes.obj").string());
    m_textRenderer.loadFont((runtimePaths.assetDirectory / "fonts/slkscr.ttf").string());
    m_screenWidth = platform->windowWidth();
    m_screenHeight = platform->windowHeight();

    m_geometryGenerator.initialize(500000, 1000000);

    m_textRenderer.setRenderCallback(
        [this](const std::vector<TextRenderer::Run> &runs, const ysVector &color) {
            m_geometryGenerator.startShape();
            for (const TextRenderer::Run &run : runs) {
                m_geometryGenerator.generateLine2d(
                    { run.x, run.y, run.x + run.width, run.y, run.height });
            }

            GeometryGenerator::GeometryIndices indices;
            m_geometryGenerator.endShape(&indices);
            m_shaders.SetBaseColor(color);
            drawGenerated(indices, 0x20 + m_textRenderer.renderLayer(), m_shaders.GetUiFlags());
        });

    initialize();
}

void EngineSimApplication::initialize() {
    loadScript(m_currentScriptPath);
}

void EngineSimApplication::run() {
    while (tick()) {
        m_platform->delay(1);
    }
}

bool EngineSimApplication::tick() {
    if (m_platform == nullptr) return false;
    if (m_lastTick == 0) m_lastTick = m_platform->ticks();

    std::uint64_t renderIntervalMs = 50;

#if defined(__EMSCRIPTEN__)
    renderIntervalMs = 0;
#elif defined(ENGINE_SIM_IOS)
    if (m_iceEngine != nullptr && std::abs(m_iceEngine->getRpm()) > 100.0) {
        renderIntervalMs = 33;
    }
    else {
        renderIntervalMs = 16;
    }
#endif

    m_platform->pumpEvents();

    if (m_platform->shouldQuit() || m_platform->wasKeyPressed(DesktopKey::Escape)) {
        return false;
    }

    const std::uint64_t now = m_platform->ticks();

    const float dt = std::min(
        static_cast<float>(now - m_lastTick) / 1000.0f,
        0.25f);

    m_lastTick = now;

    if (dt > 0.0f) {
        m_averageFramerate = 0.9f * m_averageFramerate + 0.1f / dt;
    }

    if (m_platform->wasKeyPressed(DesktopKey::F)) toggleFullscreen();
    if (m_platform->wasKeyPressed(DesktopKey::Tab)) m_screen = (m_screen + 1) % 3;
    if (m_platform->wasKeyPressed(DesktopKey::Return)) loadScript(m_currentScriptPath);

    m_screenWidth = m_platform->windowWidth();
    m_screenHeight = m_platform->windowHeight();

    if (dt > 0.0f) {
        processEngineInput(dt);

        if (!m_paused || m_platform->wasKeyPressed(DesktopKey::Right)) {
            process(dt);
        }
    }

    if (m_engineView != nullptr) {
        const float uiDt = std::min(dt, 1.0f / 30.0f);
        m_uiManager.update(uiDt);
    }

    if (!m_pendingScriptPath.empty()) {
        const std::string selectedScript = m_pendingScriptPath;
        m_pendingScriptPath.clear();

        if (loadScript(selectedScript)) {
            m_currentScriptPath = selectedScript;
        }
    }

    if (now - m_lastRenderTick >= renderIntervalMs) {
        renderScene();
        m_lastRenderTick = now;
    }

    return true;
}

void EngineSimApplication::destroy() {
    if (m_audioOutput != nullptr) {
        m_audioOutput->stop();
        m_audioOutput = nullptr;
    }

    destroyObjects();
    m_uiManager.destroy();

    m_engineView = nullptr;
    m_rightGaugeCluster = nullptr;
    m_oscCluster = nullptr;
    m_performanceCluster = nullptr;
    m_loadSimulationCluster = nullptr;
    m_mixerCluster = nullptr;
    m_infoCluster = nullptr;

    if (m_simulator != nullptr) {
        m_simulator->releaseSimulation();
        delete m_simulator;
        m_simulator = nullptr;
    }

    if (m_iceEngine != nullptr) {
        m_iceEngine->destroy();
        delete m_iceEngine;
        m_iceEngine = nullptr;
    }

    delete m_vehicle;
    m_vehicle = nullptr;

    delete m_transmission;
    m_transmission = nullptr;

    m_geometryGenerator.destroy();
}

void EngineSimApplication::process(float dt) {
    if (m_simulator == nullptr) return;

    m_simulator->startFrame(dt);

    while (m_simulator->simulateStep()) {
        if (m_oscCluster != nullptr) {
            m_oscCluster->sample();
        }
    }

    m_simulator->endFrame();
}

void EngineSimApplication::render() {
    for (SimulationObject *object : m_objects) {
        object->generateGeometry();
    }

    for (int sublayer = 0; sublayer < 3; ++sublayer) {
        m_viewParameters.Sublayer = sublayer;

        for (SimulationObject *object : m_objects) {
            object->render(&m_viewParameters);
        }
    }

    if (m_engineView != nullptr) {
        m_uiManager.render();
    }
}

void EngineSimApplication::renderScene() {
    m_renderer->beginFrame(m_shadow);
    m_geometryGenerator.reset();

    if (m_engineView != nullptr) {
        const Bounds windowBounds(
            static_cast<float>(m_screenWidth),
            static_cast<float>(m_screenHeight),
            { 0.0f, static_cast<float>(m_screenHeight) });

        if (m_screen == 0) {
            Grid dashboard = { 3, 2 };
            Grid dashboardThirds = { 3, 3 };
            Grid topStack = { 1, 3 };

            m_engineView->setDrawFrame(true);
            m_engineView->setBounds(dashboard.get(windowBounds, 1, 0));

            m_rightGaugeCluster->m_bounds = dashboard.get(windowBounds, 2, 0, 1, 2);
            m_oscCluster->m_bounds = dashboard.get(windowBounds, 1, 1);
            m_performanceCluster->m_bounds = dashboardThirds.get(windowBounds, 0, 1);
            m_loadSimulationCluster->m_bounds = dashboardThirds.get(windowBounds, 0, 2);

            const Bounds upperLeft = dashboardThirds.get(windowBounds, 0, 0);
            m_mixerCluster->m_bounds = topStack.get(upperLeft, 0, 2);
            m_infoCluster->m_bounds = topStack.get(upperLeft, 0, 0, 1, 2);

            m_engineView->setVisible(true);
            m_rightGaugeCluster->setVisible(true);
            m_oscCluster->setVisible(true);
            m_performanceCluster->setVisible(true);
            m_loadSimulationCluster->setVisible(true);
            m_mixerCluster->setVisible(true);
            m_infoCluster->setVisible(true);
        }
        else if (m_screen == 1) {
            m_engineView->setDrawFrame(false);
            m_engineView->setBounds(windowBounds);
            m_engineView->setVisible(true);

            m_rightGaugeCluster->setVisible(false);
            m_oscCluster->setVisible(false);
            m_performanceCluster->setVisible(false);
            m_loadSimulationCluster->setVisible(false);
            m_mixerCluster->setVisible(false);
            m_infoCluster->setVisible(false);
        }
        else {
            Grid split = { 3, 1 };

            m_engineView->setDrawFrame(true);
            m_engineView->setBounds(split.get(windowBounds, 0, 0, 2, 1));
            m_rightGaugeCluster->m_bounds = split.get(windowBounds, 2, 0);

            m_engineView->setVisible(true);
            m_rightGaugeCluster->setVisible(true);
            m_oscCluster->setVisible(false);
            m_performanceCluster->setVisible(false);
            m_loadSimulationCluster->setVisible(false);
            m_mixerCluster->setVisible(false);
            m_infoCluster->setVisible(false);
        }

        const Point cameraPosition = m_engineView->getCameraPosition();

        m_shaders.m_cameraPosition = ysMath::LoadVector(
            cameraPosition.x,
            cameraPosition.y,
            0.0f,
            1.0f);

        m_shaders.CalculateUiCamera(
            static_cast<float>(m_screenWidth),
            static_cast<float>(m_screenHeight));

        const float cameraAspect =
            m_engineView->m_bounds.width() / m_engineView->m_bounds.height();

        m_shaders.CalculateCamera(
            cameraAspect * m_displayHeight / m_engineView->m_zoom,
            m_displayHeight / m_engineView->m_zoom,
            m_engineView->m_bounds,
            static_cast<float>(m_screenWidth),
            static_cast<float>(m_screenHeight),
            m_displayAngle);

        m_renderer->setSceneViewport(
            m_engineView->m_bounds.left(),
            static_cast<float>(m_screenHeight) - m_engineView->m_bounds.top(),
            m_engineView->m_bounds.width(),
            m_engineView->m_bounds.height());

        render();
    }

    m_renderer->uploadGeometry(
        m_geometryGenerator.getVertexData(),
        m_geometryGenerator.getCurrentVertexCount(),
        m_geometryGenerator.getIndexData(),
        m_geometryGenerator.getCurrentIndexCount());

    m_renderer->endFrame();
}

void EngineSimApplication::drawGenerated(
    const GeometryGenerator::GeometryIndices &indices,
    int layer)
{
    drawGenerated(indices, layer, m_shaders.GetRegularFlags());
}

void EngineSimApplication::drawGeneratedUi(
    const GeometryGenerator::GeometryIndices &indices,
    int layer)
{
    drawGenerated(indices, layer, m_shaders.GetUiFlags());
}

void EngineSimApplication::drawGenerated(
    const GeometryGenerator::GeometryIndices &indices,
    int layer,
    Shaders::StageEnableFlags flags)
{
    if (indices.FaceCount <= 0) return;

    const Shaders::ScreenVariables &screen =
        (flags == m_shaders.GetUiFlags())
            ? m_shaders.m_uiScreenVariables
            : m_shaders.m_screenVariables;

    ysMatrix transform = m_shaders.m_objectVariables.Transform;

    if ((flags & Shaders::SceneStage) != 0) {
        transform = ysMath::MatMult(
            ysMath::TranslationTransform(
                ysMath::LoadVector(0.0f, EngineViewCompositionOffsetY, 0.0f)),
            transform);
    }

    m_renderer->submitGeometry(
        m_geometryGenerator.getVertexData(),
        m_geometryGenerator.getIndexData(),
        indices.BaseVertex,
        indices.BaseIndex,
        indices.FaceCount,
        transform,
        screen.CameraView,
        screen.Projection,
        m_shaders.m_objectVariables.BaseColor,
        flags,
        layer);
}

void EngineSimApplication::drawModel(
    const std::string &modelName,
    Shaders::StageEnableFlags flags,
    int layer)
{
    if (const AuthoredMeshLibrary::Mesh *mesh = m_authoredMeshes.find(modelName)) {
        GeometryGenerator::GeometryIndices imported;

        m_geometryGenerator.startShape();

        m_geometryGenerator.generateRawMesh(
            mesh->vertices.data(),
            static_cast<int>(mesh->vertices.size()),
            mesh->indices.data(),
            static_cast<int>(mesh->indices.size()));

        m_geometryGenerator.endShape(&imported);
        drawGenerated(imported, layer, flags);
    }
}

float EngineSimApplication::pixelsToUnits(float pixels) const {
    return m_engineView == nullptr
        ? pixels
        : pixels * m_displayHeight / m_engineView->m_bounds.height();
}

float EngineSimApplication::unitsToPixels(float units) const {
    return m_engineView == nullptr
        ? units
        : units * m_engineView->m_bounds.height() / m_displayHeight;
}

const SimulationObject::ViewParameters &
EngineSimApplication::getViewParameters() const {
    return m_viewParameters;
}

void EngineSimApplication::configure(const ApplicationSettings &settings) {
    m_applicationSettings = settings;

    m_background = ysColor::srgbiToLinear(settings.colorBackground);
    m_foreground = ysColor::srgbiToLinear(settings.colorForeground);
    m_shadow = ysColor::srgbiToLinear(settings.colorShadow);
    m_highlight1 = ysColor::srgbiToLinear(settings.colorHighlight1);
    m_highlight2 = ysColor::srgbiToLinear(settings.colorHighlight2);
    m_pink = ysColor::srgbiToLinear(settings.colorPink);
    m_red = ysColor::srgbiToLinear(settings.colorRed);
    m_orange = ysColor::srgbiToLinear(settings.colorOrange);
    m_yellow = ysColor::srgbiToLinear(settings.colorYellow);
    m_blue = ysColor::srgbiToLinear(settings.colorBlue);
    m_green = ysColor::srgbiToLinear(settings.colorGreen);

    if (settings.startFullscreen) {
        m_platform->setFullscreen(true);
    }
}

void EngineSimApplication::requestEngineScript(
    const std::string &relativeScriptPath)
{
    if (!relativeScriptPath.empty()) {
        m_pendingScriptPath = relativeScriptPath;
    }
}

bool EngineSimApplication::loadScript(
    const std::string &relativeScriptPath)
{
#if defined(ATG_ENGINE_SIM_PIRANHA_ENABLED)
    es_script::Compiler compiler;
    compiler.initialize(m_assetPath);

    Engine *engine = nullptr;
    Vehicle *vehicle = nullptr;
    Transmission *transmission = nullptr;

    std::filesystem::path scriptPath =
        std::filesystem::path(m_assetPath) / relativeScriptPath;

    std::filesystem::path entryPointPath = scriptPath;
    std::filesystem::path generatedEntryPoint;

    if (relativeScriptPath != "main.mr") {
        /*
         * Old-style engine files define public node main but expect the host
         * to invoke it. Newer community files may already invoke main()
         * themselves at file scope.
         *
         * Only generate the wrapper when the selected script is NOT already
         * self-starting.
         */
        if (!scriptInvokesMainAtFileScope(scriptPath)) {
            generatedEntryPoint =
                std::filesystem::temp_directory_path()
                / "engine-sim-picker-entry.mr";

            std::ofstream entryPoint(generatedEntryPoint);

            entryPoint
                << "import \""
                << scriptPath.generic_string()
                << "\"\n\nmain()\n";

            entryPoint.close();
            entryPointPath = generatedEntryPoint;
        }
    }

    if (compiler.compile(entryPointPath.string())) {
        const es_script::Compiler::Output output = compiler.execute();

        configure(output.applicationSettings);

        engine = output.engine;
        vehicle = output.vehicle;
        transmission = output.transmission;
    }

    compiler.destroy();

    if (!generatedEntryPoint.empty()) {
        std::error_code ignored;
        std::filesystem::remove(generatedEntryPoint, ignored);
    }

    if (engine != nullptr && vehicle != nullptr && transmission != nullptr) {
        loadEngine(engine, vehicle, transmission);
        return true;
    }

    if (m_infoCluster != nullptr) {
        m_infoCluster->setLogMessage("Engine script failed to load");
    }
#endif

    return false;
}

void EngineSimApplication::processEngineInput(float dt) {
    if (m_iceEngine == nullptr || m_simulator == nullptr) return;

    const bool fineControl = m_platform->isKeyDown(DesktopKey::Space);
    const float wheel = m_platform->mouseWheelY();
    bool consumedWheel = false;

    auto log = [this](const std::string &message) {
        if (m_infoCluster != nullptr) {
            m_infoCluster->setLogMessage(message);
        }
    };

    auto updateAudio = [&](auto update, const char *message) {
        Synthesizer::AudioParameters parameters =
            m_simulator->synthesizer().getAudioParameters();

        update(parameters);

        m_simulator->synthesizer().setAudioParameters(parameters);

        consumedWheel = true;
        log(message);
    };

    if (m_platform->isKeyDown(DesktopKey::Z)) {
        updateAudio(
            [&](auto &p) {
                p.volume = clamp(
                    p.volume + wheel * (fineControl ? 0.001 : 0.01) * dt);
            },
            "[Z] - volume");
    }
    else if (m_platform->isKeyDown(DesktopKey::X)) {
        updateAudio(
            [&](auto &p) {
                p.convolution = clamp(
                    p.convolution + wheel * (fineControl ? 0.001 : 0.01) * dt);
            },
            "[X] - convolution");
    }
    else if (m_platform->isKeyDown(DesktopKey::C)) {
        updateAudio(
            [&](auto &p) {
                p.dF_F_mix = clamp(
                    p.dF_F_mix + wheel * (fineControl ? 0.00001 : 0.001) * dt);
            },
            "[C] - high frequency gain");
    }
    else if (m_platform->isKeyDown(DesktopKey::V)) {
        updateAudio(
            [&](auto &p) {
                p.airNoise = clamp(
                    p.airNoise + wheel * (fineControl ? 0.001 : 0.01) * dt);
            },
            "[V] - low frequency noise");
    }
    else if (m_platform->isKeyDown(DesktopKey::B)) {
        updateAudio(
            [&](auto &p) {
                p.inputSampleNoise = clamp(
                    p.inputSampleNoise
                    + wheel * (fineControl ? 0.001 : 0.01) * dt);
            },
            "[B] - high frequency noise");
    }
    else if (m_platform->isKeyDown(DesktopKey::N)) {
        m_simulator->setSimulationFrequency(
            clamp(
                m_simulator->getSimulationFrequency()
                + wheel * (fineControl ? 10.0 : 100.0) * dt,
                400.0,
                400000.0));

        consumedWheel = true;
    }
    else if (
        m_platform->isKeyDown(DesktopKey::G)
        && m_simulator->m_dyno.m_hold)
    {
        m_dynoSpeed = clamp(
            m_dynoSpeed + wheel * m_iceEngine->getDynoHoldStep(),
            m_iceEngine->getDynoMinSpeed(),
            m_iceEngine->getDynoMaxSpeed());

        consumedWheel = true;
    }

    m_targetSpeedSetting =
        fineControl ? m_targetSpeedSetting : 0.0;

    if (m_platform->isKeyDown(DesktopKey::Q)) m_targetSpeedSetting = 0.01;
    else if (m_platform->isKeyDown(DesktopKey::W)) m_targetSpeedSetting = 0.1;
    else if (m_platform->isKeyDown(DesktopKey::E)) m_targetSpeedSetting = 0.2;
    else if (m_platform->isKeyDown(DesktopKey::R)) m_targetSpeedSetting = 1.0;
    else if (fineControl && !consumedWheel) {
        m_targetSpeedSetting = clamp(
            m_targetSpeedSetting + wheel * 0.0001);
    }

    if (m_touchThrottleHeld) {
        m_targetSpeedSetting = m_touchThrottle;
    }

    m_speedSetting =
        m_targetSpeedSetting * 0.5
        + m_speedSetting * 0.5;

    m_iceEngine->setSpeedControl(m_speedSetting);

    if (
        m_platform->wasKeyPressed(DesktopKey::M)
        && m_viewParameters.Layer0 + 1 < m_iceEngine->getMaxDepth())
    {
        ++m_viewParameters.Layer0;
    }

    if (
        m_platform->wasKeyPressed(DesktopKey::Comma)
        && m_viewParameters.Layer0 > 0)
    {
        --m_viewParameters.Layer0;
    }

    if (m_platform->wasKeyPressed(DesktopKey::D)) toggleDynamometer();
    if (m_platform->wasKeyPressed(DesktopKey::H)) toggleDynamometerHold();
    if (m_platform->wasKeyPressed(DesktopKey::A)) toggleIgnition();
    if (m_platform->wasKeyPressed(DesktopKey::Up)) changeGear(1);
    if (m_platform->wasKeyPressed(DesktopKey::Down)) changeGear(-1);

    if (m_platform->isKeyDown(DesktopKey::T)) {
        m_targetClutchPressure -= 0.2 * dt;
    }
    else if (m_platform->isKeyDown(DesktopKey::U)) {
        m_targetClutchPressure += 0.2 * dt;
    }
    else if (m_platform->isKeyDown(DesktopKey::Space)) {
        m_targetClutchPressure = 0.0;
    }
    else if (!m_platform->isKeyDown(DesktopKey::Y)) {
        m_targetClutchPressure = 1.0;
    }

    m_targetClutchPressure = clamp(m_targetClutchPressure);

    const double clutchRC =
        fineControl ? 1.0 : 0.001;

    const double clutchS =
        dt / (dt + clutchRC);

    m_clutchPressure =
        m_clutchPressure * (1 - clutchS)
        + m_targetClutchPressure * clutchS;

    m_simulator->getTransmission()->setClutchPressure(
        m_clutchPressure);

    if (
        m_simulator->m_dyno.m_enabled
        && !m_simulator->m_dyno.m_hold)
    {
        m_dynoSpeed =
            m_simulator->getFilteredDynoTorque()
                > units::torque(1.0, units::ft_lb)
            ? m_dynoSpeed + units::rpm(500) * dt
            : m_dynoSpeed / (1 + dt);
    }
    else if (!m_simulator->m_dyno.m_hold) {
        m_dynoSpeed = units::rpm(0);
    }

    m_simulator->m_dyno.m_rotationSpeed = clamp(
        m_dynoSpeed,
        m_iceEngine->getDynoMinSpeed(),
        m_iceEngine->getDynoMaxSpeed());

    m_simulator->m_starterMotor.m_enabled =
        m_platform->isKeyDown(DesktopKey::S)
        || m_touchStarterHeld;
}

void EngineSimApplication::toggleIgnition() {
    if (
        m_simulator == nullptr
        || m_simulator->getEngine() == nullptr)
    {
        return;
    }

    auto *ignition =
        m_simulator->getEngine()->getIgnitionModule();

    ignition->m_enabled = !ignition->m_enabled;
}

void EngineSimApplication::toggleDynamometer() {
    if (m_simulator != nullptr) {
        m_simulator->m_dyno.m_enabled =
            !m_simulator->m_dyno.m_enabled;
    }
}

void EngineSimApplication::toggleDynamometerHold() {
    if (m_simulator != nullptr) {
        m_simulator->m_dyno.m_hold =
            !m_simulator->m_dyno.m_hold;
    }
}

void EngineSimApplication::changeGear(int direction) {
    if (
        m_simulator == nullptr
        || m_simulator->getTransmission() == nullptr)
    {
        return;
    }

    Transmission *transmission =
        m_simulator->getTransmission();

    transmission->changeGear(
        transmission->getGear() + direction);
}

void EngineSimApplication::setTouchStarterHeld(bool held) {
    m_touchStarterHeld = held;
}

void EngineSimApplication::setTouchThrottle(double value, bool held) {
    m_touchThrottle = clamp(value);
    m_touchThrottleHeld = held;
}

void EngineSimApplication::createObjects(Engine *engine) {
    for (int i = 0; i < engine->getCylinderCount(); ++i) {
        auto *rod = new ConnectingRodObject;
        rod->initialize(this);
        rod->m_connectingRod = engine->getConnectingRod(i);
        m_objects.push_back(rod);

        auto *piston = new PistonObject;
        piston->initialize(this);
        piston->m_piston = engine->getPiston(i);
        m_objects.push_back(piston);

        auto *chamber = new CombustionChamberObject;
        chamber->initialize(this);
        chamber->m_chamber = engine->getChamber(i);
        m_objects.push_back(chamber);
    }

    for (int i = 0; i < engine->getCrankshaftCount(); ++i) {
        auto *crankshaft = new CrankshaftObject;
        crankshaft->initialize(this);
        crankshaft->m_crankshaft = engine->getCrankshaft(i);
        m_objects.push_back(crankshaft);
    }

    for (int i = 0; i < engine->getCylinderBankCount(); ++i) {
        auto *bank = new CylinderBankObject;
        bank->initialize(this);
        bank->m_bank = engine->getCylinderBank(i);
        bank->m_head = engine->getHead(i);
        m_objects.push_back(bank);

        auto *head = new CylinderHeadObject;
        head->initialize(this);
        head->m_head = engine->getHead(i);
        head->m_engine = engine;
        m_objects.push_back(head);
    }
}

void EngineSimApplication::destroyObjects() {
    for (auto *object : m_objects) {
        object->destroy();
        delete object;
    }

    m_objects.clear();
}

void EngineSimApplication::toggleFullscreen() {
    if (m_platform != nullptr) {
        m_platform->setFullscreen(
            !m_platform->isFullscreen());
    }
}

void EngineSimApplication::showControlsOverlay() {
    m_uiManager.showControlsOverlay();
}

void EngineSimApplication::showEnginePickerOverlay() {
    m_uiManager.showEnginePickerOverlay();
}

void EngineSimApplication::refreshUserInterface() {
    m_uiManager.destroy();

    m_engineView = nullptr;
    m_rightGaugeCluster = nullptr;
    m_oscCluster = nullptr;
    m_performanceCluster = nullptr;
    m_loadSimulationCluster = nullptr;
    m_mixerCluster = nullptr;
    m_infoCluster = nullptr;

    if (m_simulator == nullptr) return;

    m_uiManager.initialize(this, m_platform);

    m_engineView =
        m_uiManager.getRoot()->addElement<EngineView>();

    m_rightGaugeCluster =
        m_uiManager.getRoot()->addElement<RightGaugeCluster>();

    m_oscCluster =
        m_uiManager.getRoot()->addElement<OscilloscopeCluster>();

    m_performanceCluster =
        m_uiManager.getRoot()->addElement<PerformanceCluster>();

    m_loadSimulationCluster =
        m_uiManager.getRoot()->addElement<LoadSimulationCluster>();

    m_mixerCluster =
        m_uiManager.getRoot()->addElement<MixerCluster>();

    m_infoCluster =
        m_uiManager.getRoot()->addElement<InfoCluster>();

    m_infoCluster->setEngine(m_iceEngine);

    m_rightGaugeCluster->m_simulator = m_simulator;
    m_rightGaugeCluster->setEngine(m_iceEngine);

    m_oscCluster->setSimulator(m_simulator);
    m_performanceCluster->setSimulator(m_simulator);
    m_loadSimulationCluster->setSimulator(m_simulator);
    m_mixerCluster->setSimulator(m_simulator);
}

void EngineSimApplication::loadEngine(
    Engine *engine,
    Vehicle *vehicle,
    Transmission *transmission)
{
    if (m_audioOutput != nullptr) {
        m_audioOutput->stop();
    }

    destroyObjects();
    m_uiManager.destroy();

    m_engineView = nullptr;
    m_rightGaugeCluster = nullptr;
    m_oscCluster = nullptr;
    m_performanceCluster = nullptr;
    m_loadSimulationCluster = nullptr;
    m_mixerCluster = nullptr;
    m_infoCluster = nullptr;

    if (m_simulator != nullptr) {
        m_simulator->releaseSimulation();
        delete m_simulator;
    }

    if (m_iceEngine != nullptr) {
        m_iceEngine->destroy();
        delete m_iceEngine;
    }

    delete m_vehicle;
    delete m_transmission;

    m_speedSetting = 0.0;
    m_targetSpeedSetting = 0.0;
    m_touchThrottle = 0.0;
    m_touchThrottleHeld = false;
    m_touchStarterHeld = false;
    m_clutchPressure = 0.0;
    m_targetClutchPressure = 0.0;
    m_dynoSpeed = 0.0;

    m_iceEngine = engine;
    m_vehicle = vehicle;
    m_transmission = transmission;
    m_simulator = nullptr;

    if (
        engine == nullptr
        || vehicle == nullptr
        || transmission == nullptr)
    {
        return;
    }

    const int outputAudioSampleRate =
        m_audioOutput != nullptr
            ? m_audioOutput->outputSampleRate()
            : 44100;

    m_simulator =
        engine->createSimulator(
            vehicle,
            transmission,
            outputAudioSampleRate);

    m_viewParameters.Layer1 =
        engine->getMaxDepth();

    engine->calculateDisplacement();

    m_simulator->setSimulationFrequency(
        engine->getSimulationFrequency());

    m_simulator->setTargetSynthesizerLatency(0.1);
    m_simulator->setSynthesizerLatencyCorrectionEnabled(false);
    m_simulator->setMaximumSynthesizerInputLatency(0.03);

    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        ImpulseResponse *response =
            engine->getExhaustSystem(i)->getImpulseResponse();

        if (
            response != nullptr
            && m_audioOutput != nullptr)
        {
            m_audioOutput->loadImpulseResponse(
                m_simulator->synthesizer(),
                response->getFilename(),
                response->getVolume(),
                i);
        }
    }

    m_simulator->startAudioRenderingThread();

    if (m_audioOutput != nullptr) {
        m_audioOutput->start(m_simulator);
    }

    createObjects(engine);
    refreshUserInterface();
}
