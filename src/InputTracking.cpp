#include "InputTracking.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "DelayedDispatcher.h"
#include "InputSettings.h"

namespace
{
    constexpr float kMouseClamp = 300.0f;
    constexpr float kStickThreshold = 0.5f;
    constexpr float kStickActivityThreshold = 0.1f;

    constexpr std::string_view kCameraGraphVariable = "CameraMovementCMF";
    constexpr std::string_view kUpdateEvent = "DMKUpdate";
    constexpr std::string_view kCameraEventType = "Camera";
}

DirectionTracker* DirectionTracker::GetSingleton()
{
    static DirectionTracker singleton;
    return std::addressof(singleton);
}

void DirectionTracker::Initialize()
{
    if (initialized.exchange(true))
    {
        return;
    }

    auto* inputDispatcher = RE::BSInputDeviceManager::GetSingleton();

    if (inputDispatcher)
    {
        inputDispatcher->AddEventSink(this);
    }

    logger::info("DirectionTracker initialized");
}

void DirectionTracker::Shutdown()
{
    if (!initialized.exchange(false))
    {
        return;
    }

    CancelCameraResetTimer();

    auto* inputDispatcher = RE::BSInputDeviceManager::GetSingleton();

    if (inputDispatcher)
    {
        inputDispatcher->RemoveEventSink(this);
    }

    ResetCameraInput();

    logger::info("DirectionTracker shutdown");
}

RE::BSEventNotifyControl DirectionTracker::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*)
{
    if (!initialized || !a_event || !*a_event)
    {
        return RE::BSEventNotifyControl::kContinue;
    }

    bool stateChanged = false;

    for (auto* event = *a_event; event; event = event->next)
    {
        if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kMouseMove)
        {
            auto* mouseEvent = static_cast<RE::MouseMoveEvent*>(event);

            const float deadzone = InputSettings::CameraDeadzone;

            mouseCamX += mouseEvent->mouseInputX * InputSettings::CameraSensitivity;
            mouseCamY -= mouseEvent->mouseInputY * InputSettings::CameraSensitivity;

            mouseCamX = std::clamp(mouseCamX, -kMouseClamp, kMouseClamp);
            mouseCamY = std::clamp(mouseCamY, -kMouseClamp, kMouseClamp);

            const bool newUp = mouseCamY > deadzone;
            const bool newDown = mouseCamY < -deadzone;
            const bool newLeft = mouseCamX < -deadzone;
            const bool newRight = mouseCamX > deadzone;

            if (newUp != m_up || newDown != m_down || newLeft != m_left || newRight != m_right)
            {
                m_up = newUp;
                m_down = newDown;
                m_left = newLeft;
                m_right = newRight;

                stateChanged = true;
            }

            if (mouseEvent->mouseInputX != 0 || mouseEvent->mouseInputY != 0)
            {
                NotifyCameraActivity();
            }
        }
        else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick)
        {
            auto* stickEvent = static_cast<RE::ThumbstickEvent*>(event);

            if (!IsRightStickInput(stickEvent))
            {
                continue;
            }

            const float x = stickEvent->xValue;
            const float y = stickEvent->yValue;

            const bool newUp = y > kStickThreshold;
            const bool newDown = y < -kStickThreshold;
            const bool newLeft = x < -kStickThreshold;
            const bool newRight = x > kStickThreshold;

            if (std::abs(x) > kStickActivityThreshold || std::abs(y) > kStickActivityThreshold)
            {
                mouseCamX = 0.0f;
                mouseCamY = 0.0f;

                if (m_up || m_down || m_left || m_right)
                {
                    m_up = false;
                    m_down = false;
                    m_left = false;
                    m_right = false;

                    stateChanged = true;
                }

                NotifyCameraActivity();
            }

            if (newUp != rs_up || newDown != rs_down || newLeft != rs_left || newRight != rs_right)
            {
                rs_up = newUp;
                rs_down = newDown;
                rs_left = newLeft;
                rs_right = newRight;

                stateChanged = true;
            }
        }
    }

    if (stateChanged)
    {
        UpdateDirection();
    }

    return RE::BSEventNotifyControl::kContinue;
}

bool DirectionTracker::IsRightStickInput(const RE::ThumbstickEvent* a_event) const
{
    if (!a_event)
    {
        return false;
    }

    return a_event->IsRight();
}

void DirectionTracker::UpdateDirection()
{
    const bool CAM_FRENTE = rs_up || m_up;
    const bool CAM_TRAS = rs_down || m_down;
    const bool CAM_ESQUERDA = rs_left || m_left;
    const bool CAM_DIREITA = rs_right || m_right;

    int cameraMovementCMF = 0;

    if (CAM_FRENTE && CAM_ESQUERDA)
        cameraMovementCMF = 8;
    else if (CAM_FRENTE && CAM_DIREITA)
        cameraMovementCMF = 2;
    else if (CAM_TRAS && CAM_ESQUERDA)
        cameraMovementCMF = 6;
    else if (CAM_TRAS && CAM_DIREITA)
        cameraMovementCMF = 4;
    else if (CAM_FRENTE)
        cameraMovementCMF = 1;
    else if (CAM_ESQUERDA)
        cameraMovementCMF = 7;
    else if (CAM_TRAS)
        cameraMovementCMF = 5;
    else if (CAM_DIREITA)
        cameraMovementCMF = 3;

    auto* player = RE::PlayerCharacter::GetSingleton();

    if (!player)
    {
        return;
    }

    player->SetGraphVariableInt(kCameraGraphVariable, cameraMovementCMF);

    DispatchUpdate(cameraMovementCMF);
}

void DirectionTracker::DispatchUpdate(int a_value)
{
    if (lastDispatchedCameraState == a_value)
    {
        return;
    }

    auto* eventSource = SKSE::GetModCallbackEventSource();

    if (!eventSource)
    {
        logger::error("[DMKUpdate] Event source unavailable. type=Camera value={}", a_value);
        return;
    }

    SKSE::ModCallbackEvent event{RE::BSFixedString(kUpdateEvent), RE::BSFixedString(kCameraEventType), static_cast<float>(a_value), nullptr};

    eventSource->SendEvent(&event);

    lastDispatchedCameraState = a_value;

    logger::debug("[DMKUpdate] Sent type=Camera value={}", a_value);
}

void DirectionTracker::NotifyCameraActivity()
{
    if (!InputSettings::EnableCameraAutoReset)
    {
        return;
    }

    const auto resetDelay = std::chrono::milliseconds(static_cast<int64_t>(InputSettings::CameraResetDelaySeconds * 1000.0f));

    cameraResetDeadline = std::chrono::steady_clock::now() + resetDelay;

    if (!resetScheduled)
    {
        resetScheduled = true;
        ScheduleCameraResetCheck(resetDelay, cameraResetGeneration);
    }
}

void DirectionTracker::ScheduleCameraResetCheck(std::chrono::milliseconds a_delay, std::uint64_t a_generation)
{
    Utils::DelayedDispatcher::Get().PostDelayed(a_delay,
                                                [a_generation]()
                                                {
                                                    if (auto* taskInterface = SKSE::GetTaskInterface())
                                                    {
                                                        taskInterface->AddTask([a_generation]() { DirectionTracker::GetSingleton()->HandleCameraResetCheck(a_generation); });
                                                    }
                                                });
}

void DirectionTracker::HandleCameraResetCheck(std::uint64_t a_generation)
{
    if (a_generation != cameraResetGeneration)
    {
        return;
    }

    const auto now = std::chrono::steady_clock::now();

    if (now < cameraResetDeadline)
    {
        const auto remaining = std::max(std::chrono::duration_cast<std::chrono::milliseconds>(cameraResetDeadline - now), std::chrono::milliseconds(1));

        ScheduleCameraResetCheck(remaining, a_generation);

        return;
    }

    resetScheduled = false;
    ++cameraResetGeneration;

    ResetCameraInput();
    UpdateDirection();
}

void DirectionTracker::CancelCameraResetTimer()
{
    ++cameraResetGeneration;
    resetScheduled = false;
}

void DirectionTracker::ResetCameraInput()
{
    std::scoped_lock lock(stateMutex);

    mouseCamX = 0.0f;
    mouseCamY = 0.0f;

    m_up = false;
    m_down = false;
    m_left = false;
    m_right = false;

    rs_up = false;
    rs_down = false;
    rs_left = false;
    rs_right = false;
}