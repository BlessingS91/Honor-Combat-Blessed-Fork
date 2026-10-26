#include "AttackPosition.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <exception>
#include <initializer_list>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "Configuration.h"
#include "DelayedDispatcher.h"

namespace
{
    constexpr char kAttackDirectionVariable[] = "AttackDirectionFHB";
    constexpr char kAttackTelegraphEvent[] = "DMKAttackTelegraph";

    std::mutex g_attackDirectionsMutex;
    std::unordered_map<RE::FormID, int> g_attackDirections;

    std::mutex g_trackedNPCsMutex;
    std::unordered_set<RE::FormID> g_trackedNPCs;

    RE::NiAVObject* FindFirstNode(RE::NiAVObject* a_root, std::initializer_list<const char*> a_names)
    {
        if (!a_root)
        {
            return nullptr;
        }

        for (const auto* name : a_names)
        {
            if (auto* node = a_root->GetObjectByName(RE::BSFixedString(name)))
            {
                return node;
            }
        }

        return nullptr;
    }

    std::optional<RE::NiPoint3> GetHandPositionRelativeToPlayer(RE::Actor* a_actor, bool a_leftHand)
    {
        if (!a_actor || !a_actor->Is3DLoaded())
        {
            return std::nullopt;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded())
        {
            return std::nullopt;
        }

        auto* actorRoot = a_actor->Get3D(false);
        auto* playerRoot = player->Get3D(false);

        if (!actorRoot || !playerRoot)
        {
            return std::nullopt;
        }

        auto* hand = a_leftHand ? FindFirstNode(actorRoot, {"NPC L Hand [LHnd]", "NPC L Forearm [LLar]"}) : FindFirstNode(actorRoot, {"NPC R Hand [RHnd]", "NPC R Forearm [RLar]", "WEAPON"});

        if (!hand)
        {
            return std::nullopt;
        }

        return playerRoot->world.Invert() * hand->world.translate;
    }

    // chest height system
    //  std::optional<float> GetPlayerChestHeight()
    //  {
    //      auto* player = RE::PlayerCharacter::GetSingleton();
    //      if (!player || !player->Is3DLoaded())
    //      {
    //          return std::nullopt;
    //      }

    //     auto* playerRoot = player->Get3D(false);
    //     if (!playerRoot)
    //     {
    //         return std::nullopt;
    //     }

    //     auto* chest = FindFirstNode(playerRoot, {"NPC Spine2 [Spn2]", "NPC Spine1 [Spn1]", "NPC Spine [Spn0]"});

    //     if (!chest)
    //     {
    //         return std::nullopt;
    //     }

    //     const auto chestLocalPosition = playerRoot->world.Invert() * chest->world.translate;

    //     return chestLocalPosition.z;
    // }

    int ClassifyHandPositionRelativeToPlayer(const RE::NiPoint3& a_handPosition)
    {
        const float horizontalThreshold = Settings::NPCUI.topThreshold;

        if (a_handPosition.x > horizontalThreshold)
        {
            return 3;  // Left
        }

        if (a_handPosition.x < -horizontalThreshold)
        {
            return 2;  // Right
        }

        return 1;  // Top
    }

    // chest height system
    //  int ClassifyHandPositionRelativeToPlayer(const RE::NiPoint3& a_handPosition)
    //  {
    //      constexpr float kHorizontalThreshold = 20.0f;
    //      constexpr float kChestVerticalThreshold = 10.0f;

    //     const auto chestHeight = GetPlayerChestHeight();

    //     if (!chestHeight)
    //     {
    //         if (a_handPosition.x > kHorizontalThreshold)
    //         {
    //             return 3;
    //         }

    //         if (a_handPosition.x < -kHorizontalThreshold)
    //         {
    //             return 7;
    //         }

    //         return 1;
    //     }

    //     const float relativeZ = a_handPosition.z - *chestHeight;

    //     if (relativeZ > kChestVerticalThreshold)
    //     {
    //         return 1;
    //     }

    //     if (a_handPosition.x > kHorizontalThreshold)
    //     {
    //         return 3;
    //     }

    //     if (a_handPosition.x < -kHorizontalThreshold)
    //     {
    //         return 7;
    //     }

    //     return 1;
    // }

    bool DispatchAttackTelegraph(RE::Actor* a_actor, const char* a_phase, int a_direction)
    {
        if (!a_actor || !a_phase)
        {
            return false;
        }

        auto* eventSource = SKSE::GetModCallbackEventSource();

        if (!eventSource)
        {
            SKSE::log::error("[DMKAttackTelegraph] Event source unavailable. actor={:08X} phase={} direction={}", a_actor->GetFormID(), a_phase, a_direction);

            return false;
        }

        SKSE::ModCallbackEvent event{RE::BSFixedString(kAttackTelegraphEvent), RE::BSFixedString(a_phase), static_cast<float>(a_direction), a_actor};

        eventSource->SendEvent(&event);

        SKSE::log::info("[DMKAttackTelegraph] Sent actor={:08X} phase={} direction={}", a_actor->GetFormID(), a_phase, a_direction);

        return true;
    }

    std::string ToLower(std::string_view a_value)
    {
        std::string result;
        result.reserve(a_value.size());

        for (const auto character : a_value)
        {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        }

        return result;
    }
}

AttackPosition* AttackPosition::GetSingleton()
{
    static AttackPosition singleton;
    return std::addressof(singleton);
}

void AttackPosition::ResetAttackDirection(RE::Actor* a_actor)
{
    if (!a_actor)
    {
        return;
    }

    const bool graphUpdated = a_actor->SetGraphVariableInt(kAttackDirectionVariable, 0);

    {
        std::scoped_lock lock(g_attackDirectionsMutex);
        g_attackDirections.erase(a_actor->GetFormID());
    }

    SKSE::log::info("[NPC Attack Direction] Reset actor={:08X} graphUpdated={}", a_actor->GetFormID(), graphUpdated);

    DispatchAttackTelegraph(a_actor, "End", 0);
}

void AttackPosition::ResolveAttackDirection(RE::Actor* a_actor, bool a_leftHand)
{
    if (!a_actor)
    {
        return;
    }

    const auto handPosition = GetHandPositionRelativeToPlayer(a_actor, a_leftHand);

    if (!handPosition)
    {
        SKSE::log::info("[NPC Attack Direction] Resolve failed actor={:08X}: {} hand not found.", a_actor->GetFormID(), a_leftHand ? "left" : "right");

        return;
    }

    const int direction = ClassifyHandPositionRelativeToPlayer(*handPosition);

    {
        std::scoped_lock lock(g_attackDirectionsMutex);
        g_attackDirections[a_actor->GetFormID()] = direction;
    }

    const bool graphUpdated = a_actor->SetGraphVariableInt(kAttackDirectionVariable, direction);

    DispatchAttackTelegraph(a_actor, "Resolved", direction);

    SKSE::log::info("[NPC Attack Direction] NPC {:08X}: hand={} localX={:.2f} localY={:.2f} localZ={:.2f} direction={} graphUpdated={}.", a_actor->GetFormID(), a_leftHand ? "L" : "R", handPosition->x, handPosition->y, handPosition->z,
                    direction, graphUpdated);
}

RE::BSEventNotifyControl AttackPosition::NpcCycleSink::ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*)
{
    if (!a_event || !a_event->holder)
    {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* actor = a_event->holder->As<RE::Actor>();

    if (!actor || actor->IsPlayerRef())
    {
        return RE::BSEventNotifyControl::kContinue;
    }

    try
    {
        const std::string rawEventName = a_event->tag.c_str();
        const std::string eventName = ToLower(rawEventName);

        SKSE::log::info("[NPC Attack Direction] Event actor={:08X} tag={}", actor->GetFormID(), rawEventName);

        if (eventName.find("weaponleftswing") != std::string::npos)
        {
            AttackPosition::GetSingleton()->ResolveAttackDirection(actor, true);
        }
        else if (eventName.find("weaponswing") != std::string::npos)
        {
            AttackPosition::GetSingleton()->ResolveAttackDirection(actor, false);
        }
        else if (eventName == "hitframe" || eventName == "FakeHit")
        {
            int direction = 0;

            {
                std::scoped_lock lock(g_attackDirectionsMutex);
                const auto it = g_attackDirections.find(actor->GetFormID());

                if (it != g_attackDirections.end())
                {
                    direction = it->second;
                }
            }

            if (direction != 0)
            {
                DispatchAttackTelegraph(actor, "HitFrame", direction);
            }
        }
        else if (eventName == "mco_winopen" || eventName == "mco_powerwinopen" || eventName == "scar_combostart" || eventName == "staggerstart" || eventName == "blockstart" || eventName == "attackwinstart" || eventName == "attackstop" ||
                 eventName == "sbf_staggerstart" || eventName == "sbf_staggerstop")
        {
            SKSE::log::info("[NPC Attack Direction] Reset actor={:08X} tag={}", actor->GetFormID(), rawEventName);

            AttackPosition::GetSingleton()->ResetAttackDirection(actor);
        }
    }
    catch (const std::exception& e)
    {
        SKSE::log::error("[NPC Attack Direction] Exception for NPC {:08X}: {}", actor->GetFormID(), e.what());
    }
    catch (...)
    {
        SKSE::log::error("[NPC Attack Direction] Unknown exception for NPC {:08X}.", actor->GetFormID());
    }

    return RE::BSEventNotifyControl::kContinue;
}

AttackPosition::NpcCycleSink* AttackPosition::NpcCycleSink::GetSingleton()
{
    static NpcCycleSink singleton;
    return std::addressof(singleton);
}

RE::BSEventNotifyControl AttackPosition::NpcCombatTracker::ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>*)
{
    if (!a_event)
    {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* actorRef = a_event->actor.get();
    auto* actor = actorRef ? actorRef->As<RE::Actor>() : nullptr;

    if (!actor || actor->IsPlayerRef())
    {
        return RE::BSEventNotifyControl::kContinue;
    }

    const auto newState = *a_event->newState;

    if (newState == RE::ACTOR_COMBAT_STATE::kCombat)
    {
        AttackPosition::GetSingleton()->RegisterSink(actor);
    }
    else if (newState == RE::ACTOR_COMBAT_STATE::kNone)
    {
        AttackPosition::GetSingleton()->ResetAttackDirection(actor);

        if (ModMenu::NPCOnlyCombat)
        {
            AttackPosition::GetSingleton()->UnregisterSink(actor);
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}

AttackPosition::NpcCombatTracker* AttackPosition::NpcCombatTracker::GetSingleton()
{
    static NpcCombatTracker singleton;
    return std::addressof(singleton);
}

void AttackPosition::RegisterNPCAnimationSink(RE::Actor* a_actor) { RegisterSink(a_actor); }

void AttackPosition::RegisterSink(RE::Actor* a_actor)
{
    if (!a_actor || a_actor->IsPlayerRef())
    {
        return;
    }

    std::unique_lock lock(g_trackedNPCsMutex);

    const auto formID = a_actor->GetFormID();

    if (g_trackedNPCs.find(formID) == g_trackedNPCs.end())
    {
        const bool registered = a_actor->AddAnimationGraphEventSink(NpcCycleSink::GetSingleton());

        if (registered)
        {
            g_trackedNPCs.insert(formID);
        }

        SKSE::log::info("[NPC Attack Direction] Register actor={:08X} success={} tracked={}", formID, registered, g_trackedNPCs.contains(formID));
    }
}

void AttackPosition::UnregisterSink(RE::Actor* a_actor)
{
    if (!a_actor || a_actor->IsPlayerRef())
    {
        return;
    }

    std::scoped_lock lock(g_trackedNPCsMutex);

    const auto formID = a_actor->GetFormID();

    if (!g_trackedNPCs.contains(formID))
    {
        return;
    }

    a_actor->RemoveAnimationGraphEventSink(NpcCycleSink::GetSingleton());

    g_trackedNPCs.erase(formID);
}

void AttackPosition::RegisterSinksForExistingCombatants()
{
    auto* processLists = RE::ProcessLists::GetSingleton();

    if (!processLists)
    {
        return;
    }

    for (auto& actorHandle : processLists->highActorHandles)
    {
        auto actorPtr = actorHandle.get();
        auto* actor = actorPtr.get();

        if (!actor || actor->IsPlayerRef() || !actor->IsInCombat())
        {
            continue;
        }

        RegisterSink(actor);
    }
}

void AttackPosition::ScheduleSinkRegistration()
{
    Utils::DelayedDispatcher::Get().PostDelayed(std::chrono::milliseconds(100),
                                                []
                                                {
                                                    if (!SKSE::GetTaskInterface())
                                                    {
                                                        return;
                                                    }

                                                    SKSE::GetTaskInterface()->AddTask([] { AttackPosition::GetSingleton()->RegisterSinksForExistingCombatants(); });
                                                });
}

void AttackPosition::ScheduleSinkRegistration(RE::Actor* a_actor, int a_attempts)
{
    if (!a_actor)
    {
        return;
    }

    if (a_attempts > 20)
    {
        SKSE::log::critical("[Actor3DLoadEventHandler] Giving up after {} attempts for actor {:08X}.", a_attempts, a_actor->GetFormID());
        return;
    }

    auto actorHandle = a_actor->CreateRefHandle();

    Utils::DelayedDispatcher::Get().PostDelayed(std::chrono::milliseconds(100),
                                                [actorHandle, a_attempts]
                                                {
                                                    SKSE::GetTaskInterface()->AddTask(
                                                        [actorHandle, a_attempts]
                                                        {
                                                            if (!actorHandle || !actorHandle.get())
                                                            {
                                                                return;
                                                            }

                                                            auto actor = actorHandle.get();

                                                            auto ui = RE::UI::GetSingleton();
                                                            if (ui && (ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)))
                                                            {
                                                                return;
                                                            }

                                                            RE::BSTSmartPointer<RE::BSAnimationGraphManager> graphManager;
                                                            actor->GetAnimationGraphManager(graphManager);

                                                            if (graphManager)
                                                            {
                                                                AttackPosition::GetSingleton()->UnregisterSink(actor.get());

                                                                if (!ModMenu::NPCOnlyCombat || actor->IsInCombat())
                                                                {
                                                                    AttackPosition::GetSingleton()->RegisterSink(actor.get());
                                                                }
                                                            }
                                                            else
                                                            {
                                                                AttackPosition::GetSingleton()->ScheduleSinkRegistration(actor.get(), a_attempts + 1);
                                                            }
                                                        });
                                                });
}

RE::BSEventNotifyControl AttackPosition::PC3DLoadEventHandler::ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*)
{
    if (!a_event || !a_event->loaded)
    {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* form = RE::TESForm::LookupByID(a_event->formID);

    auto* actor = form ? form->As<RE::Actor>() : nullptr;

    if (actor)
    {
        AttackPosition::GetSingleton()->ScheduleSinkRegistration(actor, 0);
    }

    return RE::BSEventNotifyControl::kContinue;
}

AttackPosition::PC3DLoadEventHandler* AttackPosition::PC3DLoadEventHandler::GetSingleton()
{
    static PC3DLoadEventHandler singleton;
    return std::addressof(singleton);
}

void AttackPosition::Initialize()
{
    SKSE::log::info("[NPC Attack Direction] Initialize entered");

    auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
    if (!eventSourceHolder)
    {
        return;
    }

    if (!npcCombatRegistered_)
    {
        eventSourceHolder->AddEventSink(NpcCombatTracker::GetSingleton());
        npcCombatRegistered_ = true;
    }

    if (!npcLoadRegistered_)
    {
        eventSourceHolder->AddEventSink(PC3DLoadEventHandler::GetSingleton());
        npcLoadRegistered_ = true;
    }

    ScheduleSinkRegistration();
}