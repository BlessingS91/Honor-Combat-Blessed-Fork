#pragma once

#include <array>
#include <string>

#include "SKSEMCP/SKSEMenuFramework.hpp"

namespace Settings
{

    inline constexpr std::size_t kSegmentCount = 3;

    struct PlayerUISettings
    {
        bool enabled = true;
        bool editMode = false;
        bool blockReset = false;
        bool useDirectionalDMK = false;
        bool requireTDMTargetLock = false;
        bool showCenter = true;
        bool moveCenterWithDirection = false;
        bool centerInThirdPerson = false;
        bool scaleWithResolution = true;

        int positionXPercent = 50;
        int positionYPercent = 50;
        int attachOffsetXPixels = 0;
        int attachOffsetYPixels = 0;
        int scalePercent = 100;
        int diameterPixels = 260;
        int innerDiameterPercent = 36;
        int centerPieceSizePercent = 100;
        int segmentGapPixels = 4;
        int topAngleDegrees = 90;
        int rotationDegrees = 0;
        int opacityPercent = 100;
        int centerMovementPixels = 20;
        int centerMovementDurationMs = 100;

        std::array<float, 4> activeColor{0.95f, 0.72f, 0.22f, 1.0f};
        std::array<float, 4> inactiveColor{0.08f, 0.09f, 0.10f, 0.72f};
        std::array<float, 4> borderColor{0.80f, 0.80f, 0.76f, 0.90f};
        std::array<float, 4> centerColor{0.04f, 0.04f, 0.05f, 0.86f};

        std::string sharedSegmentImage;
        std::array<std::string, kSegmentCount> segmentImages{};
        std::string centerImage;
    };

    struct NPCUISettings
    {
        bool enabled = true;
        bool alwaysShowInCombat = false;
        bool scaleWithResolution = true;
        bool scaleWithDistance = false;
        bool mirrorAttackDirections = false;

        int scalePercent = 52;
        int opacityPercent = 100;
        int offsetXPixels = 0;
        int offsetYPixels = 0;
        int rotationDegrees = 0;
        int diameterPixels = 260;
        int innerDiameterPercent = 36;
        int segmentGapPixels = 4;
        int topAngleDegrees = 90;

        std::array<float, 4> activeColor{0.95f, 0.72f, 0.22f, 1.0f};
        std::array<float, 4> inactiveColor{0.08f, 0.09f, 0.10f, 0.72f};
        std::array<float, 4> borderColor{0.80f, 0.80f, 0.76f, 0.90f};

        std::string sharedSegmentImage;
        std::array<std::string, kSegmentCount> segmentImages{};
    };

    inline PlayerUISettings PlayerUI;
    inline NPCUISettings NPCUI;
}

namespace ModMenu
{

    inline bool NPCOnlyCombat = true;

    void Register();

    void PlayerUIRender();

    void NPCUIRender();

    void LoadSettings();

    void SaveSettings();

    void LoadLanguage();

    const char* GetLoc(const std::string& key, const char* defaultValue);

}