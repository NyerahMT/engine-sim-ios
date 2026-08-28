#include "../include/info_cluster.h"

#include "../include/engine_sim_application.h"

#include <iomanip>
#include <sstream>

InfoCluster::InfoCluster()
    : m_engine(nullptr), m_projectInfoButton(nullptr), m_enginePickerButton(nullptr),
      m_settingsButton(nullptr), m_logMessage("Started") { }

InfoCluster::~InfoCluster() = default;

void InfoCluster::initialize(EngineSimApplication *app) {
    UiElement::initialize(app);
    m_projectInfoButton = addElement<UiButton>(this);
    m_projectInfoButton->m_text = "INFO";
    m_projectInfoButton->m_fontSize = 16.0f;
    m_projectInfoButton->m_inverted = true;
    m_projectInfoButton->m_drawFrame = false;

    m_enginePickerButton = addElement<UiButton>(this);
    m_enginePickerButton->m_text = "SELECT";
    m_enginePickerButton->m_fontSize = 16.0f;
    m_enginePickerButton->m_inverted = true;
    m_enginePickerButton->m_drawFrame = false;

    m_settingsButton = addElement<UiButton>(this);
    m_settingsButton->m_text = "SETTINGS";
    m_settingsButton->m_fontSize = 16.0f;
    m_settingsButton->m_inverted = true;
    m_settingsButton->m_drawFrame = false;
}

void InfoCluster::destroy() { UiElement::destroy(); }

void InfoCluster::update(float dt) {
    Grid grid = { 6, 4 };
    const Bounds titleBounds = grid.get(m_bounds, 1, 0, 5, 2);
    const Bounds toolbar = titleBounds.verticalSplit(0.0f, 0.24f);
    m_enginePickerButton->m_bounds = toolbar.horizontalSplit(0.47f, 0.60f);
    m_projectInfoButton->m_bounds = toolbar.horizontalSplit(0.61f, 0.74f);
    m_settingsButton->m_bounds = toolbar.horizontalSplit(0.75f, 1.0f);
    UiElement::update(dt);
}

void InfoCluster::signal(UiElement *element, Event event) {
    if (event != Event::Clicked) return;
    if (element == m_settingsButton) m_app->showSettingsOverlay();
    else if (element == m_projectInfoButton) m_app->showControlsOverlay();
    else if (element == m_enginePickerButton) m_app->showEnginePickerOverlay();
}

void InfoCluster::render() {
    Grid grid = { 6, 4 };
    const Bounds logoBounds = grid.get(m_bounds, 0, 0, 1, 2);
    drawFrame(logoBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    // Start with the original authored OES logo so E/S retain the exact
    // source artwork instead of being recreated with a font.
    resetShader();
    const Point logoPosition =
        getRenderPoint(
            logoBounds.getPosition(
                Bounds::center));

    m_app->getShaders()->SetObjectTransform(
        ysMath::MatMult(
            ysMath::TranslationTransform(
                ysMath::LoadVector(
                    logoPosition.x,
                    logoPosition.y,
                    0.0f)),
            ysMath::ScaleTransform(
                ysMath::LoadVector(
                    logoBounds.width() * 0.58f,
                    logoBounds.height() * 0.72f,
                    1.0f))));

    m_app->getShaders()->SetBaseColor(
        m_app->getForegroundColor());

    m_app->drawModel(
        "Logo",
        m_app->getShaders()->GetUiFlags(),
        0x12);

    // The authored mesh reads OES. Cover only its left-hand O and replace it
    // with a simple geometric lowercase i, leaving the original E/S untouched.
    const Bounds logoInterior =
        logoBounds.inset(2.0f);

    const Bounds oldOBounds =
        logoInterior.horizontalSplit(
            0.0f,
            0.355f);

    drawBox(
        oldOBounds,
        m_app->getBackgroundColor(),
        0x13);

    const Bounds iArea =
        logoInterior
            .horizontalSplit(
                0.055f,
                0.285f)
            .verticalSplit(
                0.17f,
                0.83f);

    const float stemWidth =
        iArea.width() * 0.24f;

    const Bounds stem(
        stemWidth,
        iArea.height() * 0.56f,
        {
            iArea.center_h(),
            iArea.bottom()
                + iArea.height() * 0.05f
        },
        Bounds::bm);

    const Bounds dot(
        stemWidth,
        stemWidth,
        {
            iArea.center_h(),
            iArea.top()
                - iArea.height() * 0.08f
        },
        Bounds::tm);

    drawBox(
        stem,
        m_app->getForegroundColor(),
        0x14);

    drawBox(
        dot,
        m_app->getForegroundColor(),
        0x14);

    const Bounds titleBounds = grid.get(m_bounds, 1, 0, 5, 2);
    drawFrame(titleBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    const Bounds titleTextBounds = titleBounds.verticalSplit(0.62f, 1.0f);
    const Bounds subtitleBounds = titleBounds.verticalSplit(0.44f, 0.62f);
    const Bounds buildBounds = titleBounds.verticalSplit(0.28f, 0.44f);
    const Bounds toolbarBounds = titleBounds.verticalSplit(0.0f, 0.24f);
    const auto fittedHeight = [this](const std::string &text, float requestedHeight) {
        const float maximumWidth = m_bounds.width() * 0.80f;
        const float requestedWidth = m_app->getTextRenderer()->CalculateWidth(text, requestedHeight);
        return requestedWidth > maximumWidth ? requestedHeight * maximumWidth / requestedWidth : requestedHeight;
    };
    drawAlignedText("ENGINE SIMULATOR: iOS", titleTextBounds.inset(10.0f).move({ 0.0f, -21.0f }),
        fittedHeight("ENGINE SIMULATOR: iOS", 42.0f), Bounds::bl, Bounds::bl);
    drawAlignedText("ORIGINAL BY ANGETHEGREAT", subtitleBounds.inset(10.0f).move({ 0.0f, 3.0f }),
        fittedHeight("ORIGINAL BY ANGETHEGREAT", 24.0f), Bounds::tl, Bounds::tl);
    drawAlignedText("BUILD: v" + EngineSimApplication::getBuildVersion() + " // " __DATE__,
        buildBounds.inset(10.0f).move({ 0.0f, 4.0f }), 16.0f, Bounds::tl, Bounds::tl);
    drawFrame(toolbarBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    const Bounds engineInfoBounds = grid.get(m_bounds, 0, 2, 6, 1);
    drawFrame(engineInfoBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    drawAlignedText(m_engine != nullptr ? m_engine->getName() : "<NO ENGINE>",
        engineInfoBounds.horizontalSplit(0.0f, 0.66f).inset(10.0f), 24.0f, Bounds::lm, Bounds::lm);

    std::stringstream specs;
    if (m_engine != nullptr) {
        specs << std::fixed;
        if (m_engine->getDisplacement() < units::volume(1.0, units::L))
            specs << std::setprecision(0) << units::convert(m_engine->getDisplacement(), units::cc) << " cc -- ";
        else specs << std::setprecision(1) << units::convert(m_engine->getDisplacement(), units::L) << " L -- ";
        specs << std::setprecision(0) << units::convert(m_engine->getDisplacement(), units::cubic_inches) << " CI";
    }
    else specs << "N/A";
    drawAlignedText(specs.str(), engineInfoBounds.horizontalSplit(0.66f, 1.0f).inset(10.0f),
        24.0f, Bounds::rm, Bounds::rm);

    const Bounds messageBounds = grid.get(m_bounds, 0, 3, 6, 1);
    drawFrame(messageBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());
    drawAlignedText(m_logMessage, messageBounds.inset(10.0f), 24.0f, Bounds::lm, Bounds::lm);
    UiElement::render();
}
