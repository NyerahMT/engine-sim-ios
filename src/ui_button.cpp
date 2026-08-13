#include "../include/ui_button.h"

#include "../include/engine_sim_application.h"
#include "../include/ui_utilities.h"

UiButton::UiButton() {
    m_text = "";
    m_fontSize = 12;
    m_inverted = false;
    m_drawFrame = true;
    m_layer = 0x11;
    m_checkMouse = true;
}

UiButton::~UiButton() {
    /* void */
}

void UiButton::update(float dt) {
    m_mouseBounds = m_bounds;
}

void UiButton::render() {
    const ysVector foreground = m_app->getForegroundColor();
    const ysVector background = m_app->getBackgroundColor();
    ysVector color = m_inverted ? foreground : background;
    if (isMouseHeld()) {
        color = mix(color, m_inverted ? background : foreground, 0.08f);
    }
    else if (isMouseOver()) {
        color = mix(color, m_inverted ? background : foreground, 0.04f);
    }

    if (m_drawFrame) {
        drawFrame(m_bounds, 1.0, m_inverted ? background : foreground, color, true, m_layer);
    }
    else {
        drawBox(m_bounds, color, m_layer);
    }
    m_app->getTextRenderer()->SetColor(m_inverted ? background : foreground);
    drawCenteredText(m_text, m_bounds, m_fontSize, Bounds::center);
    m_app->getTextRenderer()->SetColor(foreground);
}
