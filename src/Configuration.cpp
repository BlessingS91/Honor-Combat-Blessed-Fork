#include "Configuration.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

namespace
{
    constexpr const char* kModDirectory = "Data/Viny Mods/Honor Combat";
    constexpr const char* kLegacyUISettingsPath = "Data/Viny Mods/Honor Combat/UISettings.json";
    constexpr const char* kPlayerUISettingsPath = "Data/Viny Mods/Honor Combat/PlayerUISettings.json";
    constexpr const char* kNPCUISettingsPath = "Data/Viny Mods/Honor Combat/NPCUISettings.json";
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
        ClampColor(ui.inactiveColor);
        ClampColor(ui.borderColor);

        ui.sharedSegmentImage = NormalizeAssetPath(std::move(ui.sharedSegmentImage));

        for (auto& path : ui.segmentImages)
        {
            path = NormalizeAssetPath(std::move(path));
        }
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
            readBool("showCenter", ui.showCenter);
            readBool("moveCenterWithDirection", ui.moveCenterWithDirection);
            readBool("centerInThirdPerson", ui.centerInThirdPerson);
            readBool("scaleWithResolution", ui.scaleWithResolution);

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

                readBool("npcOnlyCombat", NPCOnlyCombat);
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
    }
}