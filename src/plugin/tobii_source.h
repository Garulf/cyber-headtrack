#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

#include "gaze.h"
#include "head_pose.h"

struct tobii_api_t;
struct tobii_device_t;

// Wraps the Tobii Stream Engine SDK's head-pose and gaze-point streams (Tobii Eye Tracker 5).
// The gaze point only drives the scanner reticle in scan mode; when the tracker doesn't offer
// it, head pose works on its own. Not unit-testable without hardware.
//
// All Stream Engine calls (connect, device enumeration, callback pumping, reconnect) run on a
// private worker thread, because several of them block for a noticeable time and must never
// stall the game's main thread. Latest() and LatestGaze() are the only cross-thread access and
// are mutex-guarded.
class TobiiSource {
public:
    TobiiSource() = default;
    ~TobiiSource();

    TobiiSource(const TobiiSource&) = delete;
    TobiiSource& operator=(const TobiiSource&) = delete;

    // Starts the worker thread, which connects to the first available tracker and keeps
    // retrying on a low-frequency timer until one is found or the connection recovers. When
    // Stream Engine enumerates no tracker, fallbackUrl (if not empty) is opened instead.
    // Calling Start() while already running does nothing.
    void Start(std::string fallbackUrl);

    // Stops the worker thread and releases the Tobii API. Safe to call repeatedly.
    void Stop();

    // Returns the most recent mapped head pose, or std::nullopt if no valid sample has been
    // received yet or the tracker is currently reporting invalid data.
    std::optional<HeadPose> Latest() const;

    // Returns the most recent valid gaze point, or std::nullopt if there is none or it is
    // older than a few frames (the tracker stopped sending, e.g. the eyes left its view).
    std::optional<ScreenPoint> LatestGaze() const;

private:
    friend struct TobiiSourceCallbacks;

    void Run(std::stop_token stop);
    bool Connect();
    void Disconnect();
    void Publish(std::optional<HeadPose> pose);
    void PublishGaze(std::optional<ScreenPoint> point);
    void SubscribeGaze();
    void ReportSilentGaze();
    void ReportFailure(const std::string& step, int error);
    void ReportProblem(const std::string& problem);

    std::jthread worker_;
    tobii_api_t* api_ = nullptr;
    tobii_device_t* device_ = nullptr;
    std::string fallbackUrl_;
    std::string lastFailure_;
    bool poseValid_ = false;
    bool gazeSubscribed_ = false;
    bool gazeValid_ = false;
    bool gazeEverValid_ = false;
    bool gazeSilenceReported_ = false;
    std::chrono::steady_clock::time_point connectedAt_;

    mutable std::mutex latestMutex_;
    std::optional<HeadPose> latest_;
    std::optional<ScreenPoint> latestGaze_;
    std::chrono::steady_clock::time_point latestGazeAt_;
};
