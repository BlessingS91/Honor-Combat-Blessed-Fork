#include "Configuration.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "Prisma.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

namespace
{

    constexpr const char* kModDirectory = "Data/Viny Mods/Honor Combat";
    constexpr const char* kLegacyUISettingsPath = "Data/Viny Mods/Honor Combat/UISettings.json";
    constexpr const char* kPlayerUISettingsPath = "Data/Viny Mods/Honor Combat/PlayerUISettings.json";
    constexpr const char* kNPCUISettingsPath = "Data/Viny Mods/Honor Combat/NPCUISettings.json";
    constexpr const char* kInputTrackingSettingsPath = "Data/Viny Mods/Honor Combat/InputTrackingSettings.json";
    constexpr const char* kLanguagePath = "Data/Viny Mods/Honor Combat/Language.json";
    constexpr std::size_t kMaxAssetPathLength = 260;

    std::unordered_map<std::string, std::string> language;

    void ClampColor(std::array<float, 4>& color)
    {
        for (auto& value : color)
        {
            value = std::clamp(value, 0.0f, 1.0f);
        }
    }

    std::string NormalizeAssetPath(std::string path)
    {
        std::replace(path.begin(), path.end(), '\\', '/');

        if (path.size() > kMaxAssetPathLength || path.find("..") != std::string::npos || path.find(':') != std::string::npos || path.starts_with('/') || path.starts_with("//"))
        {
            return {};
        }

        return path;
    }

    void ClampSettings(Settings::PlayerUISettings& ui)
    {
        ui.positionXPercent = std::clamp(ui.positionXPercent, 0, 100);
        ui.positionYPercent = std::clamp(ui.positionYPercent, 0, 100);
        ui.attachOffsetXPixels = std::clamp(ui.attachOffsetXPixels, -1000, 1000);
        ui.attachOffsetYPixels = std::clamp(ui.attachOffsetYPixels, -1000, 1000);
        ui.scalePercent = std::clamp(ui.scalePercent, 25, 300);
        ui.diameterPixels = std::clamp(ui.diameterPixels, 96, 800);
        ui.innerDiameterPercent = std::clamp(ui.innerDiameterPercent, 10, 80);
        ui.centerPieceSizePercent = std::clamp(ui.centerPieceSizePercent, 10, 200);
        ui.segmentGapPixels = std::clamp(ui.segmentGapPixels, 0, 30);
        ui.topAngleDegrees = std::clamp(ui.topAngleDegrees, 30, 150);
        ui.rotationDegrees = std::clamp(ui.rotationDegrees, -180, 180);
        ui.opacityPercent = std::clamp(ui.opacityPercent, 10, 100);
        ui.centerMovementPixels = std::clamp(ui.centerMovementPixels, 0, 400);
        ui.centerMovementDurationMs = std::clamp(ui.centerMovementDurationMs, 0, 1000);

        ClampColor(ui.activeColor);
        ClampColor(ui.unblockableColor);
        ClampColor(ui.inactiveColor);
        ClampColor(ui.borderColor);
        ClampColor(ui.centerColor);

        ui.sharedSegmentImage = NormalizeAssetPath(std::move(ui.sharedSegmentImage));

        for (auto& path : ui.segmentImages)
        {
            path = NormalizeAssetPath(std::move(path));
        }

        ui.centerImage = NormalizeAssetPath(std::move(ui.centerImage));
    }

    void ClampSettings(Settings::NPCUISettings& ui)
    {
        ui.scalePercent = std::clamp(ui.scalePercent, 20, 200);
        ui.opacityPercent = std::clamp(ui.opacityPercent, 10, 100);
        ui.offsetXPixels = std::clamp(ui.offsetXPixels, -1000, 1000);
        ui.offsetYPixels = std::clamp(ui.offsetYPixels, -1000, 1000);
        ui.rotationDegrees = std::clamp(ui.rotationDegrees, -180, 180);
        ui.diameterPixels = std::clamp(ui.diameterPixels, 96, 800);
        ui.innerDiameterPercent = std::clamp(ui.innerDiameterPercent, 10, 80);
        ui.segmentGapPixels = std::clamp(ui.segmentGapPixels, 0, 30);
        ui.topAngleDegrees = std::clamp(ui.topAngleDegrees, 30, 150);

        ui.topThreshold = std::clamp(ui.topThreshold, 0.0f, 50.0f);

        ClampColor(ui.activeColor);
        ClampColor(ui.unblockableColor);
        ClampColor(ui.inactiveColor);
        ClampColor(ui.borderColor);

        ui.sharedSegmentImage = NormalizeAssetPath(std::move(ui.sharedSegmentImage));

        for (auto& path : ui.segmentImages)
        {
            path = NormalizeAssetPath(std::move(path));
        }
    }

    bool RenderIntSliderWithInput(const char* label, int* value, int minimum, int maximum)
    {
        bool changed = false;

        ImGui::PushID(label);

        ImGui::SetNextItemWidth(200.0f);
        changed |= ImGui::SliderInt("##slider", value, minimum, maximum);

        ImGui::SameLine();

        ImGui::SetNextItemWidth(200.0f);
        changed |= ImGui::InputInt(label, value);

        const int clamped = std::clamp(*value, minimum, maximum);

        if (*value != clamped)
        {
            *value = clamped;
            changed = true;
        }

        ImGui::PopID();

        return changed;
    }

    bool ReadDocument(const char* path, rapidjson::Document& document)
    {
        std::ifstream file(path, std::ios::binary);

        if (!file.is_open())
        {
            return false;
        }

        std::stringstream contents;
        contents << file.rdbuf();

        auto json = contents.str();

        if (json.size() >= 3 && static_cast<unsigned char>(json[0]) == 0xEF && static_cast<unsigned char>(json[1]) == 0xBB && static_cast<unsigned char>(json[2]) == 0xBF)
        {
            json.erase(0, 3);
        }

        document.Parse(json.c_str());

        return !document.HasParseError() && document.IsObject();
    }

    void ReadColor(const rapidjson::Value& parent, const char* key, std::array<float, 4>& color)
    {
        if (!parent.HasMember(key) || !parent[key].IsArray() || parent[key].Size() < 3)
        {
            return;
        }

        const auto& array = parent[key];

        for (rapidjson::SizeType i = 0; i < 4 && i < array.Size(); ++i)
        {
            if (array[i].IsNumber())
            {
                color[i] = array[i].GetFloat();
            }
        }
    }

    void WriteColor(rapidjson::Value& parent, const char* key, const std::array<float, 4>& color, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);

        for (const float value : color)
        {
            array.PushBack(value, allocator);
        }

        parent.AddMember(rapidjson::Value(key, allocator).Move(), array, allocator);
    }

}

namespace ModMenu
{
    void LoadLanguage()
    {
        language.clear();

        std::ifstream file(kLanguagePath, std::ios::binary);

        if (!file.is_open())
        {
            return;
        }

        std::stringstream contents;
        contents << file.rdbuf();

        rapidjson::Document document;
        document.Parse(contents.str().c_str());

        if (document.HasParseError() || !document.IsObject())
        {
            logger::warn("Honor Combat language file could not be parsed.");
            return;
        }

        for (auto member = document.MemberBegin(); member != document.MemberEnd(); ++member)
        {
            if (member->value.IsString())
            {
                language[member->name.GetString()] = member->value.GetString();
            }
            else if (member->value.IsObject())
            {
                const std::string category = member->name.GetString();

                for (auto nested = member->value.MemberBegin(); nested != member->value.MemberEnd(); ++nested)
                {
                    if (nested->value.IsString())
                    {
                        language[category + "." + nested->name.GetString()] = nested->value.GetString();
                    }
                }
            }
        }
    }

    const char* GetLoc(const std::string& key, const char* defaultValue)
    {
        const auto found = language.find(key);

        return found == language.end() ? defaultValue : found->second.c_str();
    }

    void LoadSettings()
    {
        rapidjson::Document document;

        const bool playerLoaded = ReadDocument(kPlayerUISettingsPath, document);

        const bool legacyLoaded = !playerLoaded && ReadDocument(kLegacyUISettingsPath, document);

        auto& ui = Settings::PlayerUI;

        if (playerLoaded || legacyLoaded)
        {
            const auto readBool = [&](const char* key, bool& value)
            {
                if (document.HasMember(key) && document[key].IsBool())
                {
                    value = document[key].GetBool();
                }
            };

            const auto readInt = [&](const char* key, int& value)
            {
                if (document.HasMember(key) && document[key].IsInt())
                {
                    value = document[key].GetInt();
                }
            };

            const auto readString = [&](const rapidjson::Value& parent, const char* key, std::string& value)
            {
                if (parent.HasMember(key) && parent[key].IsString())
                {
                    value = parent[key].GetString();
                }
            };

            readBool("enabled", ui.enabled);
            readBool("editMode", ui.editMode);
            readBool("blockReset", ui.blockReset);
            readBool("useDirectionalDMK", ui.useDirectionalDMK);
            readBool("requireTDMTargetLock", ui.requireTDMTargetLock);
            readBool("showOnlyWhileBlocking", ui.showOnlyWhileBlocking);
            readBool("showCenter", ui.showCenter);
            readBool("moveCenterWithDirection", ui.moveCenterWithDirection);
            readBool("centerInThirdPerson", ui.centerInThirdPerson);
            readBool("scaleWithResolution", ui.scaleWithResolution);
            readBool("npcOnlyCombat", NPCOnlyCombat);

            readInt("positionXPercent", ui.positionXPercent);
            readInt("positionYPercent", ui.positionYPercent);
            readInt("attachOffsetXPixels", ui.attachOffsetXPixels);
            readInt("attachOffsetYPixels", ui.attachOffsetYPixels);
            readInt("scalePercent", ui.scalePercent);
            readInt("diameterPixels", ui.diameterPixels);
            readInt("innerDiameterPercent", ui.innerDiameterPercent);
            readInt("centerPieceSizePercent", ui.centerPieceSizePercent);
            readInt("segmentGapPixels", ui.segmentGapPixels);
            readInt("topAngleDegrees", ui.topAngleDegrees);
            readInt("rotationDegrees", ui.rotationDegrees);
            readInt("opacityPercent", ui.opacityPercent);
            readInt("centerMovementPixels", ui.centerMovementPixels);
            readInt("centerMovementDurationMs", ui.centerMovementDurationMs);

            ReadColor(document, "activeColor", ui.activeColor);
            ReadColor(document, "unblockableColor", ui.unblockableColor);
            ReadColor(document, "inactiveColor", ui.inactiveColor);
            ReadColor(document, "borderColor", ui.borderColor);
            ReadColor(document, "centerColor", ui.centerColor);

            readString(document, "sharedSegmentImage", ui.sharedSegmentImage);

            readString(document, "centerImage", ui.centerImage);

            if (document.HasMember("segmentImages") && document["segmentImages"].IsArray())
            {
                const auto& images = document["segmentImages"];

                for (rapidjson::SizeType i = 0; i < images.Size() && i < Settings::kSegmentCount; ++i)
                {
                    if (images[i].IsString())
                    {
                        ui.segmentImages[i] = images[i].GetString();
                    }
                }
            }

            if (!document.HasMember("layoutVersion") || !document["layoutVersion"].IsInt() || document["layoutVersion"].GetInt() < 2)
            {
                ui.positionXPercent = 50;
                ui.positionYPercent = 50;
            }
        }

        ClampSettings(ui);

        rapidjson::Document npcDocument;

        const bool npcLoaded = ReadDocument(kNPCUISettingsPath, npcDocument);

        const bool legacyNpcLoaded = !npcLoaded && ReadDocument(kLegacyUISettingsPath, npcDocument);

        auto& npc = Settings::NPCUI;

        if (npcLoaded || legacyNpcLoaded)
        {
            const auto readBool = [&](const char* key, bool& value)
            {
                if (npcDocument.HasMember(key) && npcDocument[key].IsBool())
                {
                    value = npcDocument[key].GetBool();
                }
            };

            const auto readInt = [&](const char* key, int& value)
            {
                if (npcDocument.HasMember(key) && npcDocument[key].IsInt())
                {
                    value = npcDocument[key].GetInt();
                }
            };

            const auto readString = [&](const char* key, std::string& value)
            {
                if (npcDocument.HasMember(key) && npcDocument[key].IsString())
                {
                    value = npcDocument[key].GetString();
                }
            };

            const auto readFloat = [&](const char* key, float& value)
            {
                if (npcDocument.HasMember(key) && npcDocument[key].IsNumber())
                {
                    value = npcDocument[key].GetFloat();
                }
            };

            if (legacyNpcLoaded)
            {
                readBool("showNpcAttackWarnings", npc.enabled);

                readBool("alwaysShowNpcWarningsInCombat", npc.alwaysShowInCombat);

                readBool("scaleNpcWarningsWithDistance", npc.scaleWithDistance);

                readBool("mirrorNpcAttackDirections", npc.mirrorAttackDirections);

                readBool("scaleWithResolution", npc.scaleWithResolution);

                readInt("npcWarningScalePercent", npc.scalePercent);

                readInt("npcWarningOpacityPercent", npc.opacityPercent);

                readInt("npcWarningOffsetXPixels", npc.offsetXPixels);

                readInt("npcWarningOffsetYPixels", npc.offsetYPixels);

                readInt("npcWarningRotationDegrees", npc.rotationDegrees);
            }
            else
            {
                readBool("enabled", npc.enabled);
                readBool("alwaysShowInCombat", npc.alwaysShowInCombat);
                readBool("scaleWithDistance", npc.scaleWithDistance);
                readBool("mirrorAttackDirections", npc.mirrorAttackDirections);
                readBool("scaleWithResolution", npc.scaleWithResolution);

                readFloat("topThreshold", npc.topThreshold);

                readInt("scalePercent", npc.scalePercent);
                readInt("opacityPercent", npc.opacityPercent);
                readInt("offsetXPixels", npc.offsetXPixels);
                readInt("offsetYPixels", npc.offsetYPixels);
                readInt("rotationDegrees", npc.rotationDegrees);
            }

            readInt("diameterPixels", npc.diameterPixels);
            readInt("innerDiameterPercent", npc.innerDiameterPercent);
            readInt("segmentGapPixels", npc.segmentGapPixels);
            readInt("topAngleDegrees", npc.topAngleDegrees);

            ReadColor(npcDocument, "activeColor", npc.activeColor);

            ReadColor(npcDocument, "unblockableColor", npc.unblockableColor);

            ReadColor(npcDocument, "inactiveColor", npc.inactiveColor);

            ReadColor(npcDocument, "borderColor", npc.borderColor);

            readString("sharedSegmentImage", npc.sharedSegmentImage);

            if (npcDocument.HasMember("segmentImages") && npcDocument["segmentImages"].IsArray())
            {
                const auto& images = npcDocument["segmentImages"];

                for (rapidjson::SizeType i = 0; i < images.Size() && i < Settings::kSegmentCount; ++i)
                {
                    if (images[i].IsString())
                    {
                        npc.segmentImages[i] = images[i].GetString();
                    }
                }
            }
        }

        ClampSettings(npc);

        rapidjson::Document inputDocument;

        if (ReadDocument(kInputTrackingSettingsPath, inputDocument))
        {
            auto& input = Settings::InputTracking;

            if (inputDocument.HasMember("directionSwitchDelayMs") && inputDocument["directionSwitchDelayMs"].IsInt())
            {
                input.directionSwitchDelayMs = inputDocument["directionSwitchDelayMs"].GetInt();
            }

            if (inputDocument.HasMember("neutralDecayMs") && inputDocument["neutralDecayMs"].IsInt())
            {
                input.neutralDecayMs = inputDocument["neutralDecayMs"].GetInt();
            }
        }

        Settings::InputTracking.directionSwitchDelayMs = std::clamp(Settings::InputTracking.directionSwitchDelayMs, 0, 1000);

        Settings::InputTracking.neutralDecayMs = std::clamp(Settings::InputTracking.neutralDecayMs, 0, 5000);

        if (legacyLoaded || legacyNpcLoaded)
        {
            SaveSettings();
        }
    }

    void SaveSettings()
    {
        ClampSettings(Settings::PlayerUI);
        ClampSettings(Settings::NPCUI);

        std::filesystem::create_directories(kModDirectory);

        rapidjson::Document document;
        document.SetObject();

        auto& allocator = document.GetAllocator();

        const auto& ui = Settings::PlayerUI;

        document.AddMember("layoutVersion", 2, allocator);

        document.AddMember("enabled", ui.enabled, allocator);

        document.AddMember("editMode", ui.editMode, allocator);

        document.AddMember("blockReset", ui.blockReset, allocator);

        document.AddMember("useDirectionalDMK", ui.useDirectionalDMK, allocator);

        document.AddMember("requireTDMTargetLock", ui.requireTDMTargetLock, allocator);

        document.AddMember("showOnlyWhileBlocking", ui.showOnlyWhileBlocking, allocator);

        document.AddMember("showCenter", ui.showCenter, allocator);

        document.AddMember("moveCenterWithDirection", ui.moveCenterWithDirection, allocator);

        document.AddMember("centerInThirdPerson", ui.centerInThirdPerson, allocator);

        document.AddMember("scaleWithResolution", ui.scaleWithResolution, allocator);

        document.AddMember("positionXPercent", ui.positionXPercent, allocator);

        document.AddMember("positionYPercent", ui.positionYPercent, allocator);

        document.AddMember("attachOffsetXPixels", ui.attachOffsetXPixels, allocator);

        document.AddMember("attachOffsetYPixels", ui.attachOffsetYPixels, allocator);

        document.AddMember("scalePercent", ui.scalePercent, allocator);

        document.AddMember("diameterPixels", ui.diameterPixels, allocator);

        document.AddMember("innerDiameterPercent", ui.innerDiameterPercent, allocator);

        document.AddMember("centerPieceSizePercent", ui.centerPieceSizePercent, allocator);

        document.AddMember("segmentGapPixels", ui.segmentGapPixels, allocator);

        document.AddMember("topAngleDegrees", ui.topAngleDegrees, allocator);

        document.AddMember("rotationDegrees", ui.rotationDegrees, allocator);

        document.AddMember("opacityPercent", ui.opacityPercent, allocator);

        document.AddMember("centerMovementPixels", ui.centerMovementPixels, allocator);

        document.AddMember("centerMovementDurationMs", ui.centerMovementDurationMs, allocator);

        WriteColor(document, "activeColor", ui.activeColor, allocator);

        WriteColor(document, "unblockableColor", ui.unblockableColor, allocator);

        WriteColor(document, "inactiveColor", ui.inactiveColor, allocator);

        WriteColor(document, "borderColor", ui.borderColor, allocator);

        WriteColor(document, "centerColor", ui.centerColor, allocator);

        document.AddMember("sharedSegmentImage", rapidjson::Value(ui.sharedSegmentImage.c_str(), allocator).Move(), allocator);

        rapidjson::Value images(rapidjson::kArrayType);

        for (const auto& image : ui.segmentImages)
        {
            images.PushBack(rapidjson::Value(image.c_str(), allocator).Move(), allocator);
        }

        document.AddMember("segmentImages", images, allocator);

        document.AddMember("centerImage", rapidjson::Value(ui.centerImage.c_str(), allocator).Move(), allocator);

        rapidjson::StringBuffer buffer;

        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

        document.Accept(writer);

        std::ofstream file(kPlayerUISettingsPath, std::ios::binary);

        if (file.is_open())
        {
            file << buffer.GetString();
        }

        const auto& npc = Settings::NPCUI;

        rapidjson::Document npcDocument;
        npcDocument.SetObject();

        auto& npcAllocator = npcDocument.GetAllocator();

        npcDocument.AddMember("enabled", npc.enabled, npcAllocator);

        npcDocument.AddMember("alwaysShowInCombat", npc.alwaysShowInCombat, npcAllocator);

        npcDocument.AddMember("scaleWithResolution", npc.scaleWithResolution, npcAllocator);

        npcDocument.AddMember("scaleWithDistance", npc.scaleWithDistance, npcAllocator);

        npcDocument.AddMember("mirrorAttackDirections", npc.mirrorAttackDirections, npcAllocator);

        npcDocument.AddMember("scalePercent", npc.scalePercent, npcAllocator);

        npcDocument.AddMember("opacityPercent", npc.opacityPercent, npcAllocator);

        npcDocument.AddMember("offsetXPixels", npc.offsetXPixels, npcAllocator);

        npcDocument.AddMember("offsetYPixels", npc.offsetYPixels, npcAllocator);

        npcDocument.AddMember("rotationDegrees", npc.rotationDegrees, npcAllocator);

        npcDocument.AddMember("diameterPixels", npc.diameterPixels, npcAllocator);

        npcDocument.AddMember("innerDiameterPercent", npc.innerDiameterPercent, npcAllocator);

        npcDocument.AddMember("segmentGapPixels", npc.segmentGapPixels, npcAllocator);

        npcDocument.AddMember("topAngleDegrees", npc.topAngleDegrees, npcAllocator);

        npcDocument.AddMember("npcOnlyCombat", NPCOnlyCombat, npcAllocator);

        npcDocument.AddMember("topThreshold", npc.topThreshold, npcAllocator);

        WriteColor(npcDocument, "activeColor", npc.activeColor, npcAllocator);

        WriteColor(npcDocument, "unblockableColor", npc.unblockableColor, npcAllocator);

        WriteColor(npcDocument, "inactiveColor", npc.inactiveColor, npcAllocator);

        WriteColor(npcDocument, "borderColor", npc.borderColor, npcAllocator);

        npcDocument.AddMember("sharedSegmentImage", rapidjson::Value(npc.sharedSegmentImage.c_str(), npcAllocator).Move(), npcAllocator);

        rapidjson::Value npcImages(rapidjson::kArrayType);

        for (const auto& image : npc.segmentImages)
        {
            npcImages.PushBack(rapidjson::Value(image.c_str(), npcAllocator).Move(), npcAllocator);
        }

        npcDocument.AddMember("segmentImages", npcImages, npcAllocator);

        rapidjson::StringBuffer npcBuffer;

        rapidjson::PrettyWriter<rapidjson::StringBuffer> npcWriter(npcBuffer);

        npcDocument.Accept(npcWriter);

        std::ofstream npcFile(kNPCUISettingsPath, std::ios::binary);

        if (npcFile.is_open())
        {
            npcFile << npcBuffer.GetString();
        }

        rapidjson::Document inputDocument;
        inputDocument.SetObject();

        auto& inputAllocator = inputDocument.GetAllocator();
        const auto& input = Settings::InputTracking;

        inputDocument.AddMember("directionSwitchDelayMs", input.directionSwitchDelayMs, inputAllocator);
        inputDocument.AddMember("neutralDecayMs", input.neutralDecayMs, inputAllocator);

        rapidjson::StringBuffer inputBuffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> inputWriter(inputBuffer);
        inputDocument.Accept(inputWriter);

        std::ofstream inputFile(kInputTrackingSettingsPath, std::ios::binary);

        if (inputFile.is_open())
        {
            inputFile << inputBuffer.GetString();
        }
    }

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

            changed |= ImGui::Checkbox(GetLoc("menu.show_only_while_blocking", "Show HUD only while blocking"), &ui.showOnlyWhileBlocking);

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

            changed |= ImGui::ColorEdit4(GetLoc("menu.unblockable_color", "Unblockable attack"), ui.unblockableColor.data(), colorFlags);

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

        ClampSettings(ui);

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

            ImGui::TextWrapped("%s", GetLoc("menu.npc_warnings_mirror_help", "Mirroring swaps left and right for a front-facing view. Leave it disabled when the direction already matches what you see in game."));

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

            changed |= ImGui::ColorEdit4(GetLoc("menu.unblockable_color", "Unblockable attack"), npc.unblockableColor.data(), colorFlags);

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

        ClampSettings(npc);

        if (changed)
        {
            SaveSettings();
            Prisma::ApplyUISettings();
        }
    }

    void InputTrackingRender()
    {
        auto& input = Settings::InputTracking;
        bool changed = false;

        if (ImGui::CollapsingHeader("Camera Direction Tracking", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            changed |= RenderIntSliderWithInput("Direction switch delay (ms)", &input.directionSwitchDelayMs, 0, 1000);

            ImGui::TextWrapped(
                "Delay before switching between non-neutral camera directions. "
                "Set to 0 for instant switching.");

            changed |= RenderIntSliderWithInput("Neutral reset delay (ms)", &input.neutralDecayMs, 0, 5000);

            ImGui::TextWrapped("Time without camera activity before the direction resets to neutral.");

            ImGui::Unindent();
        }

        input.directionSwitchDelayMs = std::clamp(input.directionSwitchDelayMs, 0, 1000);

        input.neutralDecayMs = std::clamp(input.neutralDecayMs, 0, 5000);

        if (changed)
        {
            SaveSettings();
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

        SKSEMenuFramework::AddSectionItem("Input Tracking", InputTrackingRender);
    }
}