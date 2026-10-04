#include "InputSettings.h"

namespace InputSettings
{
    void Reset()
    {
        CameraSensitivity = kDefaultCameraSensitivity;
        CameraDeadzone = kDefaultCameraDeadzone;
        EnableCameraAutoReset = kDefaultEnableCameraAutoReset;
        CameraResetDelaySeconds = kDefaultCameraResetDelaySeconds;
    }
}