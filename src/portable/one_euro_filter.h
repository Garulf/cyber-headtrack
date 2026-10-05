#pragma once

// One Euro filter (Casiez, Roussel, Vogel, CHI 2012): a low-pass filter whose cutoff rises
// with the signal's speed, so it smooths jitter when the input is still and follows closely
// when it moves fast. One instance filters one scalar signal.
//
// minCutoffHz sets smoothing at rest (lower = steadier, more lag on slow motion); beta sets
// how quickly the cutoff opens up with speed, per unit of input change per second.
class OneEuroFilter {
public:
    OneEuroFilter(double minCutoffHz, double beta);

    // Returns the filtered value for a new sample taken dtSeconds after the previous one. The
    // first sample after construction passes through unchanged; a non-positive dtSeconds
    // returns the previous output.
    double Filter(double value, double dtSeconds);

    // Continues as if the signal had been resting at value, so later samples ease away from
    // it instead of jumping.
    void Reset(double value);

private:
    double minCutoffHz_;
    double beta_;
    bool initialized_ = false;
    double previousValue_ = 0.0;
    double previousDerivative_ = 0.0;
};
