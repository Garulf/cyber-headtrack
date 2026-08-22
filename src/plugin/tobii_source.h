#pragma once

#include <optional>

#include "head_pose.h"

struct tobii_api_t;
struct tobii_device_t;

// Wraps the Tobii Stream Engine SDK's head-pose stream (Tobii Eye Tracker 5, head pose only
// -- no gaze/eye data is ever read from the SDK). Not unit-testable without hardware.
//
// Threading contract: Start(), Stop(), Poll(), and Latest() must all be called from the
// same thread. The Stream Engine invokes the head-pose callback synchronously inside
// tobii_device_process_callbacks (i.e. inside Poll()), so no locking is needed.
class TobiiSource {
public:
    TobiiSource() = default;
    ~TobiiSource();

    TobiiSource(const TobiiSource&) = delete;
    TobiiSource& operator=(const TobiiSource&) = delete;

    // Initializes the Tobii API, finds the first available device, and subscribes to head
    // pose. Returns false if no device was found or the API failed to initialize; does not
    // throw. Safe to call again after a failed Start() once a tracker becomes available.
    bool Start();

    // Unsubscribes and releases the Tobii API. Safe to call even if Start() was never
    // called or already failed.
    void Stop();

    // Must be called periodically (e.g. once per game frame) to pump Tobii's callback queue.
    void Poll();

    // Returns the most recent mapped head pose, or std::nullopt if no valid sample has been
    // received yet or the tracker is currently reporting invalid data.
    std::optional<HeadPose> Latest() const;

private:
    friend struct TobiiSourceCallbacks;

    tobii_api_t* api_ = nullptr;
    tobii_device_t* device_ = nullptr;
    bool subscribed_ = false;
    std::optional<HeadPose> latest_;
};
