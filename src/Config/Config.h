#pragma once

#include "ll/api/io/LogLevel.h"
#include <string>

namespace my_mod {

struct LandFlightConfig {
    bool        enabled               = true;
    std::string command               = "plandfly";
    std::string alias                 = "pfly";
    long long   chargeAmount          = 10;
    int         chargeIntervalSeconds = 10;
    bool        chargeOnStart         = true;
    bool        requireLandMember     = true;
    bool        notifyEachCharge      = true;
};

struct Config {
    int              version                  = 2;
    ll::io::LogLevel logLevel                 = ll::io::LogLevel::Info;
    std::string      language                 = "zh_CN";
    LandFlightConfig landFlight{};
};

} // namespace my_mod
