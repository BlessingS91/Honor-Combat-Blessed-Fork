#include "Menu.h"

#include <algorithm>
#include <string>

#include "Configuration.h"
#include "InputSettings.h"
#include "Prisma.h"
namespace
{
    bool RenderIntSliderWithInput(const char* label, int* value, int minValue, int maxValue)
    {
        bool changed = false;

        changed |= ImGui::SliderInt(label, value, minValue, maxValue);

        ImGui::SameLine();

        ImGui::SetNextItemWidth(80.0f);

        std::string inputId = std::string("##") + label;

        if (ImGui::InputInt(inputId.c_str(), value, 1, 1))
        {
            changed = true;
        }

        *value = std::clamp(*value, minValue, maxValue);

        return changed;
    }
}

namespace ModMenu
{
    void TrackerRender();

    void PlayerUIRender()

    {
        auto& ui = Settings::PlayerUI;

        bool changed = false;

        if (ImGui::CollapsingHeader(GetLoc("menu.general", "General"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            changed |= ImGui::Checkbox(GetLoc("menu.enabled", "Enable player HUD"), &ui.enabled);
            changed |= ImGui::Checkbox(GetLoc("menu.edit_mode", "UI edit mode (keep HUD visible)"), &ui.editMode);
            changed |= ImGui::Checkbox(GetLoc("menu.block_reset", "Block reset (SBF_BlockStart selects center)"), &ui.blockReset);
            changed |= ImGui::Checkbox(GetLoc("menu.use_directional_dmk", "Use DMK directional movement instead of camera"), &ui.useDirectionalDMK);
            changed |= ImGui::Checkbox(GetLoc("menu.require_tdm_target_lock", "Show HUD only while a TDM target is locked"), &ui.requireTDMTargetLock);
            changed |= ImGui::Checkbox(GetLoc("menu.show_center", "Show center piece"), &ui.showCenter);
            changed |= ImGui::Checkbox(GetLoc("menu.move_center", "Move center piece with direction"), &ui.moveCenterWithDirection);
            changed |= ImGui::Checkbox(GetLoc("menu.center_third_person", "Use center-screen position in third person"), &ui.centerInThirdPerson);
            changed |= ImGui::Checkbox(GetLoc("menu.resolution_scale", "Scale dynamically with screen resolution"), &ui.scaleWithResolution);

            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader(GetLoc("menu.layout", "Position and size"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            ImGui::TextColored({0.6f, 0.8f, 1.0f, 1.0f}, "%s", GetLoc("menu.center_position", "First-person / center-screen position"));

            changed |= RenderIntSliderWithInput(GetLoc("menu.position_x", "Center horizontal position (%)"), &ui.positionXPercent, 0, 100);

            changed |= RenderIntSliderWithInput(GetLoc("menu.position_y", "Center vertical position (%)"), &ui.positionYPercent, 0, 100);

            ImGui::Separator();

            ImGui::TextColored({0.6f, 0.8f, 1.0f, 1.0f}, "%s", GetLoc("menu.torso_attachment", "Third-person torso attachment"));

            changed |= RenderIntSliderWithInput(GetLoc("menu.attach_offset_x", "Torso horizontal offset (pixels)"), &ui.attachOffsetXPixels, -1000, 1000);

            changed |= RenderIntSliderWithInput(GetLoc("menu.attach_offset_y", "Torso vertical offset (negative moves down)"), &ui.attachOffsetYPixels, -1000, 1000);

            changed |= RenderIntSliderWithInput(GetLoc("menu.scale", "HUD scale (%)"), &ui.scalePercent, 25, 300);

            changed |= RenderIntSliderWithInput(GetLoc("menu.diameter", "Base diameter (pixels)"), &ui.diameterPixels, 96, 800);

            changed |= RenderIntSliderWithInput(GetLoc("menu.inner_diameter", "Center opening (%)"), &ui.innerDiameterPercent, 10, 80);

            changed |= RenderIntSliderWithInput(GetLoc("menu.center_piece_size", "Center piece size (%)"), &ui.centerPieceSizePercent, 10, 200);

            changed |= RenderIntSliderWithInput(GetLoc("menu.segment_gap", "Segment gap (pixels)"), &ui.segmentGapPixels, 0, 30);

            changed |= RenderIntSliderWithInput(GetLoc("menu.rotation", "Ring rotation (degrees)"), &ui.rotationDegrees, -180, 180);

            changed |= RenderIntSliderWithInput(GetLoc("menu.top_angle", "Top segment angle (degrees)"), &ui.topAngleDegrees, 30, 150);

            changed |= RenderIntSliderWithInput(GetLoc("menu.opacity", "HUD opacity (%)"), &ui.opacityPercent, 10, 100);

            ImGui::Separator();

            ImGui::TextColored({0.6f, 0.8f, 1.0f, 1.0f}, "%s", GetLoc("menu.center_movement", "Center movement"));

            changed |= RenderIntSliderWithInput(GetLoc("menu.center_movement_pixels", "Maximum center movement (pixels)"), &ui.centerMovementPixels, 0, 400);

            changed |= RenderIntSliderWithInput(GetLoc("menu.center_movement_duration", "Center movement duration (ms, 0 = instant)"), &ui.centerMovementDurationMs, 0, 1000);

            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader(GetLoc("menu.colors", "Colors"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            constexpr ImGuiColorEditFlags colorFlags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf;

            changed |= ImGui::ColorEdit4(GetLoc("menu.active_color", "Active direction"), ui.activeColor.data(), colorFlags);

            changed |= ImGui::ColorEdit4(GetLoc("menu.inactive_color", "Inactive directions"), ui.inactiveColor.data(), colorFlags);

            changed |= ImGui::ColorEdit4(GetLoc("menu.border_color", "Segment border"), ui.borderColor.data(), colorFlags);

            changed |= ImGui::ColorEdit4(GetLoc("menu.center_color", "Center piece"), ui.centerColor.data(), colorFlags);

            ImGui::Unindent();
        }

        if (ImGui::Button(GetLoc("menu.reset_player", "Reset player UI settings")))
        {
            ui = Settings::PlayerUISettings{};
            changed = true;
        }

        if (changed)
        {
            SaveSettings();
            Prisma::ApplyUISettings();
        }
    }

    void NPCUIRender()
    {
        auto& npc = Settings::NPCUI;

        bool changed = false;

        if (ImGui::CollapsingHeader(GetLoc("menu.npc_warnings", "NPC attack warnings"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            changed |= ImGui::Checkbox(GetLoc("menu.npc_warnings_enabled", "Show attack warnings on NPCs targeting the player"), &npc.enabled);

            changed |= ImGui::Checkbox(GetLoc("menu.npc_warnings_always_in_combat", "Always show NPC indicators while they target the player in combat"), &npc.alwaysShowInCombat);

            changed |= ImGui::Checkbox(GetLoc("menu.npc_warnings_scale_with_distance", "Reduce NPC indicator size with camera distance"), &npc.scaleWithDistance);

            changed |= ImGui::Checkbox(GetLoc("menu.npc_resolution_scale", "Scale NPC indicators with screen resolution"), &npc.scaleWithResolution);

            changed |= ImGui::Checkbox(GetLoc("menu.npc_warnings_mirror", "Mirror NPC attack directions"), &npc.mirrorAttackDirections);

            changed |= ImGui::SliderFloat(GetLoc("menu.npc_top_threshold", "NPC top attack threshold"), &npc.topThreshold, 0.0f, 50.0f, "%.0f");

            ImGui::TextWrapped("%s", GetLoc("menu.npc_warnings_mirror_help",
                                            "Mirroring swaps left and right for a front-facing view. "
                                            "Leave it disabled when the direction already matches what you see in game."));

            ImGui::Separator();

            changed |= RenderIntSliderWithInput(GetLoc("menu.npc_warnings_scale", "NPC warning scale (%)"), &npc.scalePercent, 20, 200);

            changed |= RenderIntSliderWithInput(GetLoc("menu.npc_warnings_opacity", "NPC warning opacity (%)"), &npc.opacityPercent, 10, 100);

            changed |= RenderIntSliderWithInput(GetLoc("menu.npc_warnings_offset_x", "NPC warning horizontal offset (pixels)"), &npc.offsetXPixels, -1000, 1000);

            changed |= RenderIntSliderWithInput(GetLoc("menu.npc_warnings_offset_y", "NPC warning vertical offset (positive moves up)"), &npc.offsetYPixels, -1000, 1000);

            changed |= RenderIntSliderWithInput(GetLoc("menu.npc_warnings_rotation", "NPC warning rotation (degrees)"), &npc.rotationDegrees, -180, 180);

            changed |= RenderIntSliderWithInput(GetLoc("menu.npc_top_angle", "NPC top segment angle (degrees)"), &npc.topAngleDegrees, 30, 150);

            changed |= RenderIntSliderWithInput(GetLoc("menu.npc_diameter", "NPC base diameter (pixels)"), &npc.diameterPixels, 96, 800);

            changed |= RenderIntSliderWithInput(GetLoc("menu.npc_inner_diameter", "NPC center opening (%)"), &npc.innerDiameterPercent, 10, 80);

            changed |= RenderIntSliderWithInput(GetLoc("menu.npc_segment_gap", "NPC segment gap (pixels)"), &npc.segmentGapPixels, 0, 30);

            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader(GetLoc("menu.colors", "Colors"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            constexpr ImGuiColorEditFlags colorFlags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf;

            changed |= ImGui::ColorEdit4(GetLoc("menu.active_color", "Active direction"), npc.activeColor.data(), colorFlags);

            changed |= ImGui::ColorEdit4(GetLoc("menu.inactive_color", "Inactive directions"), npc.inactiveColor.data(), colorFlags);

            changed |= ImGui::ColorEdit4(GetLoc("menu.border_color", "Segment border"), npc.borderColor.data(), colorFlags);

            ImGui::Unindent();
        }

        ImGui::Separator();

        changed |= ImGui::Checkbox(GetLoc("menu.npc_only_combat", "Track NPC attacks only while in combat"), &NPCOnlyCombat);

        if (ImGui::Button(GetLoc("menu.reset_npc", "Reset NPC UI settings")))
        {
            npc = Settings::NPCUISettings{};
            changed = true;
        }

        if (changed)
        {
            SaveSettings();
            Prisma::ApplyUISettings();
        }
    }

    void Register()
    {
        LoadLanguage();
        LoadSettings();

        if (!SKSEMenuFramework::IsInstalled())
        {
            logger::warn("SKSE Menu Framework not found; Honor Combat will use saved/default UI settings.");
            return;
        }

        SKSEMenuFramework::SetSection("Honor Combat");

        SKSEMenuFramework::AddSectionItem(GetLoc("menu.player_ui_settings", "Player UI"), PlayerUIRender);

        SKSEMenuFramework::AddSectionItem(GetLoc("menu.npc_ui_settings", "NPC UI"), NPCUIRender);

        SKSEMenuFramework::AddSectionItem(GetLoc("menu.tracker_settings", "Input"), TrackerRender);
    }

    void TrackerRender()
    {
        bool changed = false;

        if (ImGui::CollapsingHeader("Input Tracking", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            changed |= ImGui::SliderFloat("Camera sensitivity", &InputSettings::CameraSensitivity, 0.1f, 5.0f, "%.2f");

            changed |= ImGui::SliderFloat("Camera deadzone", &InputSettings::CameraDeadzone, 0.0f, 100.0f, "%.1f");

            changed |= ImGui::Checkbox("Enable camera auto reset", &InputSettings::EnableCameraAutoReset);

            changed |= ImGui::SliderFloat("Camera reset delay (seconds)", &InputSettings::CameraResetDelaySeconds, 0.0f, 5.0f, "%.2f");

            ImGui::Unindent();
        }

        if (ImGui::Button("Reset input settings"))
        {
            InputSettings::Reset();
            changed = true;
        }

        if (changed)
        {
            SaveSettings();
        }
    }
}