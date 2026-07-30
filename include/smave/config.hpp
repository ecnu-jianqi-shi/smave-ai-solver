#pragma once

#include "smave/routing.hpp"
#include "smave/runtime.hpp"

#include <filesystem>
#include <string>

namespace smave {

inline constexpr const char* kConfigSchemaVersion = "smave.config.v1";

struct RuntimeConfig {
    std::string schema_version{kConfigSchemaVersion};
    Tolerance tolerance;
    RoutingConfig routing;
    std::string ood_policy{"fallback"};
    std::string event_policy{"original"};
    bool online_learning{false};
    bool trace{true};
    std::string retain_context{"redacted"};

    void validate() const;
    static RuntimeConfig read(const std::filesystem::path& path);
};

}  // namespace smave

