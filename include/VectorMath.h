#pragma once
/**
 * @file VectorMath.h
 * @brief Shared vector math utilities for motion detection
 */

#include <k4a/k4a.h>
#include <cmath>

namespace kinect {
namespace math {

constexpr float EPSILON = 1e-5f;

inline float magnitudeSquared(const k4a_float3_t& v) {
    return v.xyz.x * v.xyz.x + v.xyz.y * v.xyz.y + v.xyz.z * v.xyz.z;
}

inline float magnitude(const k4a_float3_t& v) {
    return std::sqrt(magnitudeSquared(v));
}

inline k4a_float3_t normalize(const k4a_float3_t& v) {
    float magSq = magnitudeSquared(v);
    if (magSq < EPSILON * EPSILON) {
        return {0.0f, 0.0f, 0.0f};
    }
    float invMag = 1.0f / std::sqrt(magSq);
    return {v.xyz.x * invMag, v.xyz.y * invMag, v.xyz.z * invMag};
}

inline k4a_float3_t subtract(const k4a_float3_t& a, const k4a_float3_t& b) {
    return {a.xyz.x - b.xyz.x, a.xyz.y - b.xyz.y, a.xyz.z - b.xyz.z};
}

inline float dot(const k4a_float3_t& a, const k4a_float3_t& b) {
    return a.xyz.x * b.xyz.x + a.xyz.y * b.xyz.y + a.xyz.z * b.xyz.z;
}

inline float angleBetween(const k4a_float3_t& a, const k4a_float3_t& b) {
    k4a_float3_t normA = normalize(a);
    k4a_float3_t normB = normalize(b);
    float d = dot(normA, normB);
    d = std::max(-1.0f, std::min(1.0f, d));
    return std::acos(d) * (180.0f / 3.14159265359f);
}

} // namespace math
} // namespace kinect
