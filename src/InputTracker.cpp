#include "InputTracker.h"

#include <algorithm>
#include <cmath>

#include "Configuration.h"
#include "logger.h"

namespace
{
    constexpr char kCameraDirectionVariable[] = "HonorFightCameraDirection";

    constexpr int kNeutral = 0;
    constexpr int kTop = 1;
    constexpr int kRight = 2;
    constexpr int kLeft = 4;
}

InputTracker* InputTracker::GetSingleton()
{
    static InputTracker singleton;
    return &singleton;
}

void InputTracker::Initialize()
{
    if (registered_)
    {
        return;
    }

    auto* inputManager = RE::BSInputDeviceManager::GetSingleton();
    if (!inputManager)
    {
        logger::warn("[HonorFight] InputDeviceManager unavailable.");
        return;
    }

    inputManager->AddEventSink(this);
    registered_ = true;

    logger::info("[HonorFight] InputTracker initialized.");
}

void InputTracker::Reset()
{
    mouseCamX_ = 0.0f;
    mouseCamY_ = 0.0f;

    if (cameraDirection_ != kNeutral)
    {
        cameraDirection_ = kNeutral;

        if (auto* player = RE::PlayerCharacter::GetSingleton())
        {
            player->SetGraphVariableInt(kCameraDirectionVariable, kNeutral);
        }
    }
}

RE::BSEventNotifyControl InputTracker::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*)
{
    if (!a_event)
    {
        return RE::BSEventNotifyControl::kContinue;
    }

    for (auto* event = *a_event; event; event = event->next)
    {
        if (event->eventType != RE::INPUT_EVENT_TYPE::kMouseMove)
        {
            continue;
        }

        auto* mouseEvent = event->As<RE::InputEvent::MouseMove>();
        if (!mouseEvent)
        {
            continue;
        }

        mouseCamX_ += static_cast<float>(mouseEvent->mouseInputX);
        mouseCamY_ += static_cast<float>(mouseEvent->mouseInputY);

        mouseCamX_ = std::clamp(mouseCamX_, -300.0f, 300.0f);
        mouseCamY_ = std::clamp(mouseCamY_, -300.0f, 300.0f);

        UpdateCameraDirection();
    }

    return RE::BSEventNotifyControl::kContinue;
}

void InputTracker::UpdateCameraDirection()
{
    constexpr float kMinimumDistance = 20.0f;

    const float x = mouseCamX_;
    const float y = mouseCamY_;

    const float distance = std::sqrt((x * x) + (y * y));

    int newDirection = kNeutral;

    if (distance >= kMinimumDistance)
    {
        if (std::abs(y) >= std::abs(x))
        {
            newDirection = kTop;
        }
        else if (x > 0.0f)
        {
            newDirection = kRight;
        }
        else
        {
            newDirection = kLeft;
        }
    }

    if (newDirection == cameraDirection_)
    {
        return;
    }

    cameraDirection_ = newDirection;

    if (auto* player = RE::PlayerCharacter::GetSingleton())
    {
        player->SetGraphVariableInt(kCameraDirectionVariable, cameraDirection_);
    }
}