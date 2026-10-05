#pragma once

#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

#include "head_pose.h"

struct tobii_api_t;
struct tobii_device_t;

// Wraps the Tobii Stream Engine SDK's head-pose stream (Tobii Eye Tracker 5, head pose only
// -- no gaze/eye data is ever read from the SDK). Not unit-testable without hardware.
//
// All Stream Engine calls (connect, device enumeration, callback pumping, reconnect) run on a
// private worker thread, because several of them block for a noticeable time and must never
// stall the game's main thread. Latest() is the only cross-thread access and is mutex-guarded.
class TobiiSource {
public:
    TobiiSource() = default;
    ~TobiiSource();

    TobiiSource(const TobiiSource&) = delete;
    TobiiSource& operator=(const TobiiSource&) = delete;

    // Starts the worker thread, which connects to the first available tracker and keeps
    // retrying on a low-frequency timer until one is found or the connection recovers.
    // Calling Start() while already running does nothing.
    void Start();

    // Stops the worker thread and releases the Tobii API. Safe to call repeatedly.
    void Stop();

    // Returns the most recent mapped head pose, or std::nullopt if no valid sample has been
    // received yet or the tracker is currently reporting invalid data.
    std::optional<HeadPose> Latest() const;

private:
    friend struct TobiiSourceCallbacks;

    void Run(std::stop_token stop);
    bool Connect();
    void Disconnect();
    void Publish(std::optional<HeadPose> pose);
    void ReportFailure(const char* step, int error);

    std::jthread worker_;
    tobii_api_t* api_ = nullptr;
    tobii_device_t* device_ = nullptr;
    std::string lastFailure_;
    bool poseValid_ = false;

    mutable std::mutex latestMutex_;
    std::optional<HeadPose> latest_;
};
