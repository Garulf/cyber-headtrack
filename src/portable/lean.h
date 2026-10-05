#pragma once

// Positional head lean relative to the recentered neutral pose, in centimeters, from the
// player's point of view: positive side = right, up = up, forward = toward the screen.
struct Lean {
    double sideCm = 0.0;
    double upCm = 0.0;
    double forwardCm = 0.0;
};

struct LeanInversion {
    bool side = false;
    bool up = false;
    bool forward = false;
};
