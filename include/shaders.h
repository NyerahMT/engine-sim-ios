#ifndef ATG_ENGINE_SIM_SHADERS_H
#define ATG_ENGINE_SIM_SHADERS_H

#include "render_math.h"
#include "ui_math.h"

#include <cstdint>

// GPU-API-free renderer state consumed by the SDL GPU backend at submission.
class Shaders {
public:
    using StageEnableFlags = std::uint32_t;
    static constexpr StageEnableFlags SceneStage = 1u << 0;
    static constexpr StageEnableFlags UiStage = 1u << 1;

    struct ScreenVariables {
        ysMatrix Projection = ysMath::LoadIdentity();
        ysMatrix CameraView = ysMath::LoadIdentity();
        ysVector Eye = {};
        float FogNear = 16000.0f;
        float FogFar = 16001.0f;
    };

    struct ObjectVariables {
        ysMatrix Transform = ysMath::LoadIdentity();
        ysVector BaseColor = ysMath::Constants::One;
        int ColorReplace = 1;
        int Lit = 0;
    };

    Shaders();
    ~Shaders();

    void SetObjectTransform(const ysMatrix &matrix);
    void SetBaseColor(const ysVector &color);
    void ResetBaseColor();

    StageEnableFlags GetRegularFlags() const;
    StageEnableFlags GetUiFlags() const;

    void CalculateCamera(
        float width,
        float height,
        const Bounds &cameraBounds,
        float screenWidth,
        float screenHeight,
        float angle = 0.0f);
    void CalculateUiCamera(float screenWidth, float screenHeight);

    void SetClearColor(const ysVector &color);
    const ysVector &GetClearColor() const;

    ScreenVariables m_screenVariables;
    ScreenVariables m_uiScreenVariables;
    ObjectVariables m_objectVariables;
    ysVector m_cameraPosition;

private:
    ysVector m_clearColor;
};

#endif /* ATG_ENGINE_SIM_SHADERS_H */
