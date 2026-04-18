#pragma once

#include "ll/api/event/ListenerBase.h"

#include <string>
#include <unordered_map>

class Player;

namespace my_mod {

class LandFlightService {
public:
    static LandFlightService& getInstance();

    void enable();

    void disable();

private:
    struct FlightSession {
        int  secondsUntilCharge = 0;
        bool pluginGrantedMayFly = false;
        bool activeInLand        = false;
    };

    LandFlightService() = default;

    void registerCommand();
    void registerTickListener();
    void clearSessions(bool notifyPlayers, std::string const& reason);
    void processSecond();
    void openMainForm(Player& player);
    [[nodiscard]] std::string buildMainFormContent(Player& player) const;
    [[nodiscard]] bool enableForPlayer(Player& player, std::string* errorMessage = nullptr);

    void handleStatusCommand(Player& player) const;
    void handleDisableCommand(Player& player, bool notifyPlayer, std::string const& reason);

    [[nodiscard]] bool        hasSession(Player const& player) const;
    [[nodiscard]] FlightSession* tryGetSession(Player const& player);
    [[nodiscard]] FlightSession const* tryGetSession(Player const& player) const;
    [[nodiscard]] int         getChargeIntervalSeconds() const;
    [[nodiscard]] long long   getChargeAmount() const;

    ll::event::ListenerPtr mTickListener;
    std::unordered_map<std::string, FlightSession> mSessions;
    bool                  mCommandRegistered = false;
    bool                  mRunning           = false;
    int                   mTickCounter       = 0;
};

} // namespace my_mod
