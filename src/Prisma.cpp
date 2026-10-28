#include "Prisma.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Configuration.h"
#include "PrismaUI_API.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace
{
    PRISMA_UI_API::IVPrismaUI1* prismaUI = nullptr;
    PrismaView view = 0;

    bool domReady = false;

    // Raw gameplay direction.
    // 0 = none, 1 = top, 2/4/5/6/8 = other raw directions,
    // 7 = left, 3 = right.
    int currentDirection = 0;
    int currentRawDirection = 0;
    bool currentAllBlock = false;
    bool currentUnblockable = false;

    bool runtimeEligible = false;
    bool runtimePreviewEligible = false;
    bool runtimeAttached = false;

    float runtimeXPercent = 50.0f;
    float runtimeYPercent = 50.0f;
    float runtimeResolutionScale = 1.0f;

    std::vector<AttackWarningVisual> attackWarnings;

    void SyncVisibility();

    bool IsViewValid() { return prismaUI && view && prismaUI->IsValid(view); }

    void WriteColor(rapidjson::Writer<rapidjson::StringBuffer>& writer, const std::array<float, 4>& color)
    {
        writer.StartArray();

        for (const float component : color)
        {
            writer.Double(component);
        }

        writer.EndArray();
    }

    std::string BuildSettingsPayload()
    {
        const auto& ui = Settings::PlayerUI;

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        writer.StartObject();

        writer.Key("enabled");
        writer.Bool(ui.enabled);

        writer.Key("editMode");
        writer.Bool(ui.editMode);

        writer.Key("showCenter");
        writer.Bool(ui.showCenter);

        writer.Key("moveCenterWithDirection");
        writer.Bool(ui.moveCenterWithDirection);

        writer.Key("centerInThirdPerson");
        writer.Bool(ui.centerInThirdPerson);

        writer.Key("scaleWithResolution");
        writer.Bool(ui.scaleWithResolution);

        writer.Key("positionXPercent");
        writer.Int(ui.positionXPercent);

        writer.Key("positionYPercent");
        writer.Int(ui.positionYPercent);

        writer.Key("attachOffsetXPixels");
        writer.Int(ui.attachOffsetXPixels);

        writer.Key("attachOffsetYPixels");
        writer.Int(ui.attachOffsetYPixels);

        writer.Key("scalePercent");
        writer.Int(ui.scalePercent);

        writer.Key("diameterPixels");
        writer.Int(ui.diameterPixels);

        writer.Key("innerDiameterPercent");
        writer.Int(ui.innerDiameterPercent);

        writer.Key("centerPieceSizePercent");
        writer.Int(ui.centerPieceSizePercent);

        writer.Key("segmentGapPixels");
        writer.Int(ui.segmentGapPixels);

        writer.Key("topAngleDegrees");
        writer.Int(ui.topAngleDegrees);

        writer.Key("rotationDegrees");
        writer.Int(ui.rotationDegrees);

        writer.Key("opacityPercent");
        writer.Int(ui.opacityPercent);

        writer.Key("centerMovementPixels");
        writer.Int(ui.centerMovementPixels);

        writer.Key("centerMovementDurationMs");
        writer.Int(ui.centerMovementDurationMs);

        writer.Key("activeColor");
        WriteColor(writer, ui.activeColor);

        writer.Key("unblockableColor");
        WriteColor(writer, ui.unblockableColor);

        writer.Key("inactiveColor");
        WriteColor(writer, ui.inactiveColor);

        writer.Key("borderColor");
        WriteColor(writer, ui.borderColor);

        writer.Key("centerColor");
        WriteColor(writer, ui.centerColor);

        writer.Key("sharedSegmentImage");
        writer.String(ui.sharedSegmentImage.c_str());

        writer.Key("segmentImages");
        writer.StartArray();

        for (const auto& path : ui.segmentImages)
        {
            writer.String(path.c_str());
        }

        writer.EndArray();

        writer.Key("centerImage");
        writer.String(ui.centerImage.c_str());

        const auto& npc = Settings::NPCUI;

        writer.Key("npc");
        writer.StartObject();

        writer.Key("enabled");
        writer.Bool(npc.enabled);

        writer.Key("alwaysShowInCombat");
        writer.Bool(npc.alwaysShowInCombat);

        writer.Key("scaleWithResolution");
        writer.Bool(npc.scaleWithResolution);

        writer.Key("scaleWithDistance");
        writer.Bool(npc.scaleWithDistance);

        writer.Key("mirrorAttackDirections");
        writer.Bool(npc.mirrorAttackDirections);

        writer.Key("scalePercent");
        writer.Int(npc.scalePercent);

        writer.Key("opacityPercent");
        writer.Int(npc.opacityPercent);

        writer.Key("offsetXPixels");
        writer.Int(npc.offsetXPixels);

        writer.Key("offsetYPixels");
        writer.Int(npc.offsetYPixels);

        writer.Key("rotationDegrees");
        writer.Int(npc.rotationDegrees);

        writer.Key("diameterPixels");
        writer.Int(npc.diameterPixels);

        writer.Key("innerDiameterPercent");
        writer.Int(npc.innerDiameterPercent);

        writer.Key("segmentGapPixels");
        writer.Int(npc.segmentGapPixels);

        writer.Key("topAngleDegrees");
        writer.Int(npc.topAngleDegrees);

        writer.Key("activeColor");
        WriteColor(writer, npc.activeColor);

        writer.Key("unblockableColor");
        WriteColor(writer, npc.unblockableColor);

        writer.Key("inactiveColor");
        WriteColor(writer, npc.inactiveColor);

        writer.Key("borderColor");
        WriteColor(writer, npc.borderColor);

        writer.Key("sharedSegmentImage");
        writer.String(npc.sharedSegmentImage.c_str());

        writer.Key("segmentImages");
        writer.StartArray();

        for (const auto& path : npc.segmentImages)
        {
            writer.String(path.c_str());
        }

        writer.EndArray();

        writer.EndObject();
        writer.EndObject();

        return buffer.GetString();
    }

    void SendSettings()
    {
        if (!domReady || !IsViewValid())
        {
            return;
        }

        static std::string payload;
        payload = BuildSettingsPayload();

        prismaUI->InteropCall(view, "updateHonorCombatSettings", payload.c_str());
    }

    void SendDirection()
    {
        if (!domReady || !IsViewValid())
        {
            return;
        }

        static std::string payload;
        payload = std::to_string(currentDirection) + "|" + (currentAllBlock ? "1" : "0") + "|" + (currentUnblockable ? "1" : "0");

        prismaUI->InteropCall(view, "updateHonorCombatDirection", payload.c_str());
    }

    void SendRuntimeState()
    {
        if (!domReady || !IsViewValid())
        {
            return;
        }

        char payload[128]{};

        std::snprintf(payload, sizeof(payload), "%d|%d|%.3f|%.3f|%.3f|%d", runtimeEligible ? 1 : 0, runtimePreviewEligible ? 1 : 0, runtimeXPercent, runtimeYPercent, runtimeResolutionScale, runtimeAttached ? 1 : 0);

        prismaUI->InteropCall(view, "updateHonorCombatRuntime", payload);
    }

    void SendAttackWarnings()
    {
        if (!domReady || !IsViewValid())
        {
            return;
        }

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        writer.StartArray();

        for (const auto& warning : attackWarnings)
        {
            writer.StartObject();

            writer.Key("id");
            writer.Uint(warning.id);

            writer.Key("direction");
            writer.Int(warning.direction);

            writer.Key("xPercent");
            writer.Double(warning.xPercent);

            writer.Key("yPercent");
            writer.Double(warning.yPercent);

            writer.Key("resolutionScale");
            writer.Double(warning.resolutionScale);

            writer.Key("distanceScale");
            writer.Double(warning.distanceScale);

            writer.Key("hitFrameFlash");
            writer.Bool(warning.hitFrameFlash);

            writer.Key("unblockable");
            writer.Bool(warning.unblockable);

            writer.EndObject();
        }

        writer.EndArray();

        prismaUI->InteropCall(view, "updateHonorCombatAttackWarnings", buffer.GetString());
    }

    void SendAllState()
    {
        SendSettings();
        SendDirection();
        SendRuntimeState();
        SendAttackWarnings();
    }

    bool EnsureView()
    {
        if (!prismaUI)
        {
            return false;
        }

        if (IsViewValid())
        {
            return true;
        }

        view = 0;
        domReady = false;

#ifdef DEV_SERVER
        constexpr const char* path = "http://localhost:5173";
#else
        constexpr const char* path = PRODUCT_NAME "/index.html";
#endif

        view = prismaUI->CreateView(path,
                                    [](PrismaView readyView)
                                    {
                                        if (!prismaUI || !view || readyView != view)
                                        {
                                            return;
                                        }

                                        domReady = true;

                                        SendAllState();
                                        SyncVisibility();
                                    });

        if (!view)
        {
            logger::error("Honor Combat could not create its PrismaUI view.");

            return false;
        }

        prismaUI->RegisterJSListener(view, "hideHonorCombat", [](const char*) { Prisma::Hide(); });

        return true;
    }

    void SyncVisibility()
    {
        const bool shouldShow = (Settings::PlayerUI.enabled && (runtimeEligible || (Settings::PlayerUI.editMode && runtimePreviewEligible))) || (Settings::NPCUI.enabled && !attackWarnings.empty());

        const bool viewValid = IsViewValid();

        const bool hidden = viewValid ? prismaUI->IsHidden(view) : true;

        if (!shouldShow)
        {
            if (viewValid && !hidden)
            {
                prismaUI->Hide(view);
            }

            return;
        }

        if (!EnsureView() || !domReady)
        {
            return;
        }

        if (prismaUI->IsHidden(view))
        {
            prismaUI->Show(view);
        }
    }
}

void Prisma::Install()
{
    prismaUI = reinterpret_cast<PRISMA_UI_API::IVPrismaUI1*>(PRISMA_UI_API::RequestPluginAPI());

    if (!prismaUI)
    {
        logger::error("PrismaUI.dll was not found; Honor Combat HUD is unavailable.");
    }
    else
    {
        logger::info("Honor Combat linked to PrismaUI!");
    }
}

void Prisma::Preload()
{
    if (!EnsureView())
    {
        return;
    }

    prismaUI->Hide(view);

    SendAllState();
    SyncVisibility();
}

void Prisma::Show()
{
    if (EnsureView())
    {
        prismaUI->Show(view);
    }
}

void Prisma::Hide()
{
    if (IsViewValid())
    {
        prismaUI->Hide(view);
    }
}

bool Prisma::IsHidden() { return !IsViewValid() || prismaUI->IsHidden(view); }

bool Prisma::IsReady() { return domReady && IsViewValid(); }

void Prisma::ApplyUISettings()
{
    if (!prismaUI)
    {
        return;
    }

    if (Settings::PlayerUI.enabled && (runtimeEligible || (Settings::PlayerUI.editMode && runtimePreviewEligible)))
    {
        EnsureView();
    }

    SendSettings();
    SendDirection();
    SyncVisibility();
}

void Prisma::UpdateDirection(int direction)
{
    direction = std::clamp(direction, 0, 8);

    currentRawDirection = direction;
    currentDirection = direction;

    SendDirection();
    SyncVisibility();
}

void Prisma::UpdateAllBlock(bool allBlock)
{
    if (currentAllBlock == allBlock)
    {
        return;
    }

    currentAllBlock = allBlock;
    SendDirection();
}

void Prisma::UpdateUnblockable(bool unblockable)
{
    if (currentUnblockable == unblockable)
    {
        return;
    }

    currentUnblockable = unblockable;
    SendDirection();
}

void Prisma::UpdateRuntimeState(bool eligible, bool previewEligible, float xPercent, float yPercent, float resolutionScale, bool attached)
{
    xPercent = std::clamp(xPercent, 0.0f, 100.0f);
    yPercent = std::clamp(yPercent, 0.0f, 100.0f);
    resolutionScale = std::clamp(resolutionScale, 0.5f, 4.0f);

    const bool changed = runtimeEligible != eligible || runtimePreviewEligible != previewEligible || runtimeAttached != attached || std::abs(runtimeXPercent - xPercent) >= 0.025f || std::abs(runtimeYPercent - yPercent) >= 0.025f ||
                         std::abs(runtimeResolutionScale - resolutionScale) >= 0.005f;

    runtimeEligible = eligible;
    runtimePreviewEligible = previewEligible;
    runtimeAttached = attached;

    runtimeXPercent = xPercent;
    runtimeYPercent = yPercent;
    runtimeResolutionScale = resolutionScale;

    if (changed)
    {
        SendRuntimeState();
    }

    SyncVisibility();
}

void Prisma::UpdateAttackWarnings(const std::vector<AttackWarningVisual>& warnings)
{
    const auto nearlyEqual = [](float left, float right, float threshold) { return std::abs(left - right) < threshold; };

    const bool changed = attackWarnings.size() != warnings.size() || !std::equal(attackWarnings.begin(), attackWarnings.end(), warnings.begin(), warnings.end(),
                                                                                 [&](const AttackWarningVisual& left, const AttackWarningVisual& right)
                                                                                 {
                                                                                     return left.id == right.id && left.direction == right.direction && nearlyEqual(left.xPercent, right.xPercent, 0.025f) &&
                                                                                            nearlyEqual(left.yPercent, right.yPercent, 0.025f) && nearlyEqual(left.resolutionScale, right.resolutionScale, 0.005f) &&
                                                                                            nearlyEqual(left.distanceScale, right.distanceScale, 0.005f) && left.hitFrameFlash == right.hitFrameFlash && left.unblockable == right.unblockable;
                                                                                 });

    if (!changed)
    {
        return;
    }

    attackWarnings = warnings;

    SendAttackWarnings();
    SyncVisibility();
}

void Prisma::Reset()
{
    currentRawDirection = 0;
    currentDirection = 0;
    currentAllBlock = false;
    currentUnblockable = false;

    runtimeEligible = false;
    runtimePreviewEligible = false;
    runtimeAttached = false;

    runtimeXPercent = 50.0f;
    runtimeYPercent = 50.0f;
    runtimeResolutionScale = 1.0f;

    attackWarnings.clear();

    SendDirection();
    SendRuntimeState();
    SendAttackWarnings();

    SyncVisibility();
}