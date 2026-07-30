#include "smave/config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace smave {
namespace {

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    const auto last = value.find_last_not_of(" \t\r\n");
    return first == std::string::npos ? std::string{} : value.substr(first, last - first + 1);
}

bool parse_bool(const std::string& value) {
    if (value == "true") return true;
    if (value == "false") return false;
    throw std::invalid_argument("configuration boolean must be true or false: " + value);
}

std::map<std::string, std::string> parse_yaml_subset(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read configuration: " + path.string());
    std::map<std::string, std::string> values;
    std::vector<std::string> sections;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const auto comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        if (trim(line).empty()) continue;
        const auto first = line.find_first_not_of(' ');
        if (first != std::string::npos && first % 2 != 0) {
            throw std::invalid_argument(
                "configuration indentation must use two spaces at line " +
                std::to_string(line_number));
        }
        if (line.find('\t') != std::string::npos) {
            throw std::invalid_argument("configuration tabs are not allowed");
        }
        const std::size_t depth = first == std::string::npos ? 0 : first / 2;
        const std::string content = trim(line);
        const auto separator = content.find(':');
        if (separator == std::string::npos) {
            throw std::invalid_argument(
                "configuration entry lacks ':' at line " + std::to_string(line_number));
        }
        const std::string key = trim(content.substr(0, separator));
        const std::string value = trim(content.substr(separator + 1));
        if (key.empty()) throw std::invalid_argument("configuration key is empty");
        if (depth > sections.size()) {
            throw std::invalid_argument("configuration indentation skips a level");
        }
        sections.resize(depth);
        if (value.empty()) {
            sections.push_back(key);
            continue;
        }
        std::string path_key;
        for (const auto& section : sections) {
            if (!path_key.empty()) path_key += '.';
            path_key += section;
        }
        if (!path_key.empty()) path_key += '.';
        path_key += key;
        if (!values.emplace(path_key, value).second) {
            throw std::invalid_argument("duplicate configuration field: " + path_key);
        }
    }
    return values;
}

}  // namespace

void RuntimeConfig::validate() const {
    if (schema_version != kConfigSchemaVersion) {
        throw std::invalid_argument("unsupported configuration schema: " + schema_version);
    }
    if (!(tolerance.absolute > 0.0) || !(tolerance.relative > 0.0) ||
        !(tolerance.qoi_relative > 0.0)) {
        throw std::invalid_argument("all tolerances must be positive");
    }
    if (tolerance.qoi_relative > 1.0e-4) {
        throw std::invalid_argument("QoI relative tolerance cannot exceed 0.01%");
    }
    if (routing.top_k == 0) throw std::invalid_argument("routing.top_k must be positive");
    if (routing.minimum_pass_probability < 0.0 ||
        routing.minimum_pass_probability > 1.0) {
        throw std::invalid_argument("routing minimum pass probability must be in [0,1]");
    }
    if (!routing.require_original_fallback) {
        throw std::invalid_argument("configuration cannot disable original fallback");
    }
    if (ood_policy != "fallback") {
        throw std::invalid_argument("only safety.ood_policy=fallback is supported");
    }
    if (event_policy != "original") {
        throw std::invalid_argument("only safety.event_policy=original is supported");
    }
    if (online_learning) {
        throw std::invalid_argument("online learning is forbidden in the runtime data plane");
    }
    if (!trace) {
        throw std::invalid_argument("runtime audit trace cannot be disabled in this release");
    }
    if (retain_context != "redacted" && retain_context != "none") {
        throw std::invalid_argument("telemetry.retain_context must be redacted or none");
    }
}

RuntimeConfig RuntimeConfig::read(const std::filesystem::path& path) {
    const auto values = parse_yaml_subset(path);
    static const std::set<std::string> known{
        "schema_version",
        "tolerance.residual.relative",
        "tolerance.residual.absolute",
        "tolerance.qoi.default_relative",
        "routing.top_k",
        "routing.min_pass_probability",
        "routing.risk_weight",
        "routing.require_original_fallback",
        "safety.ood_policy",
        "safety.event_policy",
        "safety.online_learning",
        "telemetry.trace",
        "telemetry.retain_context",
    };
    for (const auto& __entry : values) {

        const auto& key = __entry.first;

        const auto& _ = __entry.second;
        if (!known.contains(key)) throw std::invalid_argument("unknown configuration field: " + key);
    }
    const auto schema = values.find("schema_version");
    if (schema == values.end()) throw std::invalid_argument("configuration schema_version is required");
    RuntimeConfig config;
    config.schema_version = schema->second;
    auto assign_double = [&](const std::string& key, double& target) {
        if (const auto item = values.find(key); item != values.end()) target = std::stod(item->second);
    };
    assign_double("tolerance.residual.relative", config.tolerance.relative);
    assign_double("tolerance.residual.absolute", config.tolerance.absolute);
    assign_double("tolerance.qoi.default_relative", config.tolerance.qoi_relative);
    assign_double("routing.min_pass_probability", config.routing.minimum_pass_probability);
    assign_double("routing.risk_weight", config.routing.risk_weight);
    if (const auto item = values.find("routing.top_k"); item != values.end()) {
        config.routing.top_k = std::stoul(item->second);
    }
    if (const auto item = values.find("routing.require_original_fallback"); item != values.end()) {
        config.routing.require_original_fallback = parse_bool(item->second);
    }
    if (const auto item = values.find("safety.ood_policy"); item != values.end()) {
        config.ood_policy = item->second;
    }
    if (const auto item = values.find("safety.event_policy"); item != values.end()) {
        config.event_policy = item->second;
    }
    if (const auto item = values.find("safety.online_learning"); item != values.end()) {
        config.online_learning = parse_bool(item->second);
    }
    if (const auto item = values.find("telemetry.trace"); item != values.end()) {
        config.trace = parse_bool(item->second);
    }
    if (const auto item = values.find("telemetry.retain_context"); item != values.end()) {
        config.retain_context = item->second;
    }
    config.validate();
    return config;
}

}  // namespace smave
