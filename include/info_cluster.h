#ifndef ATG_ENGINE_SIM_INFO_CLUSTER_H
#define ATG_ENGINE_SIM_INFO_CLUSTER_H

#include "ui_element.h"
#include "ui_button.h"

#include "engine.h"

#include <string>

class InfoCluster : public UiElement {
public:
    InfoCluster();
    virtual ~InfoCluster();

    virtual void initialize(EngineSimApplication *app);
    virtual void destroy();

    virtual void update(float dt);
    virtual void render();
    virtual void signal(UiElement *element, Event event);
    virtual void onMouseClick(const Point &mouseLocal) override;

    void setEngine(Engine *engine) { m_engine = engine; }
    void setLogMessage(const std::string &logMessage) { m_logMessage = logMessage; }
    std::string getLogMessage() const { return m_logMessage; }
    bool controlsVisible() const { return m_controlsVisible; }

protected:
    Engine *m_engine;
    UiButton *m_projectInfoButton;
    UiButton *m_fullscreenButton;
    UiButton *m_closeControlsButton;
    UiButton *m_githubButton;
    UiButton *m_issuesButton;
    bool m_controlsVisible;

    std::string m_logMessage;

    void setControlsVisible(bool visible);
};

#endif /* ATG_ENGINE_SIM_INFO_CLUSTER_H */
