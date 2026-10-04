#pragma once

namespace InputSettings
{
    inline constexpr float kDefaultCameraSensitivity = 1.0f;
    inline constexpr float kDefaultCameraDeadzone = 0.0f;
    inline constexpr bool kDefaultEnableCameraAutoReset = true;
    inline constexpr float kDefaultCameraResetDelaySeconds = 0.5f;

    inline float CameraSensitivity = kDefaultCameraSensitivity;
    inline float CameraDeadzone = kDefaultCameraDeadzone;
    inline bool EnableCameraAutoReset = kDefaultEnableCameraAutoReset;
    inline float CameraResetDelaySeconds = kDefaultCameraResetDelaySeconds;

    void Load();
    void Save();
    void Reset();
}