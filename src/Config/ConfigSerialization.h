#pragma once

#include "Config.h"

#include <nlohmann/json.hpp>

namespace my_mod {

namespace detail {

template <typename T>
void getConfigValue(nlohmann::json const& json, char const* name, T& value, T const& defaultValue) {
    auto const it = json.find(name);
    if (it == json.end()) {
        value = defaultValue;
        return;
    }

    it->get_to(value);
}

} // namespace detail

inline void to_json(nlohmann::json& json, LandFlightConfig const& value) {
    json = nlohmann::json::object();
    json["enabled"]               = value.enabled;
    json["command"]               = value.command;
    json["alias"]                 = value.alias;
    json["useEconomy"]            = value.useEconomy;
    json["chargeAmount"]          = value.chargeAmount;
    json["chargeIntervalSeconds"] = value.chargeIntervalSeconds;
    json["chargeOnStart"]         = value.chargeOnStart;
    json["requireLandMember"]     = value.requireLandMember;
    json["notifyEachCharge"]      = value.notifyEachCharge;
}

inline void from_json(nlohmann::json const& json, LandFlightConfig& value) {
    LandFlightConfig const defaults{};
    detail::getConfigValue(json, "enabled", value.enabled, defaults.enabled);
    detail::getConfigValue(json, "command", value.command, defaults.command);
    detail::getConfigValue(json, "alias", value.alias, defaults.alias);
    detail::getConfigValue(json, "useEconomy", value.useEconomy, defaults.useEconomy);
    detail::getConfigValue(json, "chargeAmount", value.chargeAmount, defaults.chargeAmount);
    detail::getConfigValue(
        json,
        "chargeIntervalSeconds",
        value.chargeIntervalSeconds,
        defaults.chargeIntervalSeconds
    );
    detail::getConfigValue(json, "chargeOnStart", value.chargeOnStart, defaults.chargeOnStart);
    detail::getConfigValue(json, "requireLandMember", value.requireLandMember, defaults.requireLandMember);
    detail::getConfigValue(json, "notifyEachCharge", value.notifyEachCharge, defaults.notifyEachCharge);
}

inline void to_json(nlohmann::json& json, Config const& value) {
    json = nlohmann::json::object();
    json["version"]    = value.version;
    json["logLevel"]   = value.logLevel;
    json["language"]   = value.language;
    json["landFlight"] = value.landFlight;
}

inline void from_json(nlohmann::json const& json, Config& value) {
    Config const defaults{};
    detail::getConfigValue(json, "version", value.version, defaults.version);
    detail::getConfigValue(json, "logLevel", value.logLevel, defaults.logLevel);
    detail::getConfigValue(json, "language", value.language, defaults.language);
    detail::getConfigValue(json, "landFlight", value.landFlight, defaults.landFlight);
}

} // namespace my_mod
