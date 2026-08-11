#include "../include/shaders.h"

#include <cmath>

namespace {

ysMatrix orthographicProjection(float width, float height, float nearPlane, float farPlane) {
    ysMatrix result = {};
    result.m[0][0] = 2.0f / width;
    result.m[1][1] = 2.0f / height;
    result.m[2][2] = 1.0f / (nearPlane - farPlane);
    result.m[2][3] = nearPlane / (nearPlane - farPlane);
    result.m[3][3] = 1.0f;
    return result;
}

ysMatrix cameraTarget(const ysVector &eye, const ysVector &target, const ysVector &up) {
    const ysVector forward = ysMath::Normalize(ysMath::Sub(target, eye));
    const ysVector right = ysMath::Normalize(ysMath::Cross(forward, up));
    const ysVector correctedUp = ysMath::Cross(right, forward);

    ysMatrix result = ysMath::LoadIdentity();
    result.m[0][0] = right.x;
    result.m[0][1] = right.y;
    result.m[0][2] = right.z;
    result.m[0][3] = -ysMath::GetScalar(ysMath::Dot(right, eye));
    result.m[1][0] = correctedUp.x;
    result.m[1][1] = correctedUp.y;
    result.m[1][2] = correctedUp.z;
    result.m[1][3] = -ysMath::GetScalar(ysMath::Dot(correctedUp, eye));
    result.m[2][0] = -forward.x;
    result.m[2][1] = -forward.y;
    result.m[2][2] = -forward.z;
    result.m[2][3] = ysMath::GetScalar(ysMath::Dot(forward, eye));
    return result;
}

} // namespace

Shaders::Shaders()
    : m_cameraPosition(ysMath::LoadVector(0.0f, 0.0f, 0.0f, 1.0f)),
      m_clearColor(ysColor::srgbiToLinear(0x34, 0x98, 0xdb)) { }

Shaders::~Shaders() = default;

void Shaders::SetObjectTransform(const ysMatrix &matrix) { m_objectVariables.Transform = matrix; }
void Shaders::SetBaseColor(const ysVector &color) { m_objectVariables.BaseColor = color; }
void Shaders::ResetBaseColor() { m_objectVariables.BaseColor = ysMath::Constants::One; }
Shaders::StageEnableFlags Shaders::GetRegularFlags() const { return SceneStage; }
Shaders::StageEnableFlags Shaders::GetUiFlags() const { return UiStage; }

void Shaders::CalculateCamera(
    float width,
    float height,
    const Bounds &,
    float,
    float,
    float angle)
{
    m_screenVariables.Projection = ysMath::Transpose(
        orthographicProjection(width, height, 0.001f, 500.0f));
    const ysVector eye = ysMath::Add(
        ysMath::LoadVector(10.0f * std::sin(angle), 0.0f, 10.0f * std::cos(angle), 1.0f),
        m_cameraPosition);
    m_screenVariables.CameraView = ysMath::Transpose(cameraTarget(
        eye, m_cameraPosition, ysMath::Constants::YAxis));
    m_screenVariables.Eye = eye;
}

void Shaders::CalculateUiCamera(float screenWidth, float screenHeight) {
    m_uiScreenVariables.Projection = ysMath::Transpose(
        orthographicProjection(screenWidth, screenHeight, 0.001f, 500.0f));
    const ysVector eye = ysMath::LoadVector(0.0f, 0.0f, 10.0f, 1.0f);
    m_uiScreenVariables.CameraView = ysMath::Transpose(cameraTarget(
        eye, ysMath::LoadVector(0.0f, 0.0f, 0.0f, 1.0f), ysMath::Constants::YAxis));
    m_uiScreenVariables.Eye = eye;
}

void Shaders::SetClearColor(const ysVector &color) { m_clearColor = color; }
const ysVector &Shaders::GetClearColor() const { return m_clearColor; }
