#include "Feature/LandFlightService.h"

#include "LLMoney.h"
#include "mod/Global.h"
#include "pland/PLand.h"
#include "pland/land/Land.h"
#include "pland/land/repo/LandRegistry.h"

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/form/SimpleForm.h"
#include "ll/api/service/Bedrock.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/AbilitiesIndex.h"
#include "mc/world/actor/player/Player.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace my_mod {

namespace {

constexpr int TickPerSecond = 20;

enum class ChargeResult {
    Success,
    NotEnoughMoney,
    Failed
};

Player* tryGetCommandPlayer(CommandOrigin const& origin) {
    auto* entity = origin.getEntity();
    if (entity == nullptr || !entity->isPlayer()) {
        return nullptr;
    }
    return static_cast<Player*>(entity);
}

std::string makeMessage(std::string const& text) { return tr("message.prefix") + text; }

template <typename... Args>
std::string makeMessage(std::string const& key, Args&&... args) {
    return tr("message.prefix") + tr(key, std::forward<Args>(args)...);
}

void sendMessage(Player& player, std::string const& text) { player.sendMessage(makeMessage(text)); }

template <typename... Args>
void sendMessage(Player& player, std::string const& key, Args&&... args) {
    player.sendMessage(makeMessage(key, std::forward<Args>(args)...));
}

std::unordered_map<std::string, Player*> getOnlinePlayersByXuid() {
    std::unordered_map<std::string, Player*> players;
    if (auto level = ll::service::getLevel()) {
        level->forEachPlayer([&](Player& player) {
            players.emplace(player.getXuid(), &player);
            return true;
        });
    }
    return players;
}

std::shared_ptr<land::Land> getEligibleLand(Player& player) {
    auto& registry = land::PLand::getInstance().getLandRegistry();
    auto  land     = registry.getLandAt(player.getFeetBlockPos(), static_cast<land::LandDimid>(player.getDimensionId()));
    if (!land) {
        return nullptr;
    }

    if (!config.landFlight.requireLandMember) {
        return land;
    }

    return land->getPermType(player.getUuid()) == land::LandPermType::Actor ? nullptr : land;
}

ChargeResult tryCharge(Player& player, std::string& error) {
    const auto amount = std::max(0LL, config.landFlight.chargeAmount);
    if (amount <= 0) {
        return ChargeResult::Success;
    }

    auto const xuid = player.getXuid();
    if (LLMoney_Get(xuid) < amount) {
        error = tr("flight.error.not_enough_money", amount);
        return ChargeResult::NotEnoughMoney;
    }
    if (!LLMoney_Reduce(xuid, amount)) {
        error = tr("flight.error.charge_failed");
        return ChargeResult::Failed;
    }
    return ChargeResult::Success;
}

void revokePluginFlight(Player& player, bool& pluginGrantedMayFly) {
    if (!pluginGrantedMayFly) {
        return;
    }

    player.setAbility(AbilitiesIndex::MayFly, false);
    pluginGrantedMayFly = false;
}

} // namespace

LandFlightService& LandFlightService::getInstance() {
    static LandFlightService instance;
    return instance;
}

void LandFlightService::enable() {
    if (mRunning) {
        return;
    }

    if (!mCommandRegistered) {
        registerCommand();
    }

    if (!config.landFlight.enabled) {
        logger.info(tr("landflight.feature_disabled"));
        return;
    }

    registerTickListener();
    mTickCounter = 0;
    mRunning     = true;
    logger.info(tr("landflight.enabled"));
}

void LandFlightService::disable() {
    if (mTickListener) {
        ll::event::EventBus::getInstance().removeListener<ll::event::ServerLevelTickEvent>(mTickListener);
        mTickListener.reset();
    }

    clearSessions(true, tr("flight.stop.reason.plugin_disabled"));

    if (mRunning) {
        logger.info(tr("landflight.disabled"));
    }

    mRunning     = false;
    mTickCounter = 0;
}

void LandFlightService::registerCommand() {
    auto& cmd = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
        config.landFlight.command,
        tr("command.description"),
        CommandPermissionLevel::Any
    );

    if (!config.landFlight.alias.empty()) {
        cmd.alias(config.landFlight.alias);
    }

    cmd.overload().execute([this](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = tryGetCommandPlayer(origin);
        if (player == nullptr) {
            output.error("{}", tr("command.player_only"));
            return;
        }
        openMainForm(*player);
        output.success("{}", tr("command.gui.opened"));
    });

    cmd.overload().text("on").execute([this](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = tryGetCommandPlayer(origin);
        if (player == nullptr) {
            output.error("{}", tr("command.player_only"));
            return;
        }
        std::string error;
        if (!enableForPlayer(*player, &error)) {
            output.error("{}", error);
            return;
        }
        output.success("{}", tr("flight.command_enable_ok"));
    });

    cmd.overload().text("off").execute([this](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = tryGetCommandPlayer(origin);
        if (player == nullptr) {
            output.error("{}", tr("command.player_only"));
            return;
        }
        if (!hasSession(*player)) {
            output.error("{}", tr("flight.error.not_enabled"));
            return;
        }

        handleDisableCommand(*player, true, tr("flight.stop.reason.manual"));
        output.success("{}", tr("flight.command_disable_ok"));
    });

    cmd.overload().text("status").execute([this](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = tryGetCommandPlayer(origin);
        if (player == nullptr) {
            output.error("{}", tr("command.player_only"));
            return;
        }
        handleStatusCommand(*player);
        output.success("{}", tr("command.status.sent"));
    });

    cmd.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = tryGetCommandPlayer(origin);
        if (player == nullptr) {
            output.error("{}", tr("command.player_only"));
            return;
        }
        openMainForm(*player);
        output.success("{}", tr("command.gui.opened"));
    });

    mCommandRegistered = true;
}

void LandFlightService::registerTickListener() {
    if (mTickListener) {
        return;
    }

    mTickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(
        [this](ll::event::ServerLevelTickEvent&) {
            if (!mRunning || mSessions.empty()) {
                return;
            }

            ++mTickCounter;
            if (mTickCounter < TickPerSecond) {
                return;
            }

            mTickCounter = 0;
            processSecond();
        }
    );
}

void LandFlightService::clearSessions(bool notifyPlayers, std::string const& reason) {
    if (mSessions.empty()) {
        return;
    }

    auto onlinePlayers = getOnlinePlayersByXuid();
    for (auto& [xuid, session] : mSessions) {
        auto it = onlinePlayers.find(xuid);
        if (it == onlinePlayers.end()) {
            continue;
        }

        revokePluginFlight(*it->second, session.pluginGrantedMayFly);
        session.activeInLand = false;

        if (notifyPlayers) {
            sendMessage(*it->second, reason);
        }
    }

    mSessions.clear();
}

void LandFlightService::processSecond() {
    auto onlinePlayers = getOnlinePlayersByXuid();
    std::vector<std::string> toRemove;
    toRemove.reserve(mSessions.size());

    for (auto& [xuid, session] : mSessions) {
        auto it = onlinePlayers.find(xuid);
        if (it == onlinePlayers.end()) {
            toRemove.push_back(xuid);
            continue;
        }

        Player& player = *it->second;
        auto    land   = getEligibleLand(player);
        if (!land) {
            if (session.activeInLand || session.pluginGrantedMayFly) {
                revokePluginFlight(player, session.pluginGrantedMayFly);
                session.activeInLand = false;
                sendMessage(player, tr("flight.pause.reason.left_land"));
            }
            continue;
        }

        if (!session.activeInLand) {
            if (session.secondsUntilCharge <= 0) {
                std::string error;
                auto        result = tryCharge(player, error);
                if (result != ChargeResult::Success) {
                    sendMessage(
                        player,
                        result == ChargeResult::NotEnoughMoney ? tr("flight.stop.reason.not_enough_money")
                                                               : tr("flight.stop.reason.charge_failed")
                    );
                    sendMessage(player, error);
                    toRemove.push_back(xuid);
                    continue;
                }

                player.setAbility(AbilitiesIndex::MayFly, true);
                session.pluginGrantedMayFly = true;
                session.activeInLand        = true;
                session.secondsUntilCharge  = getChargeIntervalSeconds();
                sendMessage(
                    player,
                    "flight.entered_land.charged",
                    land->getName(),
                    getChargeAmount(),
                    getChargeIntervalSeconds()
                );
                continue;
            }

            player.setAbility(AbilitiesIndex::MayFly, true);
            session.pluginGrantedMayFly = true;
            session.activeInLand        = true;
            sendMessage(player, "flight.entered_land", land->getName(), session.secondsUntilCharge);
            continue;
        }

        if (session.secondsUntilCharge > 1) {
            --session.secondsUntilCharge;
            continue;
        }

        std::string error;
        auto result = tryCharge(player, error);
        if (result != ChargeResult::Success) {
            revokePluginFlight(player, session.pluginGrantedMayFly);
            session.activeInLand = false;
            sendMessage(
                player,
                result == ChargeResult::NotEnoughMoney ? tr("flight.stop.reason.not_enough_money")
                                                       : tr("flight.stop.reason.charge_failed")
            );
            sendMessage(player, error);
            toRemove.push_back(xuid);
            continue;
        }

        session.secondsUntilCharge = getChargeIntervalSeconds();
        if (config.landFlight.notifyEachCharge && getChargeAmount() > 0) {
            sendMessage(player, "flight.charge.success", getChargeAmount(), getChargeIntervalSeconds(), LLMoney_Get(xuid));
        }
    }

    for (auto const& xuid : toRemove) {
        mSessions.erase(xuid);
    }
}

void LandFlightService::openMainForm(Player& player) {
    ll::form::SimpleForm form{tr("gui.title"), buildMainFormContent(player)};

    if (hasSession(player)) {
        form.appendButton(tr("gui.button.disable"), [this](Player& target) {
            handleDisableCommand(target, true, tr("flight.stop.reason.manual"));
            openMainForm(target);
        });
    } else {
        form.appendButton(tr("gui.button.enable"), [this](Player& target) {
            std::string error;
            if (!enableForPlayer(target, &error)) {
                sendMessage(target, error);
            }
            openMainForm(target);
        });
    }

    form.appendButton(tr("gui.button.status"), [this](Player& target) { handleStatusCommand(target); });
    form.appendButton(tr("gui.button.refresh"), [this](Player& target) { openMainForm(target); });
    form.appendButton(tr("gui.button.close"));
    form.sendTo(player);
}

std::string LandFlightService::buildMainFormContent(Player& player) const {
    auto const land     = getEligibleLand(player);
    auto const landName = land ? land->getName() : tr("gui.land.none");

    auto const balance = LLMoney_Get(player.getXuid());

    if (auto* session = tryGetSession(player); session != nullptr) {
        if (session->activeInLand) {
            return tr(
                "gui.content.enabled",
                landName,
                session->secondsUntilCharge,
                getChargeAmount(),
                getChargeIntervalSeconds(),
                balance
            );
        }

        return tr("gui.content.waiting", landName, getChargeAmount(), getChargeIntervalSeconds(), balance);
    }

    if (land) {
        return tr("gui.content.ready", landName, getChargeAmount(), getChargeIntervalSeconds(), balance);
    }

    return tr("gui.content.disabled", getChargeAmount(), getChargeIntervalSeconds(), balance);
}

bool LandFlightService::enableForPlayer(Player& player, std::string* errorMessage) {
    auto setError = [&](std::string message) {
        if (errorMessage != nullptr) {
            *errorMessage = std::move(message);
        }
    };

    if (!mRunning) {
        setError(tr("command.feature_unavailable"));
        return false;
    }
    if (hasSession(player)) {
        setError(tr("flight.error.already_enabled"));
        return false;
    }
    if (player.canUseAbility(AbilitiesIndex::MayFly)) {
        setError(tr("flight.error.already_can_fly"));
        return false;
    }

    auto& session = mSessions[player.getXuid()];
    session.secondsUntilCharge = config.landFlight.chargeOnStart ? 0 : getChargeIntervalSeconds();
    session.pluginGrantedMayFly = false;
    session.activeInLand        = false;

    auto land = getEligibleLand(player);
    if (!land) {
        sendMessage(player, "flight.enabled_waiting", getChargeAmount(), getChargeIntervalSeconds());
        return true;
    }

    if (session.secondsUntilCharge <= 0) {
        std::string error;
        if (tryCharge(player, error) != ChargeResult::Success) {
            mSessions.erase(player.getXuid());
            setError(error);
            return false;
        }

        player.setAbility(AbilitiesIndex::MayFly, true);
        session.pluginGrantedMayFly = true;
        session.activeInLand        = true;
        session.secondsUntilCharge  = getChargeIntervalSeconds();
        sendMessage(player, "flight.started.charged", land->getName(), getChargeAmount(), getChargeIntervalSeconds());
        return true;
    }

    player.setAbility(AbilitiesIndex::MayFly, true);
    session.pluginGrantedMayFly = true;
    session.activeInLand        = true;
    sendMessage(player, "flight.started", land->getName(), getChargeAmount(), getChargeIntervalSeconds());
    return true;
}

void LandFlightService::handleStatusCommand(Player& player) const {
    auto land = getEligibleLand(player);
    if (auto* session = tryGetSession(player); session != nullptr) {
        if (session->activeInLand) {
            sendMessage(
                player,
                "flight.status.enabled",
                land ? land->getName() : tr("flight.status.no_land"),
                session->secondsUntilCharge,
                getChargeAmount(),
                getChargeIntervalSeconds()
            );
        } else {
            sendMessage(player, "flight.status.waiting", getChargeAmount(), getChargeIntervalSeconds());
        }
        return;
    }

    if (land) {
        sendMessage(player, "flight.status.ready", land->getName(), getChargeAmount(), getChargeIntervalSeconds());
    } else {
        sendMessage(player, "flight.status.disabled", getChargeAmount(), getChargeIntervalSeconds());
    }
}

void LandFlightService::handleDisableCommand(Player& player, bool notifyPlayer, std::string const& reason) {
    auto* session = tryGetSession(player);
    if (session == nullptr) {
        return;
    }

    revokePluginFlight(player, session->pluginGrantedMayFly);
    session->activeInLand = false;

    mSessions.erase(player.getXuid());

    if (notifyPlayer) {
        sendMessage(player, reason);
    }
}

bool LandFlightService::hasSession(Player const& player) const { return mSessions.contains(player.getXuid()); }

LandFlightService::FlightSession* LandFlightService::tryGetSession(Player const& player) {
    auto it = mSessions.find(player.getXuid());
    return it == mSessions.end() ? nullptr : &it->second;
}

LandFlightService::FlightSession const* LandFlightService::tryGetSession(Player const& player) const {
    auto it = mSessions.find(player.getXuid());
    return it == mSessions.end() ? nullptr : &it->second;
}

int LandFlightService::getChargeIntervalSeconds() const { return std::max(1, config.landFlight.chargeIntervalSeconds); }

long long LandFlightService::getChargeAmount() const { return std::max(0LL, config.landFlight.chargeAmount); }

} // namespace my_mod
