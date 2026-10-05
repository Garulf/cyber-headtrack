#include "engine_angles.h"

namespace {

double Signed(double value, bool inverted) {
    return inverted ? -value : value;
}

}  // namespace

EngineAngles ToEngineAngles(const Rotation& offset, const AxisInversion& invert) {
    EngineAngles angles;
    angles.yaw = Signed(-offset.yaw, invert.yaw);
    angles.pitch = Signed(offset.pitch, invert.pitch);
    angles.roll = Signed(-offset.roll, invert.roll);
    return angles;
}
