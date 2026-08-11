#ifndef ATG_ENGINE_SIM_RENDER_MATH_H
#define ATG_ENGINE_SIM_RENDER_MATH_H

#include <cmath>

// Transitional local math vocabulary. The names mirror the former geometry
// API so that mesh generation can move independently from renderer behavior.
// Unlike the old types, these declarations belong to this project and have no
// graphics backend dependency.
struct ysVector {
    float x, y, z, w;

    constexpr ysVector(float x_ = 0.0f, float y_ = 0.0f, float z_ = 0.0f, float w_ = 0.0f)
        : x(x_), y(y_), z(z_), w(w_) { }

    void Set(float x_, float y_, float z_, float w_) { x = x_; y = y_; z = z_; w = w_; }
};

struct ysVector2 {
    float x, y;
    constexpr ysVector2(float x_ = 0.0f, float y_ = 0.0f) : x(x_), y(y_) { }
};

struct ysMatrix {
    float m[4][4] = {};
};

struct EngineSimVertex {
    ysVector Pos;
    ysVector Normal;
    ysVector2 TexCoord;
};

namespace ysMath {
namespace Constants {
inline constexpr float PI = 3.14159265358979323846f;
inline constexpr float TWO_PI = 2.0f * PI;
inline constexpr ysVector XAxis = { 1.0f, 0.0f, 0.0f, 0.0f };
inline constexpr ysVector YAxis = { 0.0f, 1.0f, 0.0f, 0.0f };
inline constexpr ysVector ZAxis = { 0.0f, 0.0f, 1.0f, 0.0f };
inline constexpr ysVector One = { 1.0f, 1.0f, 1.0f, 1.0f };
}

inline ysVector LoadVector(float x, float y, float z = 0.0f, float w = 1.0f) {
    return { x, y, z, w };
}
inline ysVector GetVector4(const ysVector &v) { return v; }
inline ysVector LoadScalar(float value) { return { value, value, value, value }; }
inline ysVector ExtendVector(const ysVector &v) { return { v.x, v.y, v.z, 1.0f }; }
inline ysVector Add(const ysVector &a, const ysVector &b) { return { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w }; }
inline ysVector Sub(const ysVector &a, const ysVector &b) { return { a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; }
inline ysVector Mul(const ysVector &a, const ysVector &b) { return { a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w }; }
inline ysVector Cross(const ysVector &a, const ysVector &b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x, 0.0f };
}
inline ysVector Dot(const ysVector &a, const ysVector &b) { return { a.x * b.x + a.y * b.y + a.z * b.z, 0.0f, 0.0f, 0.0f }; }
inline float GetScalar(const ysVector &v) { return v.x; }
inline ysVector Magnitude(const ysVector &v) { return { std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z), 0.0f, 0.0f, 0.0f }; }
inline ysVector Normalize(const ysVector &v) {
    const float length = GetScalar(Magnitude(v));
    return length == 0.0f ? ysVector{} : ysVector{ v.x / length, v.y / length, v.z / length, v.w };
}
inline ysMatrix LoadMatrix(const ysVector &c0, const ysVector &c1, const ysVector &c2, const ysVector &c3) {
    ysMatrix result = {};
    const ysVector columns[] = { c0, c1, c2, c3 };
    for (int column = 0; column < 4; ++column) {
        result.m[0][column] = columns[column].x;
        result.m[1][column] = columns[column].y;
        result.m[2][column] = columns[column].z;
        result.m[3][column] = columns[column].w;
    }
    return result;
}
inline ysMatrix Transpose(const ysMatrix &matrix) {
    ysMatrix result = {};
    for (int row = 0; row < 4; ++row) for (int column = 0; column < 4; ++column) result.m[row][column] = matrix.m[column][row];
    return result;
}
inline ysMatrix LoadIdentity() {
    ysMatrix result = {};
    for (int i = 0; i < 4; ++i) result.m[i][i] = 1.0f;
    return result;
}
inline ysMatrix TranslationTransform(const ysVector &translation) {
    ysMatrix result = LoadIdentity();
    result.m[0][3] = translation.x;
    result.m[1][3] = translation.y;
    result.m[2][3] = translation.z;
    return result;
}
inline ysMatrix ScaleTransform(const ysVector &scale) {
    ysMatrix result = {};
    result.m[0][0] = scale.x;
    result.m[1][1] = scale.y;
    result.m[2][2] = scale.z;
    result.m[3][3] = 1.0f;
    return result;
}
inline ysMatrix RotationTransform(const ysVector &axis, float radians) {
    const ysVector normalized = Normalize(axis);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    const float t = 1.0f - c;
    ysMatrix result = LoadIdentity();
    result.m[0][0] = c + normalized.x * normalized.x * t;
    result.m[0][1] = normalized.x * normalized.y * t - normalized.z * s;
    result.m[0][2] = normalized.x * normalized.z * t + normalized.y * s;
    result.m[1][0] = normalized.y * normalized.x * t + normalized.z * s;
    result.m[1][1] = c + normalized.y * normalized.y * t;
    result.m[1][2] = normalized.y * normalized.z * t - normalized.x * s;
    result.m[2][0] = normalized.z * normalized.x * t - normalized.y * s;
    result.m[2][1] = normalized.z * normalized.y * t + normalized.x * s;
    result.m[2][2] = c + normalized.z * normalized.z * t;
    return result;
}
inline ysMatrix MatMult(const ysMatrix &a, const ysMatrix &b) {
    ysMatrix result = {};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            for (int i = 0; i < 4; ++i) result.m[row][column] += a.m[row][i] * b.m[i][column];
        }
    }
    return result;
}
inline ysVector MatMult(const ysMatrix &matrix, const ysVector &v) {
    return {
        matrix.m[0][0] * v.x + matrix.m[0][1] * v.y + matrix.m[0][2] * v.z + matrix.m[0][3] * v.w,
        matrix.m[1][0] * v.x + matrix.m[1][1] * v.y + matrix.m[1][2] * v.z + matrix.m[1][3] * v.w,
        matrix.m[2][0] * v.x + matrix.m[2][1] * v.y + matrix.m[2][2] * v.z + matrix.m[2][3] * v.w,
        matrix.m[3][0] * v.x + matrix.m[3][1] * v.y + matrix.m[3][2] * v.z + matrix.m[3][3] * v.w
    };
}
}

namespace ysColor {
inline ysVector srgbiToLinear(unsigned int rgb) {
    const float r = static_cast<float>((rgb >> 16) & 0xff) / 255.0f;
    const float g = static_cast<float>((rgb >> 8) & 0xff) / 255.0f;
    const float b = static_cast<float>(rgb & 0xff) / 255.0f;
    return { r, g, b, 1.0f };
}
inline ysVector srgbiToLinear(unsigned char r, unsigned char g, unsigned char b) {
    return { r / 255.0f, g / 255.0f, b / 255.0f, 1.0f };
}
}

#endif /* ATG_ENGINE_SIM_RENDER_MATH_H */
