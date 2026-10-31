#include "aim_math.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr Vec3 kForward{0.0, 1.0, 0.0};

double Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double Length(const Vec3& v) {
    return std::sqrt(Dot(v, v));
}

Vec3 Normalize(const Vec3& v) {
    const double length = Length(v);
    return {v.x / length, v.y / length, v.z / length};
}

}  // namespace

Quat Multiply(const Quat& a, const Quat& b) {
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

Quat Conjugate(const Quat& q) {
    return {-q.x, -q.y, -q.z, q.w};
}

Vec3 Rotate(const Quat& q, const Vec3& v) {
    const Quat p{v.x, v.y, v.z, 0.0};
    const Quat r = Multiply(Multiply(q, p), Conjugate(q));
    return {r.x, r.y, r.z};
}

Quat FromBasis(const Vec3& right, const Vec3& forward, const Vec3& up) {
    const double m00 = right.x, m01 = forward.x, m02 = up.x;
    const double m10 = right.y, m11 = forward.y, m12 = up.y;
    const double m20 = right.z, m21 = forward.z, m22 = up.z;
    const double trace = m00 + m11 + m22;
    Quat q;
    if (trace > 0.0) {
        const double s = std::sqrt(trace + 1.0) * 2.0;
        q = {(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25 * s};
    } else if (m00 > m11 && m00 > m22) {
        const double s = std::sqrt(1.0 + m00 - m11 - m22) * 2.0;
        q = {0.25 * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s};
    } else if (m11 > m22) {
        const double s = std::sqrt(1.0 + m11 - m00 - m22) * 2.0;
        q = {(m01 + m10) / s, 0.25 * s, (m12 + m21) / s, (m02 - m20) / s};
    } else {
        const double s = std::sqrt(1.0 + m22 - m00 - m11) * 2.0;
        q = {(m02 + m20) / s, (m12 + m21) / s, 0.25 * s, (m10 - m01) / s};
    }
    return q;
}

Vec3 CleanAimDirection(const Quat& cameraWorld, const Quat& headLocal, const Vec3& shot) {
    const Quat correction =
        Multiply(Multiply(cameraWorld, Conjugate(headLocal)), Conjugate(cameraWorld));
    return Rotate(correction, shot);
}

bool IsNearForward(const Quat& cameraWorld, const Vec3& direction, double maxDegrees) {
    const double length = Length(direction);
    if (length < 1e-9) {
        return false;
    }
    const double cosine = Dot(Rotate(cameraWorld, kForward), direction) / length;
    return cosine >= std::cos(maxDegrees * std::numbers::pi / 180.0);
}

double AngleDegrees(const Vec3& a, const Vec3& b) {
    const double cosine = Dot(a, b) / (Length(a) * Length(b));
    return std::acos(std::clamp(cosine, -1.0, 1.0)) * 180.0 / std::numbers::pi;
}

Vec3 ScreenToCameraDirection(ScreenPoint point, double ndcPerTanX, double ndcPerTanY) {
    const double ndcX = 2.0 * point.x - 1.0;
    const double ndcY = 1.0 - 2.0 * point.y;
    return Normalize({ndcX / ndcPerTanX, 1.0, ndcY / ndcPerTanY});
}
