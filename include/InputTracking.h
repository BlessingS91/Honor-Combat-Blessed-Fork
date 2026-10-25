#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

class DirectionTracker final : public RE::BSTEventSink<RE::InputEvent*>
{
public:
    static DirectionTracker* GetSingleton();

    void Initialize();
    void Shutdown();

    void ApplyAndDispatchState(int a_cameraState);
    void SyncExternalGraphState(RE::PlayerCharacter* a_player);

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

private:
    DirectionTracker() = default;
    ~DirectionTracker() = default;

    DirectionTracker(const DirectionTracker&) = delete;
    DirectionTracker(DirectionTracker&&) = delete;
    DirectionTracker& operator=(const DirectionTracker&) = delete;
    DirectionTracker& operator=(DirectionTracker&&) = delete;

    void UpdateDirection();
    void ResetCameraInput();

    void NotifyCameraActivity();
    void ScheduleCameraResetCheck(std::chrono::milliseconds a_delay, std::uint64_t a_generation);
    void HandleCameraResetCheck(std::uint64_t a_generation);
    void CancelCameraResetTimer();

    void DispatchUpdate(int a_value);

    bool IsRightStickInput(const RE::ThumbstickEvent* a_event) const;

    float mouseCamX{0.0f};
    float mouseCamY{0.0f};

    bool m_up{false};
    bool m_down{false};
    bool m_left{false};
    bool m_right{false};

    bool rs_up{false};
    bool rs_down{false};
    bool rs_left{false};
    bool rs_right{false};

    std::atomic_bool initialized{false};
    std::atomic_bool resetScheduled{false};

    std::mutex stateMutex{};

    std::chrono::steady_clock::time_point cameraResetDeadline{};

    std::uint64_t cameraResetGeneration{0};

    int lastWrittenCameraState{-1};
    int lastDispatchedCameraState{-1};
};