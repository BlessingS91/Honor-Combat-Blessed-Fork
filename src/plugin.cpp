#include "Plugin.h"

#include "AttackPosition.h"
#include "Configuration.h"
#include "Events.h"
#include "Hooks.h"
#include "InputTracking.h"
#include "Prisma.h"

namespace
{
    void OnMessage(SKSE::MessagingInterface::Message* message)
    {
        switch (message->type)
        {
            case SKSE::MessagingInterface::kPostLoad:
                Prisma::Install();
                ModMenu::Register();
                HonorCombatEventHandler::GetSingleton()->InitializeTDMAPI();
                break;
            case SKSE::MessagingInterface::kDataLoaded:
                Prisma::Preload();
                HonorCombatEventHandler::GetSingleton()->RegisterMenuListener();
                AttackPosition::GetSingleton()->Initialize();
                break;
            case SKSE::MessagingInterface::kPreLoadGame:
                HonorCombatEventHandler::GetSingleton()->ClearAttackWarnings();
                Prisma::Reset();
                break;
            case SKSE::MessagingInterface::kNewGame:
            case SKSE::MessagingInterface::kPostLoadGame:
                HonorCombatEventHandler::GetSingleton()->Reset();
                DirectionTracker::GetSingleton()->Initialize();
                break;
            default:
                break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    SetupLog();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    HonorCombatEventHandler::GetSingleton()->RegisterDMKListener();

    Hooks::Install();
    logger::info("Honor Combat loaded.");
    return true;
}
