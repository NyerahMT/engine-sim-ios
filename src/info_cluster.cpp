#include "../include/info_cluster.h"

#include "../include/engine_sim_application.h"
#include "../include/ui_utilities.h"

#include <sstream>
#include <iomanip>
#include <algorithm>

InfoCluster::InfoCluster() {
    m_engine = nullptr;
    m_projectInfoButton = nullptr;
    m_fullscreenButton = nullptr;
    m_closeControlsButton = nullptr;
    m_githubButton = nullptr;
    m_issuesButton = nullptr;
    m_controlsVisible = false;
    m_logMessage = "Started";
}

InfoCluster::~InfoCluster() {
    /* void */
}

void InfoCluster::initialize(EngineSimApplication *app) {
    UiElement::initialize(app);
    m_projectInfoButton = addElement<UiButton>(this);
    m_projectInfoButton->m_text = "INFO";
    m_projectInfoButton->m_fontSize = 16.0f;
    m_projectInfoButton->m_inverted = true;
    m_projectInfoButton->m_drawFrame = false;
    m_fullscreenButton = addElement<UiButton>(this);
    m_fullscreenButton->m_text = "Fullscreen";
    m_fullscreenButton->m_fontSize = 16.0f;
    m_fullscreenButton->m_inverted = true;
    m_fullscreenButton->m_drawFrame = false;
    m_closeControlsButton = addElement<UiButton>(this);
    m_closeControlsButton->m_text = "CLOSE";
    m_closeControlsButton->m_fontSize = 16.0f;
    m_closeControlsButton->m_inverted = true;
    m_closeControlsButton->m_drawFrame = false;
    m_closeControlsButton->m_layer = 0x20;
    m_githubButton = addElement<UiButton>(this);
    m_githubButton->m_text = "GITHUB";
    m_githubButton->m_fontSize = 14.0f;
    m_githubButton->m_inverted = true;
    m_githubButton->m_drawFrame = false;
    m_githubButton->m_layer = 0x20;
    m_issuesButton = addElement<UiButton>(this);
    m_issuesButton->m_text = "ISSUES";
    m_issuesButton->m_fontSize = 14.0f;
    m_issuesButton->m_inverted = true;
    m_issuesButton->m_drawFrame = false;
    m_issuesButton->m_layer = 0x20;
    setControlsVisible(false);
}

void InfoCluster::destroy() {
    UiElement::destroy();
}

void InfoCluster::update(float dt) {
    if (m_app->getPlatform()->wasKeyPressed(DesktopKey::F1)) {
        setControlsVisible(!m_controlsVisible);
    }

    Grid grid = { 6, 4 };
    const Bounds titleBounds = grid.get(m_bounds, 1, 0, 5, 2);
    const Bounds toolbar = titleBounds.verticalSplit(0.0f, 0.24f);
    m_projectInfoButton->m_bounds = toolbar.horizontalSplit(0.61f, 0.74f);
    m_fullscreenButton->m_bounds = toolbar.horizontalSplit(0.75f, 1.0f);
    if (m_controlsVisible) {
        const Bounds windowBounds(
            static_cast<float>(m_app->getScreenWidth()),
            static_cast<float>(m_app->getScreenHeight()),
            { 0.0f, static_cast<float>(m_app->getScreenHeight()) });
        const float inset = std::max(20.0f, std::min(windowBounds.width(), windowBounds.height()) * 0.10f);
        const Bounds panel = windowBounds.inset(inset);
        m_mouseBounds = windowBounds;
        m_closeControlsButton->m_bounds = panel
            .verticalSplit(0.83f, 0.96f)
            .horizontalSplit(0.76f, 0.96f);
        const Bounds content = panel.inset(30.0f);
        const Bounds projectButtons = content
            .horizontalSplit(0.58f, 0.98f)
            .verticalSplit(0.02f, 0.08f);
        m_githubButton->m_bounds = projectButtons.horizontalSplit(0.0f, 0.47f);
        m_issuesButton->m_bounds = projectButtons.horizontalSplit(0.53f, 1.0f);
    }
    m_checkMouse = m_controlsVisible;
    UiElement::update(dt);
}

void InfoCluster::signal(UiElement *element, Event event) {
    if (element == m_fullscreenButton && event == Event::Clicked) {
        m_app->toggleFullscreen();
    }
    else if (element == m_projectInfoButton && event == Event::Clicked) {
        setControlsVisible(true);
    }
    else if (element == m_closeControlsButton && event == Event::Clicked) {
        setControlsVisible(false);
    }
    else if (element == m_githubButton && event == Event::Clicked) {
        m_app->getPlatform()->openUrl("https://github.com/carlesonielfa/open-engine-sim");
    }
    else if (element == m_issuesButton && event == Event::Clicked) {
        m_app->getPlatform()->openUrl("https://github.com/carlesonielfa/open-engine-sim/issues");
    }
}

void InfoCluster::onMouseClick(const Point &mouseLocal) {
    if (m_controlsVisible) setControlsVisible(false);
}

void InfoCluster::setControlsVisible(bool visible) {
    m_controlsVisible = visible;
    setRenderLayer(visible ? 0x100 : 0);
    m_projectInfoButton->setVisible(!visible);
    m_fullscreenButton->setVisible(!visible);
    m_closeControlsButton->setVisible(visible);
    m_githubButton->setVisible(visible);
    m_issuesButton->setVisible(visible);
}

void InfoCluster::render() {
    if (m_controlsVisible) {
        const Bounds windowBounds(
            static_cast<float>(m_app->getScreenWidth()),
            static_cast<float>(m_app->getScreenHeight()),
            { 0.0f, static_cast<float>(m_app->getScreenHeight()) });
        const float inset = std::max(20.0f, std::min(windowBounds.width(), windowBounds.height()) * 0.10f);
        const Bounds panel = windowBounds.inset(inset);
        const Bounds content = panel.inset(30.0f);
        const ysVector foreground = m_app->getForegroundColor();
        const ysVector background = m_app->getBackgroundColor();
        const ysVector secondary = mix(foreground, background, 0.45f);
        TextRenderer *textRenderer = m_app->getTextRenderer();

        const auto drawKey = [&](const std::string &key, Point &cursor, float y) {
            const float keyHeight = 28.0f;
            const float keyWidth = textRenderer->CalculateWidth(key, 14.0f) + 18.0f;
            const Bounds keyBounds(keyWidth, keyHeight, { cursor.x, y }, Bounds::bl);
            drawFrame(keyBounds, 1.0f, foreground, foreground, true, 0x20);
            textRenderer->SetColor(background);
            drawCenteredText(key, keyBounds, 14.0f, Bounds::center);
            textRenderer->SetColor(foreground);
            cursor.x += keyWidth + 7.0f;
        };
        const auto drawAction = [&](float fraction, const std::vector<std::string> &keys,
                                    const std::string &label, const std::string &hint) {
            const Bounds row = content.verticalSplit(fraction, fraction + 0.065f);
            Point cursor(content.left(), row.center_v() - 14.0f);
            for (const std::string &key : keys) drawKey(key, cursor, row.center_v() - 14.0f);

            const Bounds labelBounds(content.right() - cursor.x, 28.0f, cursor, Bounds::bl);
            drawAlignedText(label, labelBounds, 16.0f, Bounds::lm, Bounds::lm);
            cursor.x += textRenderer->CalculateWidth(label, 16.0f) + 18.0f;

            if (!hint.empty()) {
                textRenderer->SetColor(secondary);
                const Bounds hintBounds(content.right() - cursor.x, 28.0f, cursor, Bounds::bl);
                drawAlignedText(hint, hintBounds, 14.0f, Bounds::lm, Bounds::lm);
            }
            textRenderer->SetColor(foreground);
        };
        const auto drawShortcut = [&](const std::string &key, const std::string &label, Point &cursor, float y) {
            drawKey(key, cursor, y);
            const float labelWidth = textRenderer->CalculateWidth(label, 14.0f);
            drawAlignedText(label, Bounds(labelWidth, 28.0f, cursor, Bounds::bl), 14.0f, Bounds::lm, Bounds::lm);
            cursor.x += labelWidth + 22.0f;
        };

        drawFrame(panel, 2.0f, foreground, background, true, 0x1f);
        drawAlignedText("CONTROLS", content.verticalSplit(0.89f, 0.98f), 32.0f, Bounds::lm, Bounds::lm);
        drawAlignedText("START", content.verticalSplit(0.79f, 0.85f), 20.0f, Bounds::lm, Bounds::lm);
        drawAction(0.69f, { "A" }, "ENABLE IGNITION", "OR TAP IGNITION");
        drawAction(0.60f, { "S" }, "HOLD STARTER", "OR HOLD STARTER");
        drawAlignedText("DRIVE", content.verticalSplit(0.49f, 0.55f), 20.0f, Bounds::lm, Bounds::lm);
        drawAction(0.39f, { "Q", "W", "E", "R" }, "ADJUST THROTTLE", "OR DRAG THROTTLE");
        drawAction(0.30f, { "UP", "DOWN" }, "CHANGE GEAR", "OR TAP ARROWS");
        drawAction(0.21f, { "SPACE" }, "HOLD CLUTCH", "");
        drawAlignedText("EXTRAS", content.verticalSplit(0.11f, 0.17f), 20.0f, Bounds::lm, Bounds::lm);
        Point shortcutCursor(content.left(), content.verticalSplit(0.02f, 0.08f).center_v() - 14.0f);
        drawShortcut("D", "DYNO", shortcutCursor, shortcutCursor.y);
        drawShortcut("H", "HOLD", shortcutCursor, shortcutCursor.y);
        drawShortcut("F", "FULLSCREEN", shortcutCursor, shortcutCursor.y);
        drawShortcut("F1", "GUIDE", shortcutCursor, shortcutCursor.y);
        const Bounds projectArea = content.horizontalSplit(0.58f, 0.98f);
        drawAlignedText("PROJECT", projectArea.verticalSplit(0.22f, 0.28f), 20.0f, Bounds::lm, Bounds::lm);
        textRenderer->SetColor(secondary);
        drawAlignedText(
            "OPEN-SOURCE, CROSS-PLATFORM ENGINE SOUND SIMULATOR.",
            projectArea.verticalSplit(0.16f, 0.21f),
            14.0f,
            Bounds::lm,
            Bounds::lm);
        drawAlignedText(
            "HAVING TROUBLE? REPORT IT ON GITHUB.",
            projectArea.verticalSplit(0.10f, 0.15f),
            14.0f,
            Bounds::lm,
            Bounds::lm);
        textRenderer->SetColor(foreground);
        UiElement::render();
        return;
    }

    Grid grid;
    grid.h_cells = 6;
    grid.v_cells = 4;

    const Bounds logoBounds = grid.get(m_bounds, 0, 0, 1, 2);
    drawFrame(logoBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    // The original Blender scene supplies the logo in model-local space.
    // Place it inside the existing dashboard frame using the portable OBJ
    // loader, independent of Blender or Delta at runtime.
    resetShader();
    const Point logoPosition = getRenderPoint(logoBounds.getPosition(Bounds::center));
    m_app->getShaders()->SetObjectTransform(
        ysMath::MatMult(
            ysMath::TranslationTransform(ysMath::LoadVector(logoPosition.x, logoPosition.y, 0.0f)),
            ysMath::ScaleTransform(ysMath::LoadVector(
                logoBounds.width() * 0.58f, logoBounds.height() * 0.72f, 1.0f))));
    m_app->getShaders()->SetBaseColor(m_app->getForegroundColor());
    m_app->drawModel("Logo", m_app->getShaders()->GetUiFlags(), 0x12);

    const Bounds titleBounds = grid.get(m_bounds, 1, 0, 5, 2);
    drawFrame(titleBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    // Keep the title compact and reserve a distinct bottom strip for future
    // dashboard actions (currently fullscreen). The explicit proportions
    // avoid wasting a full grid row between the build metadata and controls.
    const Bounds titleTextBounds = titleBounds.verticalSplit(0.62f, 1.0f);
    const Bounds subtitleBounds = titleBounds.verticalSplit(0.44f, 0.62f);
    const Bounds buildBounds = titleBounds.verticalSplit(0.28f, 0.44f);
    const Bounds toolbarBounds = titleBounds.verticalSplit(0.0f, 0.24f);
    const auto fittedHeight = [this](const std::string &text, float requestedHeight) {
        // Reserve a meaningful right margin even when a fork name is longer
        // than the upstream heading that originally occupied this panel.
        const float maximumWidth = m_bounds.width() * 0.80f;
        const float requestedWidth = m_app->getTextRenderer()->CalculateWidth(text, requestedHeight);
        return requestedWidth > maximumWidth
            ? requestedHeight * maximumWidth / requestedWidth
            : requestedHeight;
    };
    drawAlignedText(
        "OPEN ENGINE SIMULATOR",
        titleTextBounds.inset(10.0f).move({ 0.0f, -21.0f }),
        fittedHeight("OPEN ENGINE SIMULATOR", 42.0f),
        Bounds::bl,
        Bounds::bl);
    drawAlignedText(
        "ORIGINAL BY ANGETHEGREAT",
        subtitleBounds.inset(10.0f).move({ 0.0f, 3.0f }),
        fittedHeight("ORIGINAL BY ANGETHEGREAT", 24.0f),
        Bounds::tl,
        Bounds::tl);
    drawAlignedText(
        "BUILD: v" + EngineSimApplication::getBuildVersion() + " // " __DATE__,
        buildBounds.inset(10.0f).move({ 0.0f, 4.0f }),
        16.0f,
        Bounds::tl,
        Bounds::tl);

    drawFrame(toolbarBounds, 1.0f,
        m_app->getForegroundColor(), m_app->getBackgroundColor());

    const Bounds engineInfoBounds = grid.get(m_bounds, 0, 2, 6, 1);
    drawFrame(engineInfoBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    drawAlignedText(
        (m_engine != nullptr) ? m_engine->getName() : "<NO ENGINE>",
        engineInfoBounds.inset(10.0f),
        24.0f,
        Bounds::lm,
        Bounds::lm);

    std::stringstream ss;
    if (m_engine != nullptr) {
        ss << std::fixed;

        if (m_engine->getDisplacement() < units::volume(1.0, units::L)) {
            ss << std::setprecision(0) << units::convert(m_engine->getDisplacement(), units::cc) << " cc -- ";
        }
        else {
            ss << std::setprecision(1) << units::convert(m_engine->getDisplacement(), units::L) << " L -- ";
        }

        ss << std::setprecision(0) << units::convert(m_engine->getDisplacement(), units::cubic_inches) << " CI";
    }
    else {
        ss << "N/A";
    }

    drawAlignedText(
        ss.str(),
        engineInfoBounds.inset(10.0f),
        24.0f,
        Bounds::rm,
        Bounds::rm);

    const Bounds infoMessagesBounds = grid.get(m_bounds, 0, 3, 6, 1);
    drawFrame(infoMessagesBounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    drawAlignedText(
        m_logMessage,
        infoMessagesBounds.inset(10.0f),
        24.0f,
        Bounds::lm,
        Bounds::lm);

    UiElement::render();
}
