#pragma once

#include "RE/Skyrim.h"

class AttackPosition
{
public:
    static AttackPosition* GetSingleton();

    void RegisterNPCAnimationSink(RE::Actor* a_actor);
    void RegisterSinksForExistingCombatants();
    void ScheduleSinkRegistration();
    void Initialize();

    void ResetAttackDirection(RE::Actor* a_actor);
    void ResolveAttackDirection(RE::Actor* a_actor, bool a_leftHand);

    class NpcCycleSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
    {
    public:
        static NpcCycleSink* GetSingleton();

        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) override;
    };

    class NpcCombatTracker : public RE::BSTEventSink<RE::TESCombatEvent>
    {
    public:
        static NpcCombatTracker* GetSingleton();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>* a_eventSource) override;
    };

    class PC3DLoadEventHandler : public RE::BSTEventSink<RE::TESObjectLoadedEvent>
    {
    public:
        static PC3DLoadEventHandler* GetSingleton();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>* a_eventSource) override;
    };

private:
    AttackPosition() = default;

    void RegisterSink(RE::Actor* a_actor);
    void UnregisterSink(RE::Actor* a_actor);
    void ScheduleSinkRegistration(RE::Actor* a_actor, int a_attempts);

    bool npcCycleRegistered_{false};
    bool npcCombatRegistered_{false};
    bool npcLoadRegistered_{false};
};