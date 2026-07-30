#pragma once

#include "smave/ir.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace smave {

[[nodiscard]] ModelIR compile_model(
    const std::filesystem::path& source,
    const std::optional<std::string>& top = std::nullopt);

}  // namespace smave

