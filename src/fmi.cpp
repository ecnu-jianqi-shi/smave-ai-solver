#include "smave/fmi.hpp"
#include "smave/ssp.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace smave {
namespace {

constexpr std::size_t kMaximumArchiveSize = 512U * 1024U * 1024U;
constexpr std::size_t kMaximumDescriptorSize = 16U * 1024U * 1024U;

std::string digest(std::string_view input) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char value : input) {
        hash ^= value;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << hash;
    return output.str();
}

std::string hexadecimal(const std::vector<std::uint8_t>& bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(bytes.size() * 2, '0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        result[index * 2] = digits[bytes[index] >> 4U];
        result[index * 2 + 1] = digits[bytes[index] & 0x0fU];
    }
    return result;
}

std::string read_text(const std::filesystem::path& path, std::size_t maximum_size) {
    const auto size = std::filesystem::file_size(path);
    if (size > maximum_size) throw std::runtime_error("file exceeds import size limit");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read FMI input: " + path.string());
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

std::uint16_t little_u16(const std::string& bytes, std::size_t offset) {
    if (offset + 2 > bytes.size()) throw std::runtime_error("truncated FMU ZIP field");
    return static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[offset])) |
        static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8U;
}

std::uint32_t little_u32(const std::string& bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) throw std::runtime_error("truncated FMU ZIP field");
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset])) |
        static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8U |
        static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 2])) << 16U |
        static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 3])) << 24U;
}

bool safe_archive_path(const std::string& path) {
    if (path.empty() || path.front() == '/' || path.front() == '\\') return false;
    if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) &&
        path[1] == ':') return false;
    std::filesystem::path parsed(path);
    for (const auto& part : parsed) {
        if (part == "..") return false;
    }
    return path.find('\\') == std::string::npos;
}

struct ZipEntry {
    std::string name;
    std::uint16_t flags{0};
    std::uint16_t method{0};
    std::uint32_t crc{0};
    std::uint32_t compressed_size{0};
    std::uint32_t uncompressed_size{0};
    std::uint32_t local_offset{0};
};

std::vector<ZipEntry> zip_entries(const std::string& archive) {
    constexpr std::uint32_t end_signature = 0x06054b50U;
    constexpr std::uint32_t central_signature = 0x02014b50U;
    const std::size_t search_start = archive.size() > 65557U
        ? archive.size() - 65557U
        : 0U;
    std::size_t end_offset = std::string::npos;
    for (std::size_t offset = archive.size() >= 22U ? archive.size() - 22U : 0U;; --offset) {
        if (little_u32(archive, offset) == end_signature) {
            end_offset = offset;
            break;
        }
        if (offset == search_start) break;
    }
    if (end_offset == std::string::npos) throw std::invalid_argument("FMU is not a ZIP archive");
    const auto disk = little_u16(archive, end_offset + 4);
    const auto central_disk = little_u16(archive, end_offset + 6);
    const auto disk_entries = little_u16(archive, end_offset + 8);
    const auto total_entries = little_u16(archive, end_offset + 10);
    const auto central_size = little_u32(archive, end_offset + 12);
    const auto central_offset = little_u32(archive, end_offset + 16);
    if (disk != 0 || central_disk != 0 || disk_entries != total_entries) {
        throw std::invalid_argument("multi-disk FMU ZIP archives are unsupported");
    }
    if (total_entries == 0xffffU || central_size == 0xffffffffU ||
        central_offset == 0xffffffffU) {
        throw std::invalid_argument("ZIP64 FMU archives are unsupported");
    }
    if (static_cast<std::size_t>(central_offset) + central_size > archive.size()) {
        throw std::invalid_argument("invalid FMU ZIP central directory");
    }
    std::vector<ZipEntry> result;
    std::unordered_set<std::string> names;
    std::size_t offset = central_offset;
    for (std::size_t index = 0; index < total_entries; ++index) {
        if (little_u32(archive, offset) != central_signature) {
            throw std::invalid_argument("invalid FMU ZIP central entry");
        }
        ZipEntry entry;
        entry.flags = little_u16(archive, offset + 8);
        entry.method = little_u16(archive, offset + 10);
        entry.crc = little_u32(archive, offset + 16);
        entry.compressed_size = little_u32(archive, offset + 20);
        entry.uncompressed_size = little_u32(archive, offset + 24);
        const auto name_size = little_u16(archive, offset + 28);
        const auto extra_size = little_u16(archive, offset + 30);
        const auto comment_size = little_u16(archive, offset + 32);
        entry.local_offset = little_u32(archive, offset + 42);
        const auto next = offset + 46U + name_size + extra_size + comment_size;
        if (next > archive.size()) throw std::runtime_error("truncated FMU ZIP entry");
        entry.name = archive.substr(offset + 46U, name_size);
        if (!safe_archive_path(entry.name)) {
            throw std::invalid_argument("unsafe FMU archive path: " + entry.name);
        }
        if (!names.insert(entry.name).second) {
            throw std::invalid_argument("duplicate FMU archive path: " + entry.name);
        }
        if ((entry.flags & 0x1U) != 0U) {
            throw std::invalid_argument("encrypted FMU ZIP entries are unsupported");
        }
        if (entry.method != 0U && entry.method != 8U) {
            throw std::invalid_argument("unsupported FMU ZIP compression method");
        }
        result.push_back(std::move(entry));
        offset = next;
    }
    return result;
}

std::string extract_zip_entry(
    const std::string& archive,
    const ZipEntry& entry,
    std::size_t maximum_size = kMaximumDescriptorSize) {
    constexpr std::uint32_t local_signature = 0x04034b50U;
    if (little_u32(archive, entry.local_offset) != local_signature) {
        throw std::invalid_argument("invalid FMU ZIP local entry");
    }
    const auto name_size = little_u16(archive, entry.local_offset + 26);
    const auto extra_size = little_u16(archive, entry.local_offset + 28);
    const std::size_t data_offset = entry.local_offset + 30U + name_size + extra_size;
    if (entry.local_offset + 30U + name_size > archive.size() ||
        archive.substr(entry.local_offset + 30U, name_size) != entry.name) {
        throw std::invalid_argument("FMU ZIP local/central path mismatch");
    }
    if (data_offset + entry.compressed_size > archive.size() ||
        entry.uncompressed_size > maximum_size) {
        throw std::runtime_error("FMU descriptor exceeds extraction limit");
    }
    std::string result(entry.uncompressed_size, '\0');
    if (entry.method == 0U) {
        if (entry.compressed_size != entry.uncompressed_size) {
            throw std::invalid_argument("invalid stored FMU ZIP entry size");
        }
        result.assign(archive.data() + data_offset, entry.uncompressed_size);
    } else {
        z_stream stream{};
        stream.next_in = reinterpret_cast<Bytef*>(
            const_cast<char*>(archive.data() + data_offset));
        stream.avail_in = entry.compressed_size;
        stream.next_out = reinterpret_cast<Bytef*>(result.data());
        stream.avail_out = entry.uncompressed_size;
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            throw std::runtime_error("cannot initialize FMU ZIP inflater");
        }
        const int status = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);
        if (status != Z_STREAM_END || stream.total_out != entry.uncompressed_size) {
            throw std::invalid_argument("invalid deflated FMU ZIP entry");
        }
    }
    const auto actual_crc = crc32(
        0U, reinterpret_cast<const Bytef*>(result.data()),
        static_cast<uInt>(result.size()));
    if (actual_crc != entry.crc) throw std::invalid_argument("FMU descriptor CRC mismatch");
    return result;
}

std::string decode_entities(std::string value) {
    const std::array<std::pair<std::string_view, std::string_view>, 5> entities{{
        {"&quot;", "\""}, {"&apos;", "'"}, {"&lt;", "<"},
        {"&gt;", ">"}, {"&amp;", "&"}}};
    for (const auto& [encoded, decoded] : entities) {
        std::size_t position{};
        while ((position = value.find(encoded, position)) != std::string::npos) {
            value.replace(position, encoded.size(), decoded);
            position += decoded.size();
        }
    }
    return value;
}

struct XmlTag {
    std::string name;
    std::map<std::string, std::string> attributes;
    bool closing{false};
    bool self_closing{false};
};

std::vector<XmlTag> xml_tags(const std::string& xml) {
    std::vector<XmlTag> result;
    std::size_t position{};
    while ((position = xml.find('<', position)) != std::string::npos) {
        if (xml.compare(position, 4, "<!--") == 0) {
            const auto end = xml.find("-->", position + 4);
            if (end == std::string::npos) throw std::invalid_argument("unterminated XML comment");
            position = end + 3;
            continue;
        }
        if (position + 1 < xml.size() && (xml[position + 1] == '?' || xml[position + 1] == '!')) {
            const auto end = xml.find('>', position + 2);
            if (end == std::string::npos) throw std::invalid_argument("unterminated XML declaration");
            position = end + 1;
            continue;
        }
        const auto end = xml.find('>', position + 1);
        if (end == std::string::npos) throw std::invalid_argument("unterminated XML tag");
        std::string body = xml.substr(position + 1, end - position - 1);
        XmlTag tag;
        if (!body.empty() && body.front() == '/') {
            tag.closing = true;
            body.erase(body.begin());
        }
        while (!body.empty() && std::isspace(static_cast<unsigned char>(body.back()))) body.pop_back();
        if (!tag.closing && !body.empty() && body.back() == '/') {
            tag.self_closing = true;
            body.pop_back();
        }
        std::size_t cursor{};
        while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor]))) ++cursor;
        const auto name_begin = cursor;
        while (cursor < body.size() && !std::isspace(static_cast<unsigned char>(body[cursor]))) ++cursor;
        tag.name = body.substr(name_begin, cursor - name_begin);
        while (!tag.closing && cursor < body.size()) {
            while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor]))) ++cursor;
            if (cursor == body.size()) break;
            const auto key_begin = cursor;
            while (cursor < body.size() && body[cursor] != '=' &&
                   !std::isspace(static_cast<unsigned char>(body[cursor]))) ++cursor;
            const auto key = body.substr(key_begin, cursor - key_begin);
            while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor]))) ++cursor;
            if (cursor == body.size() || body[cursor] != '=') {
                throw std::invalid_argument("malformed XML attribute");
            }
            ++cursor;
            while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor]))) ++cursor;
            if (cursor == body.size() || (body[cursor] != '\'' && body[cursor] != '\"')) {
                throw std::invalid_argument("XML attribute must be quoted");
            }
            const char quote = body[cursor++];
            const auto value_begin = cursor;
            const auto value_end = body.find(quote, cursor);
            if (value_end == std::string::npos) throw std::invalid_argument("unterminated XML attribute");
            tag.attributes.emplace(key, decode_entities(body.substr(value_begin, value_end - value_begin)));
            cursor = value_end + 1;
        }
        if (!tag.name.empty()) result.push_back(std::move(tag));
        position = end + 1;
    }
    return result;
}

std::string attribute(
    const std::map<std::string, std::string>& attributes,
    const std::string& name,
    const std::string& fallback = {}) {
    const auto iterator = attributes.find(name);
    return iterator == attributes.end() ? fallback : iterator->second;
}

std::uint64_t unsigned_value(const std::string& value, const std::string& purpose) {
    if (value.empty()) throw std::invalid_argument("missing " + purpose);
    std::size_t parsed{};
    const auto result = std::stoull(value, &parsed);
    if (parsed != value.size()) throw std::invalid_argument("invalid " + purpose);
    return result;
}

std::string host_platform() {
#if defined(__APPLE__) && defined(__aarch64__)
    return "aarch64-darwin";
#elif defined(__APPLE__) && defined(__x86_64__)
    return "x86_64-darwin";
#elif defined(__linux__) && defined(__aarch64__)
    return "aarch64-linux";
#elif defined(__linux__) && defined(__x86_64__)
    return "x86_64-linux";
#elif defined(_WIN32) && defined(_M_ARM64)
    return "aarch64-windows";
#elif defined(_WIN32) && defined(_M_X64)
    return "x86_64-windows";
#else
    return "unknown";
#endif
}

bool is_fmi3_variable(const std::string& name) {
    static const std::unordered_set<std::string> types{
        "Float32", "Float64", "Int8", "UInt8", "Int16", "UInt16",
        "Int32", "UInt32", "Int64", "UInt64", "Boolean", "String",
        "Binary", "Enumeration", "Clock"};
    return types.contains(name);
}

FmiVariableIR variable_from_attributes(
    const std::string& type,
    const std::map<std::string, std::string>& attributes) {
    FmiVariableIR variable;
    variable.name = attribute(attributes, "name");
    variable.type = type;
    variable.value_reference = unsigned_value(attribute(attributes, "valueReference"), "valueReference");
    variable.causality = attribute(attributes, "causality", "local");
    variable.variability = attribute(attributes, "variability", "continuous");
    variable.initial = attribute(attributes, "initial");
    variable.unit = attribute(attributes, "unit");
    variable.start = attribute(attributes, "start");
    if (type == "Clock") {
        static const std::unordered_set<std::string> interval_variabilities{
            "constant", "fixed", "tunable", "changing", "countdown", "triggered"};
        const auto interval_variability = attribute(attributes, "intervalVariability");
        if (!interval_variabilities.contains(interval_variability)) {
            throw std::invalid_argument(
                "FMI 3 Clock requires a valid intervalVariability");
        }
        const auto priority = attribute(attributes, "priority");
        if (!priority.empty()) {
            const auto value = unsigned_value(priority, "Clock priority");
            if (value > std::numeric_limits<std::uint32_t>::max()) {
                throw std::invalid_argument("FMI 3 Clock priority exceeds UInt32 range");
            }
            variable.clock_priority = static_cast<std::uint32_t>(value);
        }
    }
    return variable;
}

void parse_model_description(const std::string& xml, FmiBlackboxIR& model) {
    const auto tags = xml_tags(xml);
    if (tags.empty() || tags.front().name != "fmiModelDescription" || tags.front().closing) {
        throw std::invalid_argument("missing fmiModelDescription root");
    }
    const auto& root = tags.front().attributes;
    model.fmi_version = attribute(root, "fmiVersion");
    model.model_name = attribute(root, "modelName");
    model.instantiation_token = model.fmi_version.starts_with("2.")
        ? attribute(root, "guid")
        : attribute(root, "instantiationToken");
    model.generation_tool = attribute(root, "generationTool");
    model.generation_date_time = attribute(root, "generationDateAndTime");
    model.variable_naming_convention = attribute(root, "variableNamingConvention", "flat");
    const auto indicators = attribute(root, "numberOfEventIndicators", "0");
    model.number_of_event_indicators = static_cast<std::size_t>(
        unsigned_value(indicators, "numberOfEventIndicators"));

    bool in_model_variables{};
    bool in_model_structure{};
    bool in_derivatives{};
    bool in_scalar_variable{};
    bool in_fmi3_variable{};
    FmiVariableIR scalar;
    std::unordered_set<std::string> variable_names;
    for (const auto& tag : tags) {
        if (tag.closing) {
            if (tag.name == "ModelVariables") in_model_variables = false;
            if (tag.name == "ModelStructure") in_model_structure = false;
            if (tag.name == "Derivatives") in_derivatives = false;
            if (tag.name == "ScalarVariable" && in_scalar_variable) {
                if (scalar.type.empty()) throw std::invalid_argument("FMI 2 ScalarVariable has no type");
                if (!variable_names.insert(scalar.name).second) {
                    throw std::invalid_argument("duplicate FMI variable name: " + scalar.name);
                }
                model.variables.push_back(std::move(scalar));
                scalar = {};
                in_scalar_variable = false;
            }
            if (is_fmi3_variable(tag.name) && in_fmi3_variable) {
                if (!variable_names.insert(scalar.name).second) {
                    throw std::invalid_argument("duplicate FMI variable name: " + scalar.name);
                }
                model.variables.push_back(std::move(scalar));
                scalar = {};
                in_fmi3_variable = false;
            }
            continue;
        }
        if (tag.name == "DefaultExperiment") model.default_experiment = tag.attributes;
        if (tag.name == "ModelExchange" || tag.name == "CoSimulation" ||
            tag.name == "ScheduledExecution") {
            FmiInterfaceIR interface;
            interface.kind = tag.name;
            interface.model_identifier = attribute(tag.attributes, "modelIdentifier");
            interface.capabilities = tag.attributes;
            interface.capabilities.erase("modelIdentifier");
            model.interfaces.push_back(std::move(interface));
        }
        if (tag.name == "ModelVariables") {
            in_model_variables = true;
            continue;
        }
        if (tag.name == "ModelStructure") {
            in_model_structure = true;
            continue;
        }
        if (in_model_structure && tag.name == "Derivatives") {
            in_derivatives = true;
            continue;
        }
        if (in_derivatives && tag.name == "Unknown") {
            model.derivative_variable_order.push_back(static_cast<std::size_t>(
                unsigned_value(attribute(tag.attributes, "index"),
                               "ModelStructure Derivatives index")));
            continue;
        }
        if (!in_model_variables) continue;
        if (model.fmi_version.starts_with("2.")) {
            if (tag.name == "ScalarVariable") {
                scalar = variable_from_attributes("", tag.attributes);
                in_scalar_variable = true;
            } else if (in_scalar_variable &&
                (tag.name == "Real" || tag.name == "Integer" || tag.name == "Boolean" ||
                 tag.name == "String" || tag.name == "Enumeration")) {
                scalar.type = tag.name;
                scalar.unit = attribute(tag.attributes, "unit");
                scalar.start = attribute(tag.attributes, "start");
                const auto derivative = attribute(tag.attributes, "derivative");
                if (!derivative.empty()) {
                    scalar.derivative_of = static_cast<std::size_t>(
                        unsigned_value(derivative, "derivative"));
                }
            } else if (in_scalar_variable && tag.name == "Dimension") {
                throw std::invalid_argument("FMI 2 variables cannot declare FMI 3 dimensions");
            }
            if (tag.name == "ScalarVariable" && tag.self_closing) {
                throw std::invalid_argument("FMI 2 ScalarVariable has no nested type");
            }
        } else if (is_fmi3_variable(tag.name)) {
            scalar = variable_from_attributes(tag.name, tag.attributes);
            if (tag.self_closing) {
                if (!variable_names.insert(scalar.name).second) {
                    throw std::invalid_argument("duplicate FMI variable name: " + scalar.name);
                }
                model.variables.push_back(std::move(scalar));
                scalar = {};
            } else {
                in_fmi3_variable = true;
            }
        } else if (in_fmi3_variable && tag.name == "Dimension") {
            if (scalar.type == "Clock") {
                throw std::invalid_argument("FMI 3 Clock variables must be scalar");
            }
            const auto start = attribute(tag.attributes, "start");
            const auto value_reference = attribute(tag.attributes, "valueReference");
            if (start.empty() == value_reference.empty()) {
                throw std::invalid_argument(
                    "FMI 3 Dimension requires exactly one of start or valueReference");
            }
            FmiDimensionIR dimension;
            if (!start.empty()) {
                const auto extent = unsigned_value(start, "Dimension start");
                if (extent == 0 || extent > std::numeric_limits<std::size_t>::max()) {
                    throw std::invalid_argument("FMI 3 Dimension start is out of range");
                }
                dimension.fixed_extent = static_cast<std::size_t>(extent);
            } else {
                dimension.extent_value_reference =
                    unsigned_value(value_reference, "Dimension valueReference");
            }
            scalar.dimension_descriptors.push_back(std::move(dimension));
            scalar.dimensions = scalar.dimension_descriptors.size();
        }
    }
    if (in_scalar_variable || in_fmi3_variable) {
        throw std::invalid_argument("unterminated FMI variable");
    }
}

std::vector<std::string> inventory_directory(const std::filesystem::path& directory) {
    std::vector<std::string> result;
    for (const auto& item : std::filesystem::recursive_directory_iterator(directory)) {
        if (!item.is_regular_file()) continue;
        auto relative = std::filesystem::relative(item.path(), directory).generic_string();
        if (!safe_archive_path(relative)) throw std::invalid_argument("unsafe FMI directory path");
        result.push_back(std::move(relative));
    }
    std::sort(result.begin(), result.end());
    return result;
}

void derive_binary_inventory(FmiBlackboxIR& model) {
    std::set<std::string> platforms;
    for (const auto& path : model.archive_entries) {
        if (!path.starts_with("binaries/")) continue;
        const auto end = path.find('/', 9);
        if (end != std::string::npos) platforms.insert(path.substr(9, end - 9));
    }
    model.binary_platforms.assign(platforms.begin(), platforms.end());
    std::vector<std::string> host_directories{model.host_platform};
    if (model.fmi_version.starts_with("2.")) {
        const std::string legacy = model.host_platform.ends_with("darwin")
            ? "darwin64"
            : model.host_platform.ends_with("linux") ? "linux64"
            : model.host_platform.ends_with("windows") ? "win64" : "";
        if (!legacy.empty()) host_directories.push_back(legacy);
    }
    const std::string extension = model.host_platform.ends_with("darwin")
        ? ".dylib"
        : model.host_platform.ends_with("linux") ? ".so"
        : model.host_platform.ends_with("windows") ? ".dll" : "";
    model.host_binary_candidate_available = !extension.empty();
    for (const auto& interface : model.interfaces) {
        bool found{};
        for (const auto& directory : host_directories) {
            const auto expected = "binaries/" + directory + "/" +
                interface.model_identifier + extension;
            found = found || std::binary_search(
                model.archive_entries.begin(), model.archive_entries.end(), expected);
        }
        model.host_binary_candidate_available =
            model.host_binary_candidate_available && found;
    }
    if (!model.host_binary_candidate_available) {
        model.warnings.push_back(
            "no binary candidate for host platform; metadata-only import remains available");
    }
}

void derive_capability_warnings(FmiBlackboxIR& model) {
    const auto capability = [](const FmiInterfaceIR& interface, const std::string& name) {
        const auto iterator = interface.capabilities.find(name);
        return iterator != interface.capabilities.end() && iterator->second == "true";
    };
    for (const auto& interface : model.interfaces) {
        const std::string state_capability = model.fmi_version.starts_with("2.")
            ? "canGetAndSetFMUstate"
            : "canGetAndSetFMUState";
        if (!capability(interface, state_capability)) {
            model.warnings.push_back(
                interface.kind + ": state save/restore is unavailable; rollback and replay require external restart");
        }
        const std::string serialization_capability = model.fmi_version.starts_with("2.")
            ? "canSerializeFMUstate"
            : "canSerializeFMUState";
        if (!capability(interface, serialization_capability)) {
            model.warnings.push_back(
                interface.kind +
                ": serialized FMU state is unavailable; process-independent state transfer is disabled");
        }
        const std::string directional = model.fmi_version.starts_with("2.")
            ? "providesDirectionalDerivative"
            : "providesDirectionalDerivatives";
        if (!capability(interface, directional)) {
            model.warnings.push_back(
                interface.kind + ": directional derivatives are unavailable; derivative-assisted training is disabled");
        }
        if (interface.kind == "CoSimulation" &&
            !capability(interface, "canHandleVariableCommunicationStepSize")) {
            model.warnings.push_back(
                "CoSimulation: variable communication steps are unavailable; a fixed communication grid is required");
        }
        if (interface.kind == "ScheduledExecution") {
            model.warnings.push_back(
                "ScheduledExecution native execution is opt-in and limited to deterministic periodic input Clocks with static priority ordering but without dependency graphs, concurrency, or preemption scheduling");
        }
    }
}

void require_tag(std::istream& input, std::string_view expected) {
    std::string tag;
    input >> tag;
    if (tag != expected) {
        throw std::invalid_argument(
            "expected FMI IR tag " + std::string(expected) +
            ", got " + (tag.empty() ? "<eof>" : tag));
    }
}

using Fmi3Instance = void*;
using Fmi3InstanceEnvironment = void*;
using Fmi3FmuState = void*;
using Fmi3ValueReference = std::uint32_t;
using Fmi3Float32 = float;
using Fmi3Float64 = double;
using Fmi3Int8 = std::int8_t;
using Fmi3UInt8 = std::uint8_t;
using Fmi3Int16 = std::int16_t;
using Fmi3UInt16 = std::uint16_t;
using Fmi3Int32 = std::int32_t;
using Fmi3UInt32 = std::uint32_t;
using Fmi3Int64 = std::int64_t;
using Fmi3UInt64 = std::uint64_t;
using Fmi3Boolean = bool;
using Fmi3Clock = bool;
using Fmi3String = const char*;

enum class Fmi3Status { ok, warning, discard, error, fatal };
enum class Fmi3IntervalQualifier { not_yet_known, unchanged, changed };

using Fmi2Component = void*;
using Fmi2ComponentEnvironment = void*;
using Fmi2FmuState = void*;
using Fmi2ValueReference = std::uint32_t;
using Fmi2Real = double;
using Fmi2Integer = int;
using Fmi2Boolean = int;
using Fmi2String = const char*;
enum class Fmi2Status { ok, warning, discard, error, fatal, pending };
enum class Fmi2StatusKind {
    do_step_status,
    pending_status,
    last_successful_time,
    terminated
};

enum class Fmi2Type { model_exchange, co_simulation };

using Fmi2CallbackLogger = void (*)(
    Fmi2ComponentEnvironment, Fmi2String, Fmi2Status, Fmi2String,
    Fmi2String, ...);
using Fmi2CallbackAllocateMemory = void* (*)(std::size_t, std::size_t);
using Fmi2CallbackFreeMemory = void (*)(void*);
using Fmi2StepFinished = void (*)(Fmi2ComponentEnvironment, Fmi2Status);

struct Fmi2CallbackFunctions {
    Fmi2CallbackLogger logger{};
    Fmi2CallbackAllocateMemory allocate_memory{};
    Fmi2CallbackFreeMemory free_memory{};
    Fmi2StepFinished step_finished{};
    Fmi2ComponentEnvironment component_environment{};
};

struct Fmi2AsyncContext {
    std::atomic<std::size_t> callbacks{0};
    std::atomic<int> status{static_cast<int>(Fmi2Status::pending)};
    std::thread::id caller_thread;
    std::atomic<bool> cross_thread{false};
};

void fmi2_step_finished(
    Fmi2ComponentEnvironment environment, Fmi2Status status) {
    auto* context = static_cast<Fmi2AsyncContext*>(environment);
    if (context == nullptr) return;
    context->cross_thread.store(
        std::this_thread::get_id() != context->caller_thread,
        std::memory_order_release);
    context->status.store(static_cast<int>(status), std::memory_order_release);
    context->callbacks.fetch_add(1, std::memory_order_release);
}

using Fmi2GetVersion = const char* (*)();
using Fmi2Instantiate = Fmi2Component (*)(
    Fmi2String, Fmi2Type, Fmi2String, Fmi2String,
    const Fmi2CallbackFunctions*, Fmi2Boolean, Fmi2Boolean);
using Fmi2FreeInstance = void (*)(Fmi2Component);
using Fmi2SetupExperiment = Fmi2Status (*)(
    Fmi2Component, Fmi2Boolean, Fmi2Real, Fmi2Real, Fmi2Boolean, Fmi2Real);
using Fmi2EnterInitializationMode = Fmi2Status (*)(Fmi2Component);
using Fmi2ExitInitializationMode = Fmi2Status (*)(Fmi2Component);
using Fmi2Terminate = Fmi2Status (*)(Fmi2Component);
using Fmi2SetReal = Fmi2Status (*)(
    Fmi2Component, const Fmi2ValueReference*, std::size_t, const Fmi2Real*);
using Fmi2GetReal = Fmi2Status (*)(
    Fmi2Component, const Fmi2ValueReference*, std::size_t, Fmi2Real*);
using Fmi2SetInteger = Fmi2Status (*)(
    Fmi2Component, const Fmi2ValueReference*, std::size_t, const Fmi2Integer*);
using Fmi2GetInteger = Fmi2Status (*)(
    Fmi2Component, const Fmi2ValueReference*, std::size_t, Fmi2Integer*);
using Fmi2SetBoolean = Fmi2Status (*)(
    Fmi2Component, const Fmi2ValueReference*, std::size_t, const Fmi2Boolean*);
using Fmi2GetBoolean = Fmi2Status (*)(
    Fmi2Component, const Fmi2ValueReference*, std::size_t, Fmi2Boolean*);
using Fmi2SetString = Fmi2Status (*)(
    Fmi2Component, const Fmi2ValueReference*, std::size_t, const Fmi2String*);
using Fmi2GetString = Fmi2Status (*)(
    Fmi2Component, const Fmi2ValueReference*, std::size_t, Fmi2String*);
using Fmi2DoStep = Fmi2Status (*)(
    Fmi2Component, Fmi2Real, Fmi2Real, Fmi2Boolean);
using Fmi2GetStatus = Fmi2Status (*)(
    Fmi2Component, Fmi2StatusKind, Fmi2Status*);
using Fmi2GetRealStatus = Fmi2Status (*)(
    Fmi2Component, Fmi2StatusKind, Fmi2Real*);
using Fmi2CancelStep = Fmi2Status (*)(Fmi2Component);
struct Fmi2EventInfo {
    Fmi2Boolean new_discrete_states_needed{};
    Fmi2Boolean terminate_simulation{};
    Fmi2Boolean nominals_of_continuous_states_changed{};
    Fmi2Boolean values_of_continuous_states_changed{};
    Fmi2Boolean next_event_time_defined{};
    Fmi2Real next_event_time{};
};
using Fmi2EnterEventMode = Fmi2Status (*)(Fmi2Component);
using Fmi2NewDiscreteStates = Fmi2Status (*)(Fmi2Component, Fmi2EventInfo*);
using Fmi2EnterContinuousTimeMode = Fmi2Status (*)(Fmi2Component);
using Fmi2CompletedIntegratorStep = Fmi2Status (*)(
    Fmi2Component, Fmi2Boolean, Fmi2Boolean*, Fmi2Boolean*);
using Fmi2SetTime = Fmi2Status (*)(Fmi2Component, Fmi2Real);
using Fmi2SetContinuousStates = Fmi2Status (*)(
    Fmi2Component, const Fmi2Real*, std::size_t);
using Fmi2GetDerivatives = Fmi2Status (*)(Fmi2Component, Fmi2Real*, std::size_t);
using Fmi2GetContinuousStates = Fmi2Status (*)(
    Fmi2Component, Fmi2Real*, std::size_t);
using Fmi2GetNominalsOfContinuousStates = Fmi2Status (*)(
    Fmi2Component, Fmi2Real*, std::size_t);
using Fmi2GetEventIndicators = Fmi2Status (*)(
    Fmi2Component, Fmi2Real*, std::size_t);
using Fmi2GetFmuState = Fmi2Status (*)(Fmi2Component, Fmi2FmuState*);
using Fmi2SetFmuState = Fmi2Status (*)(Fmi2Component, Fmi2FmuState);
using Fmi2FreeFmuState = Fmi2Status (*)(Fmi2Component, Fmi2FmuState*);
using Fmi2SerializedFmuStateSize = Fmi2Status (*)(
    Fmi2Component, Fmi2FmuState, std::size_t*);
using Fmi2SerializeFmuState = Fmi2Status (*)(
    Fmi2Component, Fmi2FmuState, std::uint8_t*, std::size_t);
using Fmi2DeserializeFmuState = Fmi2Status (*)(
    Fmi2Component, const std::uint8_t*, std::size_t, Fmi2FmuState*);

using Fmi3LogMessageCallback = void (*)(
    Fmi3InstanceEnvironment, Fmi3Status, Fmi3String, Fmi3String);
using Fmi3IntermediateUpdateCallback = void (*)(
    Fmi3InstanceEnvironment, Fmi3Float64, Fmi3Boolean, Fmi3Boolean,
    Fmi3Boolean, Fmi3Boolean, Fmi3Boolean*, Fmi3Float64*);
using Fmi3ClockUpdateCallback = void (*)(Fmi3InstanceEnvironment);
using Fmi3LockPreemptionCallback = void (*)();
using Fmi3UnlockPreemptionCallback = void (*)();

struct IntermediateUpdateContext {
    std::size_t callbacks{0};
};

struct ScheduledExecutionCallbackContext {
    std::size_t clock_updates{0};
    std::size_t locks{0};
    std::size_t unlocks{0};
    std::size_t lock_depth{0};
    bool invalid_unlock{false};
};

thread_local ScheduledExecutionCallbackContext* scheduled_execution_callback_context{};

void clock_update_callback(Fmi3InstanceEnvironment environment) {
    auto* context = static_cast<ScheduledExecutionCallbackContext*>(environment);
    if (context != nullptr) ++context->clock_updates;
}

void lock_preemption_callback() {
    auto* context = scheduled_execution_callback_context;
    if (context == nullptr) return;
    ++context->locks;
    ++context->lock_depth;
}

void unlock_preemption_callback() {
    auto* context = scheduled_execution_callback_context;
    if (context == nullptr) return;
    ++context->unlocks;
    if (context->lock_depth == 0) {
        context->invalid_unlock = true;
        return;
    }
    --context->lock_depth;
}

void intermediate_update_callback(
    Fmi3InstanceEnvironment environment, Fmi3Float64 time,
    Fmi3Boolean, Fmi3Boolean, Fmi3Boolean step_finished,
    Fmi3Boolean can_return_early, Fmi3Boolean* early_return_requested,
    Fmi3Float64* early_return_time) {
    auto* context = static_cast<IntermediateUpdateContext*>(environment);
    if (context != nullptr) ++context->callbacks;
    if (early_return_requested != nullptr) {
        *early_return_requested = step_finished && can_return_early;
    }
    if (early_return_time != nullptr) *early_return_time = time;
}
using Fmi3InstantiateCoSimulation = Fmi3Instance (*)(
    Fmi3String, Fmi3String, Fmi3String, Fmi3Boolean, Fmi3Boolean,
    Fmi3Boolean, Fmi3Boolean, const Fmi3ValueReference*, std::size_t,
    Fmi3InstanceEnvironment, Fmi3LogMessageCallback,
    Fmi3IntermediateUpdateCallback);
using Fmi3InstantiateModelExchange = Fmi3Instance (*)(
    Fmi3String, Fmi3String, Fmi3String, Fmi3Boolean, Fmi3Boolean,
    Fmi3InstanceEnvironment, Fmi3LogMessageCallback);
using Fmi3InstantiateScheduledExecution = Fmi3Instance (*)(
    Fmi3String, Fmi3String, Fmi3String, Fmi3Boolean, Fmi3Boolean,
    Fmi3InstanceEnvironment, Fmi3LogMessageCallback, Fmi3ClockUpdateCallback,
    Fmi3LockPreemptionCallback, Fmi3UnlockPreemptionCallback);
using Fmi3GetVersion = const char* (*)();
using Fmi3FreeInstance = void (*)(Fmi3Instance);
using Fmi3EnterInitializationMode = Fmi3Status (*)(
    Fmi3Instance, Fmi3Boolean, Fmi3Float64, Fmi3Float64,
    Fmi3Boolean, Fmi3Float64);
using Fmi3ExitInitializationMode = Fmi3Status (*)(Fmi3Instance);
using Fmi3EnterEventMode = Fmi3Status (*)(Fmi3Instance);
using Fmi3UpdateDiscreteStates = Fmi3Status (*)(
    Fmi3Instance, Fmi3Boolean*, Fmi3Boolean*, Fmi3Boolean*,
    Fmi3Boolean*, Fmi3Boolean*, Fmi3Float64*);
using Fmi3EnterStepMode = Fmi3Status (*)(Fmi3Instance);
using Fmi3Terminate = Fmi3Status (*)(Fmi3Instance);
using Fmi3SetFloat64 = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    const Fmi3Float64*, std::size_t);
using Fmi3GetFloat64 = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    Fmi3Float64*, std::size_t);
template <typename Value>
using Fmi3SetNumeric = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    const Value*, std::size_t);
template <typename Value>
using Fmi3GetNumeric = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    Value*, std::size_t);
using Fmi3SetInt32 = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    const Fmi3Int32*, std::size_t);
using Fmi3GetInt32 = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    Fmi3Int32*, std::size_t);
using Fmi3SetBoolean = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    const Fmi3Boolean*, std::size_t);
using Fmi3GetBoolean = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    Fmi3Boolean*, std::size_t);
using Fmi3SetClock = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    const Fmi3Clock*, std::size_t);
using Fmi3GetClock = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    Fmi3Clock*, std::size_t);
using Fmi3GetIntervalDecimal = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    Fmi3Float64*, Fmi3IntervalQualifier*);
using Fmi3GetShiftDecimal = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t, Fmi3Float64*);
using Fmi3SetString = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    const Fmi3String*, std::size_t);
using Fmi3GetString = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    Fmi3String*, std::size_t);
using Fmi3SetBinary = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    const std::size_t*, const std::uint8_t* const*, std::size_t);
using Fmi3GetBinary = Fmi3Status (*)(
    Fmi3Instance, const Fmi3ValueReference*, std::size_t,
    std::size_t*, const std::uint8_t**, std::size_t);

template <typename Value>
struct Fmi3NumericIo {
    using value_type = Value;
    explicit Fmi3NumericIo(std::string suffix) : symbol_suffix(std::move(suffix)) {}
    std::string symbol_suffix;
    std::vector<Fmi3ValueReference> input_references;
    std::vector<Value> input_values;
    std::vector<Fmi3ValueReference> array_input_references;
    std::vector<Value> array_input_values;
    std::vector<const FmiVariableIR*> outputs;
    std::vector<Fmi3ValueReference> output_references;
    std::vector<const FmiVariableIR*> array_outputs;
    std::vector<Fmi3ValueReference> array_output_references;
    std::vector<std::size_t> array_output_extents;
    Fmi3SetNumeric<Value> set{};
    Fmi3GetNumeric<Value> get{};
};
using Fmi3GetFmuState = Fmi3Status (*)(Fmi3Instance, Fmi3FmuState*);
using Fmi3SetFmuState = Fmi3Status (*)(Fmi3Instance, Fmi3FmuState);
using Fmi3FreeFmuState = Fmi3Status (*)(Fmi3Instance, Fmi3FmuState*);
using Fmi3SerializedFmuStateSize = Fmi3Status (*)(
    Fmi3Instance, Fmi3FmuState, std::size_t*);
using Fmi3SerializeFmuState = Fmi3Status (*)(
    Fmi3Instance, Fmi3FmuState, std::uint8_t*, std::size_t);
using Fmi3DeserializeFmuState = Fmi3Status (*)(
    Fmi3Instance, const std::uint8_t*, std::size_t, Fmi3FmuState*);
using Fmi3DoStep = Fmi3Status (*)(
    Fmi3Instance, Fmi3Float64, Fmi3Float64, Fmi3Boolean,
    Fmi3Boolean*, Fmi3Boolean*, Fmi3Boolean*, Fmi3Float64*);
using Fmi3ActivateModelPartition = Fmi3Status (*)(
    Fmi3Instance, Fmi3ValueReference, Fmi3Float64);
using Fmi3EnterContinuousTimeMode = Fmi3Status (*)(Fmi3Instance);
using Fmi3CompletedIntegratorStep = Fmi3Status (*)(
    Fmi3Instance, Fmi3Boolean, Fmi3Boolean*, Fmi3Boolean*);
using Fmi3SetTime = Fmi3Status (*)(Fmi3Instance, Fmi3Float64);
using Fmi3SetContinuousStates = Fmi3Status (*)(
    Fmi3Instance, const Fmi3Float64*, std::size_t);
using Fmi3GetContinuousStateDerivatives = Fmi3Status (*)(
    Fmi3Instance, Fmi3Float64*, std::size_t);
using Fmi3GetContinuousStates = Fmi3Status (*)(
    Fmi3Instance, Fmi3Float64*, std::size_t);
using Fmi3GetNominalsOfContinuousStates = Fmi3Status (*)(
    Fmi3Instance, Fmi3Float64*, std::size_t);
using Fmi3GetNumberOfEventIndicators = Fmi3Status (*)(
    Fmi3Instance, std::size_t*);
using Fmi3GetNumberOfContinuousStates = Fmi3Status (*)(
    Fmi3Instance, std::size_t*);
using Fmi3GetEventIndicators = Fmi3Status (*)(
    Fmi3Instance, Fmi3Float64*, std::size_t);

class DynamicLibrary {
public:
    explicit DynamicLibrary(const std::filesystem::path& path) {
#if defined(_WIN32)
        handle_ = LoadLibraryW(path.wstring().c_str());
        if (handle_ == nullptr) {
            throw std::runtime_error("cannot load FMU binary: " + path.string());
        }
#else
        handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle_ == nullptr) {
            const char* error = dlerror();
            throw std::runtime_error(
                "cannot load FMU binary: " + std::string(error == nullptr ? "unknown" : error));
        }
#endif
    }

    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    ~DynamicLibrary() {
#if defined(_WIN32)
        if (handle_ != nullptr) FreeLibrary(handle_);
#else
        if (handle_ != nullptr) dlclose(handle_);
#endif
    }

    template <class Function>
    Function symbol(const char* name) const {
#if defined(_WIN32)
        const auto raw = GetProcAddress(handle_, name);
#else
        dlerror();
        const auto raw = dlsym(handle_, name);
#endif
        if (raw == nullptr) throw std::runtime_error(std::string("missing FMU symbol: ") + name);
        Function function{};
        static_assert(sizeof(function) == sizeof(raw));
        std::memcpy(&function, &raw, sizeof(function));
        return function;
    }

    template <class Function>
    Function optional_symbol(const char* name) const {
#if defined(_WIN32)
        const auto raw = GetProcAddress(handle_, name);
#else
        dlerror();
        const auto raw = dlsym(handle_, name);
#endif
        Function function{};
        if (raw == nullptr) return function;
        static_assert(sizeof(function) == sizeof(raw));
        std::memcpy(&function, &raw, sizeof(function));
        return function;
    }

private:
#if defined(_WIN32)
    HMODULE handle_{nullptr};
#else
    void* handle_{nullptr};
#endif
};

class TemporaryDirectory {
public:
    explicit TemporaryDirectory(std::string_view source_hash) {
        const auto base = std::filesystem::temp_directory_path();
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (std::size_t attempt = 0; attempt < 100; ++attempt) {
            path_ = base / ("smave-fmu-" + std::string(source_hash) + "-" +
                std::to_string(stamp) + "-" + std::to_string(attempt));
            std::error_code error;
            if (std::filesystem::create_directory(path_, error)) return;
        }
        throw std::runtime_error("cannot create temporary FMU directory");
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

std::string dynamic_library_extension(const std::string& platform) {
    if (platform.ends_with("darwin")) return ".dylib";
    if (platform.ends_with("linux")) return ".so";
    if (platform.ends_with("windows")) return ".dll";
    throw std::runtime_error("native FMU execution is unsupported on host platform: " + platform);
}

const FmiInterfaceIR& co_simulation_interface(const FmiBlackboxIR& model) {
    const auto iterator = std::find_if(
        model.interfaces.begin(), model.interfaces.end(), [](const auto& interface) {
            return interface.kind == "CoSimulation";
        });
    if (iterator == model.interfaces.end()) {
        throw std::invalid_argument("FMI smoke requires a CoSimulation interface");
    }
    return *iterator;
}

const FmiInterfaceIR& model_exchange_interface(const FmiBlackboxIR& model) {
    const auto iterator = std::find_if(
        model.interfaces.begin(), model.interfaces.end(), [](const auto& interface) {
            return interface.kind == "ModelExchange";
        });
    if (iterator == model.interfaces.end()) {
        throw std::invalid_argument("FMI smoke requires a ModelExchange interface");
    }
    return *iterator;
}

const FmiInterfaceIR& scheduled_execution_interface(const FmiBlackboxIR& model) {
    const auto iterator = std::find_if(
        model.interfaces.begin(), model.interfaces.end(), [](const auto& interface) {
            return interface.kind == "ScheduledExecution";
        });
    if (iterator == model.interfaces.end()) {
        throw std::invalid_argument("FMI smoke requires a ScheduledExecution interface");
    }
    return *iterator;
}

bool capability_enabled(const FmiInterfaceIR& interface, const std::string& name) {
    const auto iterator = interface.capabilities.find(name);
    return iterator != interface.capabilities.end() && iterator->second == "true";
}

std::filesystem::path interface_binary_relative(
    const FmiBlackboxIR& model,
    const FmiInterfaceIR& interface) {
    const auto filename = interface.model_identifier +
        dynamic_library_extension(model.host_platform);
    const auto modern = std::filesystem::path("binaries") /
        model.host_platform / filename;
    if (std::binary_search(
            model.archive_entries.begin(), model.archive_entries.end(),
            modern.generic_string())) {
        return modern;
    }
    if (model.fmi_version == "2.0") {
        const std::string legacy = model.host_platform.ends_with("darwin")
            ? "darwin64"
            : model.host_platform.ends_with("linux") ? "linux64"
            : model.host_platform.ends_with("windows") ? "win64" : "";
        if (!legacy.empty()) {
            const auto candidate = std::filesystem::path("binaries") / legacy / filename;
            if (std::binary_search(
                    model.archive_entries.begin(), model.archive_entries.end(),
                    candidate.generic_string())) {
                return candidate;
            }
        }
    }
    return modern;
}

void write_binary_file(const std::filesystem::path& path, const std::string& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot extract FMU entry: " + path.string());
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("cannot write extracted FMU entry: " + path.string());
}

std::filesystem::path prepare_fmu_directory(
    const std::filesystem::path& source,
    const FmiBlackboxIR& model,
    const FmiInterfaceIR& interface,
    std::unique_ptr<TemporaryDirectory>& temporary) {
    if (std::filesystem::is_directory(source)) return std::filesystem::absolute(source);
    temporary = std::make_unique<TemporaryDirectory>(model.source_hash);
    const auto archive = read_text(source, kMaximumArchiveSize);
    const auto entries = zip_entries(archive);
    const auto binary_relative = interface_binary_relative(model, interface).generic_string();
    bool binary_extracted{};
    std::size_t resource_bytes{};
    constexpr std::size_t maximum_entry = 128U * 1024U * 1024U;
    constexpr std::size_t maximum_resources = 512U * 1024U * 1024U;
    for (const auto& entry : entries) {
        const bool selected_binary = entry.name == binary_relative;
        const bool resource = entry.name.starts_with("resources/") &&
            !entry.name.ends_with('/');
        if (!selected_binary && !resource) continue;
        if (resource) {
            resource_bytes += entry.uncompressed_size;
            if (resource_bytes > maximum_resources) {
                throw std::runtime_error("FMU resources exceed extraction limit");
            }
        }
        write_binary_file(
            temporary->path() / std::filesystem::path(entry.name),
            extract_zip_entry(archive, entry, maximum_entry));
        if (selected_binary) binary_extracted = true;
    }
    if (!binary_extracted) throw std::runtime_error("host FMI interface binary is absent");
    std::filesystem::create_directories(temporary->path() / "resources");
    return temporary->path();
}

std::string file_uri(const std::filesystem::path& path) {
    const auto absolute = std::filesystem::absolute(path).generic_string();
    std::ostringstream result;
    result << "file://";
#if defined(_WIN32)
    result << '/';
#endif
    constexpr char hex[] = "0123456789ABCDEF";
    for (const unsigned char character : absolute) {
        if (std::isalnum(character) || character == '/' || character == '-' ||
            character == '_' || character == '.' || character == '~' || character == ':') {
            result << static_cast<char>(character);
        } else {
            result << '%' << hex[character >> 4U] << hex[character & 0xfU];
        }
    }
    if (!absolute.ends_with('/')) result << '/';
    return result.str();
}

void require_status(Fmi3Status status, std::size_t& warnings, std::string_view operation) {
    if (status == Fmi3Status::ok) return;
    if (status == Fmi3Status::warning) {
        ++warnings;
        return;
    }
    throw std::runtime_error(std::string(operation) + " returned non-success FMI status");
}

void require_status(Fmi2Status status, std::size_t& warnings, std::string_view operation) {
    if (status == Fmi2Status::ok) return;
    if (status == Fmi2Status::warning) {
        ++warnings;
        return;
    }
    throw std::runtime_error(std::string(operation) + " returned non-success FMI status");
}

const FmiVariableIR& scalar_float64_variable(
    const FmiBlackboxIR& model,
    const std::string& name,
    const std::unordered_set<std::string>& allowed_causalities) {
    const auto iterator = std::find_if(
        model.variables.begin(), model.variables.end(), [&](const auto& variable) {
            return variable.name == name;
        });
    if (iterator == model.variables.end()) {
        throw std::invalid_argument("unknown FMI variable: " + name);
    }
    if (iterator->type != "Float64" || iterator->dimensions != 0 ||
        !allowed_causalities.contains(iterator->causality) ||
        iterator->value_reference > std::numeric_limits<Fmi3ValueReference>::max()) {
        throw std::invalid_argument(name + ": smoke supports only scalar FMI 3 Float64 variables with compatible causality");
    }
    return *iterator;
}

const FmiVariableIR& scalar_real_variable(
    const FmiBlackboxIR& model,
    const std::string& name,
    const std::unordered_set<std::string>& allowed_causalities) {
    const auto iterator = std::find_if(
        model.variables.begin(), model.variables.end(), [&](const auto& variable) {
            return variable.name == name;
        });
    if (iterator == model.variables.end()) {
        throw std::invalid_argument("unknown FMI variable: " + name);
    }
    if (iterator->type != "Real" || iterator->dimensions != 0 ||
        !allowed_causalities.contains(iterator->causality) ||
        iterator->value_reference > std::numeric_limits<Fmi2ValueReference>::max()) {
        throw std::invalid_argument(
            name + ": smoke supports only scalar FMI 2 Real variables with compatible causality");
    }
    return *iterator;
}

const FmiVariableIR& scalar_input_variable(
    const FmiBlackboxIR& model,
    const std::string& name,
    const std::unordered_set<std::string>& supported_types,
    std::string_view version) {
    const auto iterator = std::find_if(
        model.variables.begin(), model.variables.end(), [&](const auto& variable) {
            return variable.name == name;
        });
    if (iterator == model.variables.end()) {
        throw std::invalid_argument("unknown FMI variable: " + name);
    }
    if (!supported_types.contains(iterator->type) || iterator->dimensions != 0 ||
        (iterator->causality != "input" && iterator->causality != "parameter") ||
        iterator->value_reference > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument(
            name + ": smoke supports only scalar FMI " + std::string(version) +
            " input/parameter variables of a supported type");
    }
    return *iterator;
}

template <typename Integer>
Integer exact_integer_input(double value, const std::string& name) {
    constexpr int digits = std::numeric_limits<Integer>::digits;
    const double upper_exclusive = std::ldexp(1.0, digits);
    const double lower_inclusive = std::is_signed_v<Integer>
        ? -upper_exclusive
        : 0.0;
    if (!std::isfinite(value) || std::trunc(value) != value ||
        value < lower_inclusive || value >= upper_exclusive) {
        throw std::invalid_argument(
            name + ": FMI integer input must be finite, exact, and in range");
    }
    if constexpr (std::numeric_limits<Integer>::digits > 53) {
        constexpr double maximum_exact_integer = 9007199254740991.0;
        if (value < -maximum_exact_integer || value > maximum_exact_integer) {
            throw std::invalid_argument(
                name + ": FMI integer input must be exactly representable as double");
        }
    }
    return static_cast<Integer>(value);
}

bool exact_boolean_input(double value, const std::string& name) {
    if (value != 0.0 && value != 1.0) {
        throw std::invalid_argument(name + ": FMI Boolean input must be exactly 0 or 1");
    }
    return value == 1.0;
}

}  // namespace

void FmiBlackboxIR::validate() const {
    if (schema_version != kFmiBlackboxSchemaVersion &&
        schema_version != kPreviousFmiBlackboxSchemaVersion &&
        schema_version != kOlderFmiBlackboxSchemaVersion &&
        schema_version != kLegacyFmiBlackboxSchemaVersion) {
        throw std::invalid_argument("unsupported FMI blackbox schema: " + schema_version);
    }
    if ((fmi_version != "2.0" && fmi_version != "3.0") ||
        model_name.empty() || instantiation_token.empty() || source_hash.empty()) {
        throw std::invalid_argument("incomplete FMI model identity");
    }
    if (interfaces.empty()) throw std::invalid_argument("FMI model exposes no supported interface");
    std::unordered_set<std::string> interface_kinds;
    for (const auto& interface : interfaces) {
        if ((interface.kind != "ModelExchange" && interface.kind != "CoSimulation" &&
             interface.kind != "ScheduledExecution") ||
            interface.model_identifier.empty() ||
            !interface_kinds.insert(interface.kind).second) {
            throw std::invalid_argument("invalid or duplicate FMI interface");
        }
    }
    std::unordered_set<std::string> names;
    const bool scheduled_execution = interface_kinds.contains("ScheduledExecution");
    for (const auto& variable : variables) {
        if (variable.name.empty() || variable.type.empty() ||
            !names.insert(variable.name).second) {
            throw std::invalid_argument("invalid or duplicate FMI variable");
        }
        if (variable.derivative_of > variables.size()) {
            throw std::invalid_argument("FMI derivative references an invalid variable index");
        }
        if (!variable.dimension_descriptors.empty() &&
            variable.dimensions != variable.dimension_descriptors.size()) {
            throw std::invalid_argument("FMI dimension count and descriptors disagree");
        }
        if (variable.type == "Clock" && variable.dimensions != 0) {
            throw std::invalid_argument("FMI 3 Clock variables must be scalar");
        }
        if (variable.clock_priority && variable.type != "Clock") {
            throw std::invalid_argument("FMI Clock priority is attached to a non-Clock variable");
        }
        if (scheduled_execution && variable.type == "Clock" &&
            variable.causality == "input" && !variable.clock_priority) {
            throw std::invalid_argument(
                "FMI Scheduled Execution input Clock requires priority");
        }
        for (const auto& dimension : variable.dimension_descriptors) {
            if (dimension.fixed_extent.has_value() ==
                dimension.extent_value_reference.has_value()) {
                throw std::invalid_argument("FMI dimension descriptor is ambiguous");
            }
            if (dimension.fixed_extent == 0) {
                throw std::invalid_argument("FMI fixed dimension extent must be positive");
            }
        }
        if (variable.derivative_of != 0) {
            const auto& state = variables[variable.derivative_of - 1];
            if (variable.type != "Real" || variable.dimensions != 0 ||
                state.type != "Real" || state.dimensions != 0 ||
                state.derivative_of != 0) {
                throw std::invalid_argument(
                    "FMI derivative mapping must connect scalar Real derivative and state variables");
            }
        }
    }
    std::unordered_set<std::size_t> derivative_indices;
    for (const auto index : derivative_variable_order) {
        if (index == 0 || index > variables.size() ||
            !derivative_indices.insert(index).second ||
            variables[index - 1].derivative_of == 0) {
            throw std::invalid_argument(
                "FMI ModelStructure derivative order is invalid or inconsistent");
        }
    }
    if (!trajectory_proxy_allowed || !differential_test_allowed ||
        equation_level_validation_allowed || direct_expert_allowed) {
        throw std::invalid_argument("FMI blackbox permission boundary was widened");
    }
}

void FmiBlackboxIR::write(const std::filesystem::path& path) const {
    validate();
    if (schema_version != kFmiBlackboxSchemaVersion) {
        throw std::invalid_argument("legacy FMI IR must be upgraded before writing");
    }
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write FMI IR: " + path.string());
    output << schema_version << '\n'
           << "FMI_VERSION " << std::quoted(fmi_version) << '\n'
           << "MODEL " << std::quoted(model_name) << '\n'
           << "TOKEN " << std::quoted(instantiation_token) << '\n'
           << "SOURCE_HASH " << std::quoted(source_hash) << '\n'
           << "GENERATION_TOOL " << std::quoted(generation_tool) << '\n'
           << "GENERATION_TIME " << std::quoted(generation_date_time) << '\n'
           << "NAMING " << std::quoted(variable_naming_convention) << '\n'
           << "HOST_PLATFORM " << std::quoted(host_platform) << '\n'
           << "HOST_BINARY_CANDIDATE " << host_binary_candidate_available << '\n'
           << "EVENT_INDICATORS " << number_of_event_indicators << '\n'
           << "PERMISSIONS " << trajectory_proxy_allowed << ' '
           << differential_test_allowed << ' ' << equation_level_validation_allowed << ' '
           << direct_expert_allowed << '\n'
           << "DEFAULT_EXPERIMENT " << default_experiment.size();
    for (const auto& __entry : default_experiment) {

        const auto& name = __entry.first;

        const auto& value = __entry.second;
        output << ' ' << std::quoted(name) << ' ' << std::quoted(value);
    }
    output << '\n' << "INTERFACES " << interfaces.size() << '\n';
    for (const auto& interface : interfaces) {
        output << "INTERFACE " << std::quoted(interface.kind) << ' '
               << std::quoted(interface.model_identifier) << ' '
               << interface.capabilities.size();
        for (const auto& __entry : interface.capabilities) {

            const auto& name = __entry.first;

            const auto& value = __entry.second;
            output << ' ' << std::quoted(name) << ' ' << std::quoted(value);
        }
        output << '\n';
    }
    output << "VARIABLES " << variables.size() << '\n';
    for (const auto& variable : variables) {
        output << "VARIABLE " << std::quoted(variable.name) << ' '
               << std::quoted(variable.type) << ' ' << variable.value_reference << ' '
               << std::quoted(variable.causality) << ' '
               << std::quoted(variable.variability) << ' '
               << std::quoted(variable.initial) << ' ' << std::quoted(variable.unit) << ' '
               << std::quoted(variable.start) << ' ' << variable.dimensions << ' '
               << variable.derivative_of << ' ' << variable.clock_priority.has_value();
        if (variable.clock_priority) output << ' ' << *variable.clock_priority;
        output << ' ' << variable.dimension_descriptors.size();
        for (const auto& dimension : variable.dimension_descriptors) {
            if (dimension.fixed_extent) {
                output << " F " << *dimension.fixed_extent;
            } else {
                output << " R " << *dimension.extent_value_reference;
            }
        }
        output << '\n';
    }
    output << "DERIVATIVE_ORDER " << derivative_variable_order.size();
    for (const auto index : derivative_variable_order) output << ' ' << index;
    output << '\n';
    const auto write_strings = [&](std::string_view tag, const auto& values) {
        output << tag << ' ' << values.size();
        for (const auto& value : values) output << ' ' << std::quoted(value);
        output << '\n';
    };
    write_strings("ARCHIVE_ENTRIES", archive_entries);
    write_strings("BINARY_PLATFORMS", binary_platforms);
    write_strings("WARNINGS", warnings);
    output << "END\n";
}

FmiBlackboxIR FmiBlackboxIR::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read FMI IR: " + path.string());
    FmiBlackboxIR model;
    input >> model.schema_version;
    const bool legacy_schema = model.schema_version == kLegacyFmiBlackboxSchemaVersion;
    const bool previous_schema = model.schema_version == kPreviousFmiBlackboxSchemaVersion;
    const bool older_schema = model.schema_version == kOlderFmiBlackboxSchemaVersion;
    require_tag(input, "FMI_VERSION"); input >> std::quoted(model.fmi_version);
    require_tag(input, "MODEL"); input >> std::quoted(model.model_name);
    require_tag(input, "TOKEN"); input >> std::quoted(model.instantiation_token);
    require_tag(input, "SOURCE_HASH"); input >> std::quoted(model.source_hash);
    require_tag(input, "GENERATION_TOOL"); input >> std::quoted(model.generation_tool);
    require_tag(input, "GENERATION_TIME"); input >> std::quoted(model.generation_date_time);
    require_tag(input, "NAMING"); input >> std::quoted(model.variable_naming_convention);
    require_tag(input, "HOST_PLATFORM"); input >> std::quoted(model.host_platform);
    require_tag(input, "HOST_BINARY_CANDIDATE"); input >> model.host_binary_candidate_available;
    require_tag(input, "EVENT_INDICATORS"); input >> model.number_of_event_indicators;
    require_tag(input, "PERMISSIONS");
    input >> model.trajectory_proxy_allowed >> model.differential_test_allowed
          >> model.equation_level_validation_allowed >> model.direct_expert_allowed;
    std::size_t count{};
    require_tag(input, "DEFAULT_EXPERIMENT"); input >> count;
    for (std::size_t index = 0; index < count; ++index) {
        std::string name;
        std::string value;
        input >> std::quoted(name) >> std::quoted(value);
        model.default_experiment.emplace(std::move(name), std::move(value));
    }
    require_tag(input, "INTERFACES"); input >> count;
    for (std::size_t index = 0; index < count; ++index) {
        require_tag(input, "INTERFACE");
        FmiInterfaceIR interface;
        std::size_t capabilities{};
        input >> std::quoted(interface.kind) >> std::quoted(interface.model_identifier)
              >> capabilities;
        for (std::size_t capability = 0; capability < capabilities; ++capability) {
            std::string name;
            std::string value;
            input >> std::quoted(name) >> std::quoted(value);
            interface.capabilities.emplace(std::move(name), std::move(value));
        }
        model.interfaces.push_back(std::move(interface));
    }
    require_tag(input, "VARIABLES"); input >> count;
    for (std::size_t index = 0; index < count; ++index) {
        require_tag(input, "VARIABLE");
        FmiVariableIR variable;
        input >> std::quoted(variable.name) >> std::quoted(variable.type)
              >> variable.value_reference >> std::quoted(variable.causality)
              >> std::quoted(variable.variability) >> std::quoted(variable.initial)
              >> std::quoted(variable.unit) >> std::quoted(variable.start)
              >> variable.dimensions;
        if (!legacy_schema) input >> variable.derivative_of;
        if (!legacy_schema && !older_schema && !previous_schema) {
            bool has_clock_priority{};
            input >> has_clock_priority;
            if (has_clock_priority) {
                std::uint64_t priority{};
                input >> priority;
                if (priority > std::numeric_limits<std::uint32_t>::max()) {
                    throw std::invalid_argument("FMI Clock priority exceeds UInt32 range");
                }
                variable.clock_priority = static_cast<std::uint32_t>(priority);
            }
        }
        if (!legacy_schema && !older_schema) {
            std::size_t dimensions{};
            input >> dimensions;
            for (std::size_t dimension_index = 0;
                 dimension_index < dimensions; ++dimension_index) {
                char kind{};
                std::uint64_t value{};
                input >> kind >> value;
                FmiDimensionIR dimension;
                if (kind == 'F' && value > 0 &&
                    value <= std::numeric_limits<std::size_t>::max()) {
                    dimension.fixed_extent = static_cast<std::size_t>(value);
                } else if (kind == 'R') {
                    dimension.extent_value_reference = value;
                } else {
                    throw std::invalid_argument("invalid FMI dimension descriptor");
                }
                variable.dimension_descriptors.push_back(std::move(dimension));
            }
        }
        model.variables.push_back(std::move(variable));
    }
    if (!legacy_schema) {
        require_tag(input, "DERIVATIVE_ORDER");
        input >> count;
        for (std::size_t index = 0; index < count; ++index) {
            std::size_t derivative_index{};
            input >> derivative_index;
            model.derivative_variable_order.push_back(derivative_index);
        }
    }
    const auto read_strings = [&](std::string_view tag, auto& values) {
        require_tag(input, tag);
        std::size_t size{};
        input >> size;
        for (std::size_t index = 0; index < size; ++index) {
            std::string value;
            input >> std::quoted(value);
            values.push_back(std::move(value));
        }
    };
    read_strings("ARCHIVE_ENTRIES", model.archive_entries);
    read_strings("BINARY_PLATFORMS", model.binary_platforms);
    read_strings("WARNINGS", model.warnings);
    require_tag(input, "END");
    if (!input) throw std::runtime_error("truncated FMI IR");
    if (legacy_schema || older_schema || previous_schema) {
        const bool scheduled = std::any_of(
            model.interfaces.begin(), model.interfaces.end(),
            [](const FmiInterfaceIR& interface) {
                return interface.kind == "ScheduledExecution";
            });
        if (scheduled) {
            for (auto& variable : model.variables) {
                if (variable.type == "Clock" && variable.causality == "input" &&
                    !variable.clock_priority) {
                    variable.clock_priority = 0;
                }
            }
        }
    }
    model.validate();
    model.schema_version = kFmiBlackboxSchemaVersion;
    return model;
}

namespace {

std::string xml_local_name(const std::string& name) {
    const auto separator = name.rfind(':');
    return separator == std::string::npos ? name : name.substr(separator + 1);
}

struct SspConnectorDefinition {
    std::string kind;
    bool real{false};
    std::string unit;
};

struct SspUnitDefinition {
    std::array<int, 8> exponents{};
    double factor{1.0};
    double offset{0.0};
    bool complete{false};
};

struct SspComponentDefinition {
    std::string name;
    std::string source;
    std::map<std::string, SspConnectorDefinition> connectors;
};

struct SspConnectionDefinition {
    std::string source_component;
    std::string source_connector;
    std::string target_component;
    std::string target_connector;
    double factor{1.0};
    double offset{0.0};
    bool transformed{false};
    bool suppress_unit_conversion{false};
    std::string source_unit;
    std::string target_unit;
    double unit_factor{1.0};
    double unit_offset{0.0};
};

struct ParsedSspSystem {
    std::string name;
    std::vector<SspComponentDefinition> components;
    std::vector<SspConnectionDefinition> connections;
    std::map<std::string, SspUnitDefinition> units;
};

std::map<std::string, SspUnitDefinition> parse_unit_definitions(
    const std::string& descriptor) {
    std::map<std::string, SspUnitDefinition> units;
    std::optional<std::string> current_unit;
    for (const auto& tag : xml_tags(descriptor)) {
        const auto name = xml_local_name(tag.name);
        if (tag.closing) {
            if (name == "Unit") current_unit.reset();
            continue;
        }
        if (name == "Unit") {
            const auto unit_name = attribute(tag.attributes, "name");
            if (unit_name.empty() || current_unit ||
                !units.emplace(unit_name, SspUnitDefinition{}).second) {
                throw std::invalid_argument("unit definition requires a unique name");
            }
            current_unit = unit_name;
        } else if (name == "BaseUnit" && current_unit) {
            auto& unit = units.at(*current_unit);
            if (unit.complete) {
                throw std::invalid_argument("unit definition contains multiple BaseUnit elements");
            }
            static constexpr std::array<std::string_view, 8> dimensions{
                "kg", "m", "s", "A", "K", "mol", "cd", "rad"};
            for (std::size_t index = 0; index < dimensions.size(); ++index) {
                const auto text = attribute(
                    tag.attributes, std::string(dimensions[index]));
                if (text.empty()) continue;
                try {
                    std::size_t consumed{};
                    const long value = std::stol(text, &consumed);
                    if (consumed == text.size() &&
                        value >= std::numeric_limits<int>::min() &&
                        value <= std::numeric_limits<int>::max()) {
                        unit.exponents[index] = static_cast<int>(value);
                        continue;
                    }
                } catch (const std::exception&) {
                }
                throw std::invalid_argument("unit BaseUnit exponent must be an integer");
            }
            const auto parse_value = [&](std::string_view key, double fallback) {
                const auto text = attribute(tag.attributes, std::string(key));
                if (text.empty()) return fallback;
                try {
                    std::size_t consumed{};
                    const double value = std::stod(text, &consumed);
                    if (consumed == text.size() && std::isfinite(value)) return value;
                } catch (const std::exception&) {
                }
                throw std::invalid_argument("unit BaseUnit factor and offset must be finite");
            };
            unit.factor = parse_value("factor", 1.0);
            unit.offset = parse_value("offset", 0.0);
            if (unit.factor == 0.0) {
                throw std::invalid_argument("unit BaseUnit factor must be non-zero");
            }
            unit.complete = true;
        }
        if (tag.self_closing && name == "Unit") current_unit.reset();
    }
    if (current_unit || std::any_of(
            units.begin(), units.end(),
            [](const auto& item) { return !item.second.complete; })) {
        throw std::invalid_argument("incomplete unit definition");
    }
    return units;
}

std::string fmu_model_description(const std::filesystem::path& path) {
    const auto archive = read_text(path, kMaximumArchiveSize);
    const auto entries = zip_entries(archive);
    const auto descriptor = std::find_if(
        entries.begin(), entries.end(),
        [](const auto& entry) { return entry.name == "modelDescription.xml"; });
    if (descriptor == entries.end()) {
        throw std::invalid_argument("FMU lacks modelDescription.xml");
    }
    return extract_zip_entry(archive, *descriptor, kMaximumDescriptorSize);
}

ParsedSspSystem parse_ssp_system(const std::string& descriptor) {
    ParsedSspSystem result;
    std::size_t system_depth{};
    std::optional<std::size_t> current_component;
    std::optional<std::string> current_connector;
    std::optional<std::size_t> current_connection;
    std::optional<std::string> current_unit;
    bool saw_structure{};
    bool saw_system{};
    for (const auto& tag : xml_tags(descriptor)) {
        const auto name = xml_local_name(tag.name);
        if (tag.closing) {
            if (name == "Connector") current_connector.reset();
            if (name == "Component") current_component.reset();
            if (name == "Connection") current_connection.reset();
            if (name == "Unit") current_unit.reset();
            if (name == "System") {
                if (system_depth == 0) throw std::invalid_argument("unbalanced SSP System");
                --system_depth;
            }
            continue;
        }
        if (name == "SystemStructure") {
            if (saw_structure || attribute(tag.attributes, "version") != "1.0") {
                throw std::invalid_argument("SSP requires one SystemStructure version 1.0");
            }
            saw_structure = true;
        } else if (name == "System") {
            ++system_depth;
            if (system_depth != 1 || saw_system) {
                throw std::invalid_argument("nested or multiple SSP Systems are unsupported");
            }
            saw_system = true;
            result.name = attribute(tag.attributes, "name");
            if (result.name.empty()) throw std::invalid_argument("SSP System requires name");
        } else if (name == "Component") {
            if (system_depth != 1 || current_component) {
                throw std::invalid_argument("SSP Component must be a direct System element");
            }
            const auto type = attribute(tag.attributes, "type");
            if (!type.empty() && type != "application/x-fmu-sharedlibrary") {
                throw std::invalid_argument("SSP Component type must be an FMU shared library");
            }
            SspComponentDefinition component;
            component.name = attribute(tag.attributes, "name");
            component.source = attribute(tag.attributes, "source");
            if (component.name.empty() || component.source.empty()) {
                throw std::invalid_argument("SSP Component requires name and source");
            }
            result.components.push_back(std::move(component));
            current_component = result.components.size() - 1;
        } else if (name == "Connector") {
            if (!current_component) {
                throw std::invalid_argument("system-level SSP connectors are unsupported");
            }
            const auto connector_name = attribute(tag.attributes, "name");
            const auto kind = attribute(tag.attributes, "kind");
            if (connector_name.empty() || (kind != "input" && kind != "output")) {
                throw std::invalid_argument("SSP Connector requires input/output kind and name");
            }
            if (!result.components[*current_component].connectors.emplace(
                    connector_name, SspConnectorDefinition{kind, false, {}}).second) {
                throw std::invalid_argument("duplicate SSP component connector");
            }
            current_connector = connector_name;
        } else if (name == "Real") {
            if (!current_component || !current_connector) {
                throw std::invalid_argument("detached SSP Real connector type");
            }
            result.components[*current_component]
                .connectors.at(*current_connector).real = true;
            result.components[*current_component]
                .connectors.at(*current_connector).unit =
                    attribute(tag.attributes, "unit");
        } else if (name == "Unit") {
            if (system_depth != 0 || !saw_system || current_component ||
                current_connection || current_unit) {
                throw std::invalid_argument("SSP Unit must be a top-level unit definition");
            }
            const auto unit_name = attribute(tag.attributes, "name");
            if (unit_name.empty() ||
                !result.units.emplace(unit_name, SspUnitDefinition{}).second) {
                throw std::invalid_argument("SSP Unit requires a unique non-empty name");
            }
            current_unit = unit_name;
        } else if (name == "BaseUnit") {
            if (!current_unit || result.units.at(*current_unit).complete) {
                throw std::invalid_argument("SSP BaseUnit must appear once inside Unit");
            }
            auto& unit = result.units.at(*current_unit);
            static constexpr std::array<std::string_view, 8> dimensions{
                "kg", "m", "s", "A", "K", "mol", "cd", "rad"};
            for (std::size_t index = 0; index < dimensions.size(); ++index) {
                const auto text = attribute(
                    tag.attributes, std::string(dimensions[index]));
                if (text.empty()) continue;
                try {
                    std::size_t consumed{};
                    const long value = std::stol(text, &consumed);
                    if (consumed == text.size() &&
                        value >= std::numeric_limits<int>::min() &&
                        value <= std::numeric_limits<int>::max()) {
                        unit.exponents[index] = static_cast<int>(value);
                        continue;
                    }
                } catch (const std::exception&) {
                }
                throw std::invalid_argument("SSP BaseUnit exponent must be an integer");
            }
            const auto parse_unit_value = [&](std::string_view key, double fallback) {
                const auto text = attribute(tag.attributes, std::string(key));
                if (text.empty()) return fallback;
                try {
                    std::size_t consumed{};
                    const double value = std::stod(text, &consumed);
                    if (consumed == text.size() && std::isfinite(value)) return value;
                } catch (const std::exception&) {
                }
                throw std::invalid_argument("SSP BaseUnit factor and offset must be finite");
            };
            unit.factor = parse_unit_value("factor", 1.0);
            unit.offset = parse_unit_value("offset", 0.0);
            if (unit.factor == 0.0) {
                throw std::invalid_argument("SSP BaseUnit factor must be non-zero");
            }
            unit.complete = true;
        } else if (name == "Integer" || name == "Boolean" || name == "String" ||
                   name == "Enumeration" || name == "Binary") {
            throw std::invalid_argument("SSP master supports only scalar Real connectors");
        } else if (name == "Connection") {
            SspConnectionDefinition connection;
            connection.source_component = attribute(tag.attributes, "startElement");
            connection.source_connector = attribute(tag.attributes, "startConnector");
            connection.target_component = attribute(tag.attributes, "endElement");
            connection.target_connector = attribute(tag.attributes, "endConnector");
            if (connection.source_component.empty() || connection.source_connector.empty() ||
                connection.target_component.empty() || connection.target_connector.empty()) {
                throw std::invalid_argument("SSP Connection requires complete endpoints");
            }
            const auto suppress = attribute(tag.attributes, "suppressUnitConversion");
            if (!suppress.empty()) {
                if (suppress == "true" || suppress == "1") {
                    connection.suppress_unit_conversion = true;
                } else if (suppress != "false" && suppress != "0") {
                    throw std::invalid_argument(
                        "SSP Connection suppressUnitConversion must be boolean");
                }
            }
            result.connections.push_back(std::move(connection));
            current_connection = result.connections.size() - 1;
        } else if (name == "LinearTransformation") {
            if (!current_connection || result.connections[*current_connection].transformed) {
                throw std::invalid_argument(
                    "SSP LinearTransformation must appear once inside a Connection");
            }
            auto& connection = result.connections[*current_connection];
            const auto parse_value = [&](std::string_view key, double fallback) {
                const auto text = attribute(tag.attributes, std::string(key));
                if (text.empty()) return fallback;
                try {
                    std::size_t consumed{};
                    const double value = std::stod(text, &consumed);
                    if (consumed == text.size() && std::isfinite(value)) return value;
                } catch (const std::exception&) {
                }
                {
                    throw std::invalid_argument(
                        "SSP LinearTransformation requires finite factor and offset");
                }
            };
            connection.factor = parse_value("factor", 1.0);
            connection.offset = parse_value("offset", 0.0);
            connection.transformed = true;
        } else if (name == "BooleanMappingTransformation" ||
                   name == "IntegerMappingTransformation" ||
                   name == "EnumerationMappingTransformation") {
            throw std::invalid_argument(
                "SSP master supports only scalar LinearTransformation");
        } else if (name == "ParameterBindings" || name == "ParameterBinding" ||
                   name == "SignalDictionaries" || name == "SignalDictionary" ||
                   name == "Transformation") {
            throw std::invalid_argument(
                "SSP parameter and dictionary semantics are unsupported");
        }
        if (tag.self_closing) {
            if (name == "Connector") current_connector.reset();
            if (name == "Component") current_component.reset();
            if (name == "Connection") current_connection.reset();
            if (name == "Unit") current_unit.reset();
            if (name == "System") --system_depth;
        }
    }
    if (!saw_structure || !saw_system || system_depth != 0 || current_component ||
        current_connector || current_connection || current_unit ||
        result.components.size() < 2) {
        throw std::invalid_argument("incomplete SSP system or fewer than two components");
    }
    for (const auto& component : result.components) {
        if (component.connectors.empty() || std::any_of(
                component.connectors.begin(), component.connectors.end(),
                [](const auto& item) { return !item.second.real; })) {
            throw std::invalid_argument("every SSP component connector must declare scalar Real type");
        }
    }
    if (std::any_of(
            result.units.begin(), result.units.end(),
            [](const auto& item) { return !item.second.complete; })) {
        throw std::invalid_argument("every SSP Unit requires one BaseUnit");
    }
    return result;
}

class CoupledSession {
public:
    struct StepOutcome {
        double reached_time{0.0};
        bool early_return{false};
    };
    class Snapshot {
    public:
        virtual ~Snapshot() = default;
    };
    virtual ~CoupledSession() = default;
    virtual const FmiBlackboxIR& model() const = 0;
    virtual void validate_connector(
        const std::string& name, const std::string& causality,
        const std::string& unit = {}) const = 0;
    virtual double output(const std::string& name) = 0;
    virtual void set_input(const std::string& name, double value) = 0;
    virtual void finish_initialization() = 0;
    virtual StepOutcome step(double current_time, double step_size) = 0;
    virtual void terminate() = 0;
    virtual std::size_t event_mode_entries() const { return 0; }
    virtual std::size_t discrete_update_iterations() const { return 0; }
    virtual std::size_t time_events() const { return 0; }
    virtual bool supports_variable_communication_step() const = 0;
    virtual std::optional<double> next_event_time() const { return std::nullopt; }
    virtual bool supports_state_rollback() const = 0;
    virtual bool may_return_early() const { return false; }
    virtual std::unique_ptr<Snapshot> save_state() = 0;
    virtual void restore_state(const Snapshot& snapshot) = 0;
    virtual std::size_t early_returns() const { return 0; }
    virtual std::size_t rollback_replays() const { return 0; }
};

class Fmi2CoupledSession final : public CoupledSession {
public:
    Fmi2CoupledSession(
        const std::filesystem::path& path,
        std::string instance_name,
        double end_time)
        : model_(import_fmu(path)), instance_name_(std::move(instance_name)) {
        if (model_.fmi_version != "2.0") {
            throw std::invalid_argument("SSP master supports only FMI 2.0 components");
        }
        interface_ = co_simulation_interface(model_);
        if (!model_.host_binary_candidate_available) {
            throw std::runtime_error("SSP component lacks a complete host CoSimulation binary");
        }
        const auto directory = prepare_fmu_directory(path, model_, interface_, temporary_);
        library_ = std::make_unique<DynamicLibrary>(
            directory / interface_binary_relative(model_, interface_));
        get_version_ = library_->symbol<Fmi2GetVersion>("fmi2GetVersion");
        instantiate_ = library_->symbol<Fmi2Instantiate>("fmi2Instantiate");
        free_instance_ = library_->symbol<Fmi2FreeInstance>("fmi2FreeInstance");
        setup_experiment_ = library_->symbol<Fmi2SetupExperiment>("fmi2SetupExperiment");
        enter_initialization_ = library_->symbol<Fmi2EnterInitializationMode>(
            "fmi2EnterInitializationMode");
        exit_initialization_ = library_->symbol<Fmi2ExitInitializationMode>(
            "fmi2ExitInitializationMode");
        terminate_ = library_->symbol<Fmi2Terminate>("fmi2Terminate");
        set_real_ = library_->symbol<Fmi2SetReal>("fmi2SetReal");
        get_real_ = library_->symbol<Fmi2GetReal>("fmi2GetReal");
        do_step_ = library_->symbol<Fmi2DoStep>("fmi2DoStep");
        state_enabled_ = capability_enabled(interface_, "canGetAndSetFMUstate");
        if (state_enabled_) {
            get_state_ = library_->symbol<Fmi2GetFmuState>("fmi2GetFMUstate");
            set_state_ = library_->symbol<Fmi2SetFmuState>("fmi2SetFMUstate");
            free_state_ = library_->symbol<Fmi2FreeFmuState>("fmi2FreeFMUstate");
        }
        const char* version = get_version_();
        if (version == nullptr || std::string_view(version) != "2.0") {
            throw std::runtime_error("SSP component runtime version is not FMI 2.0");
        }
        callbacks_.allocate_memory = std::calloc;
        callbacks_.free_memory = std::free;
        const auto resource_path = file_uri(directory / "resources");
        instance_ = instantiate_(
            instance_name_.c_str(), Fmi2Type::co_simulation,
            model_.instantiation_token.c_str(), resource_path.c_str(),
            &callbacks_, false, false);
        if (instance_ == nullptr) throw std::runtime_error("SSP fmi2Instantiate returned null");
        try {
            require_status(
                setup_experiment_(instance_, true, 1.0e-6, 0.0, true, end_time),
                warnings_, "SSP fmi2SetupExperiment");
            require_status(
                enter_initialization_(instance_), warnings_,
                "SSP fmi2EnterInitializationMode");
            in_initialization_ = true;
        } catch (...) {
            free_instance_(instance_);
            instance_ = nullptr;
            throw;
        }
    }

    Fmi2CoupledSession(const Fmi2CoupledSession&) = delete;
    Fmi2CoupledSession& operator=(const Fmi2CoupledSession&) = delete;

    ~Fmi2CoupledSession() {
        if (instance_ != nullptr) free_instance_(instance_);
    }

    const FmiBlackboxIR& model() const override { return model_; }

    void validate_connector(
        const std::string& name,
        const std::string& causality,
        const std::string& unit = {}) const override {
        const auto iterator = std::find_if(
            model_.variables.begin(), model_.variables.end(), [&](const auto& variable) {
                return variable.name == name;
            });
        if (iterator == model_.variables.end() || iterator->type != "Real" ||
            iterator->dimensions != 0 || iterator->causality != causality ||
            iterator->value_reference > std::numeric_limits<Fmi2ValueReference>::max() ||
            (!unit.empty() && iterator->unit != unit)) {
            throw std::invalid_argument(
                instance_name_ + "." + name +
                ": SSP connector does not match a scalar Real FMI " + causality);
        }
    }

    double output(const std::string& name) override {
        const auto reference = reference_for(name, "output");
        Fmi2Real value{};
        require_status(
            get_real_(instance_, &reference, 1, &value), warnings_,
            "SSP fmi2GetReal");
        if (!std::isfinite(value)) {
            throw std::runtime_error(instance_name_ + "." + name + ": non-finite SSP output");
        }
        return value;
    }

    void set_input(const std::string& name, double value) override {
        if (!std::isfinite(value)) throw std::runtime_error("non-finite SSP connection value");
        const auto reference = reference_for(name, "input");
        require_status(
            set_real_(instance_, &reference, 1, &value), warnings_,
            "SSP fmi2SetReal");
    }

    void finish_initialization() override {
        if (!in_initialization_) return;
        require_status(
            exit_initialization_(instance_), warnings_,
            "SSP fmi2ExitInitializationMode");
        in_initialization_ = false;
    }

    StepOutcome step(double current_time, double step_size) override {
        const auto status = do_step_(instance_, current_time, step_size, true);
        if (status == Fmi2Status::warning) ++warnings_;
        else if (status != Fmi2Status::ok) {
            throw std::runtime_error(
                "SSP fixed-step master rejects Discard/Pending/error FMI status");
        }
        return {current_time + step_size, false};
    }

    void terminate() override {
        if (terminated_ || instance_ == nullptr) return;
        require_status(terminate_(instance_), warnings_, "SSP fmi2Terminate");
        terminated_ = true;
    }

    bool supports_variable_communication_step() const override {
        return capability_enabled(
            interface_, "canHandleVariableCommunicationStepSize");
    }

    bool supports_state_rollback() const override { return state_enabled_; }

    class Fmi2Snapshot final : public Snapshot {
    public:
        Fmi2FmuState state{};
        Fmi2Component instance{};
        Fmi2FreeFmuState free_state{};
        ~Fmi2Snapshot() override {
            if (state != nullptr) free_state(instance, &state);
        }
    };

    std::unique_ptr<Snapshot> save_state() override {
        if (!state_enabled_) {
            throw std::runtime_error("SSP rollback requires FMI 2 state capability");
        }
        auto snapshot = std::make_unique<Fmi2Snapshot>();
        snapshot->instance = instance_;
        snapshot->free_state = free_state_;
        require_status(
            get_state_(instance_, &snapshot->state), warnings_, "SSP fmi2GetFMUstate");
        if (snapshot->state == nullptr) {
            throw std::runtime_error("SSP fmi2GetFMUstate returned null");
        }
        return snapshot;
    }

    void restore_state(const Snapshot& snapshot) override {
        const auto* typed = dynamic_cast<const Fmi2Snapshot*>(&snapshot);
        if (typed == nullptr || typed->state == nullptr) {
            throw std::invalid_argument("invalid FMI 2 SSP snapshot");
        }
        require_status(
            set_state_(instance_, typed->state), warnings_, "SSP fmi2SetFMUstate");
        ++rollback_replays_;
    }

    std::size_t rollback_replays() const override { return rollback_replays_; }

private:
    Fmi2ValueReference reference_for(
        const std::string& name,
        const std::string& causality) const {
        validate_connector(name, causality);
        const auto iterator = std::find_if(
            model_.variables.begin(), model_.variables.end(), [&](const auto& variable) {
                return variable.name == name;
            });
        return static_cast<Fmi2ValueReference>(iterator->value_reference);
    }

    FmiBlackboxIR model_;
    FmiInterfaceIR interface_;
    std::string instance_name_;
    std::unique_ptr<TemporaryDirectory> temporary_;
    std::unique_ptr<DynamicLibrary> library_;
    Fmi2GetVersion get_version_{};
    Fmi2Instantiate instantiate_{};
    Fmi2FreeInstance free_instance_{};
    Fmi2SetupExperiment setup_experiment_{};
    Fmi2EnterInitializationMode enter_initialization_{};
    Fmi2ExitInitializationMode exit_initialization_{};
    Fmi2Terminate terminate_{};
    Fmi2SetReal set_real_{};
    Fmi2GetReal get_real_{};
    Fmi2DoStep do_step_{};
    Fmi2GetFmuState get_state_{};
    Fmi2SetFmuState set_state_{};
    Fmi2FreeFmuState free_state_{};
    Fmi2CallbackFunctions callbacks_{};
    Fmi2Component instance_{};
    std::size_t warnings_{};
    bool in_initialization_{false};
    bool terminated_{false};
    bool state_enabled_{false};
    std::size_t rollback_replays_{};
};

class Fmi3CoupledSession final : public CoupledSession {
public:
    Fmi3CoupledSession(
        const std::filesystem::path& path,
        std::string instance_name,
        double end_time)
        : model_(import_fmu(path)), instance_name_(std::move(instance_name)),
          end_time_(end_time) {
        if (model_.fmi_version != "3.0") {
            throw std::invalid_argument("FMI 3 coupled session requires FMI 3.0 metadata");
        }
        interface_ = co_simulation_interface(model_);
        if (!model_.host_binary_candidate_available) {
            throw std::runtime_error("SSP component lacks a complete host CoSimulation binary");
        }
        const auto directory = prepare_fmu_directory(path, model_, interface_, temporary_);
        library_ = std::make_unique<DynamicLibrary>(
            directory / interface_binary_relative(model_, interface_));
        get_version_ = library_->symbol<Fmi3GetVersion>("fmi3GetVersion");
        instantiate_ = library_->symbol<Fmi3InstantiateCoSimulation>(
            "fmi3InstantiateCoSimulation");
        free_instance_ = library_->symbol<Fmi3FreeInstance>("fmi3FreeInstance");
        enter_initialization_ = library_->symbol<Fmi3EnterInitializationMode>(
            "fmi3EnterInitializationMode");
        exit_initialization_ = library_->symbol<Fmi3ExitInitializationMode>(
            "fmi3ExitInitializationMode");
        terminate_ = library_->symbol<Fmi3Terminate>("fmi3Terminate");
        set_float64_ = library_->symbol<Fmi3SetFloat64>("fmi3SetFloat64");
        get_float64_ = library_->symbol<Fmi3GetFloat64>("fmi3GetFloat64");
        do_step_ = library_->symbol<Fmi3DoStep>("fmi3DoStep");
        event_mode_enabled_ = capability_enabled(interface_, "hasEventMode");
        early_return_enabled_ = capability_enabled(
            interface_, "canReturnEarlyAfterIntermediateUpdate");
        state_enabled_ = capability_enabled(interface_, "canGetAndSetFMUState");
        if (event_mode_enabled_) {
            enter_event_mode_ = library_->symbol<Fmi3EnterEventMode>("fmi3EnterEventMode");
            update_discrete_states_ = library_->symbol<Fmi3UpdateDiscreteStates>(
                "fmi3UpdateDiscreteStates");
            enter_step_mode_ = library_->symbol<Fmi3EnterStepMode>("fmi3EnterStepMode");
        }
        if (state_enabled_) {
            get_state_ = library_->symbol<Fmi3GetFmuState>("fmi3GetFMUState");
            set_state_ = library_->symbol<Fmi3SetFmuState>("fmi3SetFMUState");
            free_state_ = library_->symbol<Fmi3FreeFmuState>("fmi3FreeFMUState");
        }
        const char* version = get_version_();
        if (version == nullptr || std::string_view(version) != "3.0") {
            throw std::runtime_error("SSP component runtime version is not FMI 3.0");
        }
        const auto resource_path = file_uri(directory / "resources");
        instance_ = instantiate_(
            instance_name_.c_str(), model_.instantiation_token.c_str(),
            resource_path.c_str(), false, false, event_mode_enabled_,
            early_return_enabled_, nullptr, 0,
            early_return_enabled_ ? &intermediate_update_context_ : nullptr,
            nullptr,
            early_return_enabled_ ? intermediate_update_callback : nullptr);
        if (instance_ == nullptr) throw std::runtime_error("SSP fmi3Instantiate returned null");
        try {
            require_status(
                enter_initialization_(instance_, true, 1.0e-6, 0.0, true, end_time),
                warnings_, "SSP fmi3EnterInitializationMode");
            in_initialization_ = true;
        } catch (...) {
            free_instance_(instance_);
            instance_ = nullptr;
            throw;
        }
    }

    Fmi3CoupledSession(const Fmi3CoupledSession&) = delete;
    Fmi3CoupledSession& operator=(const Fmi3CoupledSession&) = delete;

    ~Fmi3CoupledSession() override {
        if (instance_ != nullptr) free_instance_(instance_);
    }

    const FmiBlackboxIR& model() const override { return model_; }

    void validate_connector(
        const std::string& name,
        const std::string& causality,
        const std::string& unit = {}) const override {
        const auto iterator = std::find_if(
            model_.variables.begin(), model_.variables.end(), [&](const auto& variable) {
                return variable.name == name;
            });
        if (iterator == model_.variables.end() || iterator->type != "Float64" ||
            iterator->dimensions != 0 || iterator->causality != causality ||
            iterator->value_reference > std::numeric_limits<Fmi3ValueReference>::max() ||
            (!unit.empty() && iterator->unit != unit)) {
            throw std::invalid_argument(
                instance_name_ + "." + name +
                ": SSP connector does not match a scalar Float64 FMI " + causality);
        }
    }

    double output(const std::string& name) override {
        const auto reference = reference_for(name, "output");
        Fmi3Float64 value{};
        require_status(
            get_float64_(instance_, &reference, 1, &value, 1), warnings_,
            "SSP fmi3GetFloat64");
        if (!std::isfinite(value)) {
            throw std::runtime_error(instance_name_ + "." + name + ": non-finite SSP output");
        }
        return value;
    }

    void set_input(const std::string& name, double value) override {
        if (!std::isfinite(value)) throw std::runtime_error("non-finite SSP connection value");
        const auto reference = reference_for(name, "input");
        require_status(
            set_float64_(instance_, &reference, 1, &value, 1), warnings_,
            "SSP fmi3SetFloat64");
    }

    void finish_initialization() override {
        if (!in_initialization_) return;
        require_status(
            exit_initialization_(instance_), warnings_,
            "SSP fmi3ExitInitializationMode");
        in_initialization_ = false;
    }

    StepOutcome step(double current_time, double step_size) override {
        const double target_time = current_time + step_size;
        const bool expected_time_event = next_event_time_ &&
            std::abs(*next_event_time_ - target_time) <= 1.0e-12;
        if (next_event_time_ && *next_event_time_ < target_time - 1.0e-12) {
            throw std::runtime_error("SSP master stepped across FMI 3 nextEventTime");
        }
        Fmi3Boolean event_handling_needed{};
        Fmi3Boolean terminate_simulation{};
        Fmi3Boolean early_return{};
        Fmi3Float64 last_successful_time{};
        const auto status = do_step_(
            instance_, current_time, step_size, true, &event_handling_needed,
            &terminate_simulation, &early_return, &last_successful_time);
        if (status == Fmi3Status::warning) ++warnings_;
        else if (status != Fmi3Status::ok) {
            throw std::runtime_error("SSP fixed-step master rejects non-success FMI 3 status");
        }
        if (terminate_simulation || !std::isfinite(last_successful_time) ||
            last_successful_time <= current_time + 1.0e-12 ||
            last_successful_time > target_time + 1.0e-12) {
            throw std::runtime_error(
                "SSP master rejects FMI 3 termination or invalid step progress");
        }
        if (early_return) {
            if (!early_return_enabled_ || last_successful_time >= target_time - 1.0e-12) {
                throw std::runtime_error("SSP rejects invalid or undeclared FMI 3 early return");
            }
            ++early_returns_;
        } else if (std::abs(last_successful_time - target_time) > 1.0e-12) {
            throw std::runtime_error("SSP rejects incomplete FMI 3 step without early return");
        }
        const bool reached_expected_time_event = expected_time_event &&
            std::abs(last_successful_time - target_time) <= 1.0e-12;
        if (reached_expected_time_event && !event_handling_needed) {
            throw std::runtime_error(
                "SSP FMI 3 component did not request event handling at nextEventTime");
        }
        if (event_handling_needed) {
            if (!event_mode_enabled_) {
                throw std::runtime_error(
                    "SSP FMI 3 component requested event handling without hasEventMode");
            }
            require_status(
                enter_event_mode_(instance_), warnings_, "SSP fmi3EnterEventMode");
            ++event_mode_entries_;
            Fmi3Boolean update_needed{true};
            std::optional<double> updated_next_event_time;
            for (std::size_t iteration = 0; update_needed; ++iteration) {
                if (iteration >= 1024) {
                    throw std::runtime_error(
                        "SSP FMI 3 discrete update fixed point did not converge");
                }
                Fmi3Boolean terminate{};
                Fmi3Boolean nominals_changed{};
                Fmi3Boolean continuous_states_changed{};
                Fmi3Boolean next_event_time_defined{};
                Fmi3Float64 next_event_time{};
                require_status(
                    update_discrete_states_(
                        instance_, &update_needed, &terminate, &nominals_changed,
                        &continuous_states_changed, &next_event_time_defined,
                        &next_event_time),
                    warnings_, "SSP fmi3UpdateDiscreteStates");
                ++discrete_update_iterations_;
                if (terminate || nominals_changed || continuous_states_changed) {
                    throw std::runtime_error(
                        "SSP communication-point event rejects termination or continuous-state changes");
                }
                if (next_event_time_defined) {
                    if (!std::isfinite(next_event_time) ||
                        next_event_time <= last_successful_time + 1.0e-12 ||
                        next_event_time > end_time_ + 1.0e-12) {
                        throw std::runtime_error(
                            "SSP nextEventTime must be finite, future, and within the horizon");
                    }
                    updated_next_event_time = next_event_time;
                } else {
                    updated_next_event_time.reset();
                }
            }
            next_event_time_ = updated_next_event_time;
            if (reached_expected_time_event) ++time_events_;
            require_status(
                enter_step_mode_(instance_), warnings_, "SSP fmi3EnterStepMode");
        }
        return {last_successful_time, early_return};
    }

    std::size_t event_mode_entries() const override { return event_mode_entries_; }
    std::size_t discrete_update_iterations() const override {
        return discrete_update_iterations_;
    }
    std::size_t time_events() const override { return time_events_; }
    bool supports_variable_communication_step() const override {
        return capability_enabled(
            interface_, "canHandleVariableCommunicationStepSize");
    }
    std::optional<double> next_event_time() const override {
        return next_event_time_;
    }
    bool supports_state_rollback() const override { return state_enabled_; }
    bool may_return_early() const override { return early_return_enabled_; }

    class Fmi3Snapshot final : public Snapshot {
    public:
        Fmi3FmuState state{};
        Fmi3Instance instance{};
        Fmi3FreeFmuState free_state{};
        std::optional<double> next_event_time;
        std::size_t event_mode_entries{};
        std::size_t discrete_update_iterations{};
        std::size_t time_events{};
        std::size_t early_returns{};
        ~Fmi3Snapshot() override {
            if (state != nullptr) free_state(instance, &state);
        }
    };

    std::unique_ptr<Snapshot> save_state() override {
        if (!state_enabled_) {
            throw std::runtime_error("SSP rollback requires FMI 3 state capability");
        }
        auto snapshot = std::make_unique<Fmi3Snapshot>();
        snapshot->instance = instance_;
        snapshot->free_state = free_state_;
        snapshot->next_event_time = next_event_time_;
        snapshot->event_mode_entries = event_mode_entries_;
        snapshot->discrete_update_iterations = discrete_update_iterations_;
        snapshot->time_events = time_events_;
        snapshot->early_returns = early_returns_;
        require_status(
            get_state_(instance_, &snapshot->state), warnings_, "SSP fmi3GetFMUState");
        if (snapshot->state == nullptr) {
            throw std::runtime_error("SSP fmi3GetFMUState returned null");
        }
        return snapshot;
    }

    void restore_state(const Snapshot& snapshot) override {
        const auto* typed = dynamic_cast<const Fmi3Snapshot*>(&snapshot);
        if (typed == nullptr || typed->state == nullptr) {
            throw std::invalid_argument("invalid FMI 3 SSP snapshot");
        }
        require_status(
            set_state_(instance_, typed->state), warnings_, "SSP fmi3SetFMUState");
        next_event_time_ = typed->next_event_time;
        event_mode_entries_ = typed->event_mode_entries;
        discrete_update_iterations_ = typed->discrete_update_iterations;
        time_events_ = typed->time_events;
        early_returns_ = typed->early_returns;
        ++rollback_replays_;
    }
    std::size_t early_returns() const override { return early_returns_; }
    std::size_t rollback_replays() const override { return rollback_replays_; }

    void terminate() override {
        if (terminated_ || instance_ == nullptr) return;
        require_status(terminate_(instance_), warnings_, "SSP fmi3Terminate");
        terminated_ = true;
    }

private:
    Fmi3ValueReference reference_for(
        const std::string& name,
        const std::string& causality) const {
        validate_connector(name, causality);
        const auto iterator = std::find_if(
            model_.variables.begin(), model_.variables.end(), [&](const auto& variable) {
                return variable.name == name;
            });
        return static_cast<Fmi3ValueReference>(iterator->value_reference);
    }

    FmiBlackboxIR model_;
    FmiInterfaceIR interface_;
    std::string instance_name_;
    double end_time_{};
    std::unique_ptr<TemporaryDirectory> temporary_;
    std::unique_ptr<DynamicLibrary> library_;
    Fmi3GetVersion get_version_{};
    Fmi3InstantiateCoSimulation instantiate_{};
    Fmi3FreeInstance free_instance_{};
    Fmi3EnterInitializationMode enter_initialization_{};
    Fmi3ExitInitializationMode exit_initialization_{};
    Fmi3Terminate terminate_{};
    Fmi3SetFloat64 set_float64_{};
    Fmi3GetFloat64 get_float64_{};
    Fmi3DoStep do_step_{};
    Fmi3EnterEventMode enter_event_mode_{};
    Fmi3UpdateDiscreteStates update_discrete_states_{};
    Fmi3EnterStepMode enter_step_mode_{};
    Fmi3GetFmuState get_state_{};
    Fmi3SetFmuState set_state_{};
    Fmi3FreeFmuState free_state_{};
    Fmi3Instance instance_{};
    std::size_t warnings_{};
    bool in_initialization_{false};
    bool terminated_{false};
    bool event_mode_enabled_{false};
    bool early_return_enabled_{false};
    bool state_enabled_{false};
    std::size_t event_mode_entries_{};
    std::size_t discrete_update_iterations_{};
    std::size_t time_events_{};
    std::optional<double> next_event_time_;
    IntermediateUpdateContext intermediate_update_context_;
    std::size_t early_returns_{};
    std::size_t rollback_replays_{};
};

}  // namespace

FmiBlackboxIR import_fmu(const std::filesystem::path& path) {
    FmiBlackboxIR model;
    model.host_platform = host_platform();
    std::string descriptor;
    if (std::filesystem::is_directory(path)) {
        const auto descriptor_path = path / "modelDescription.xml";
        descriptor = read_text(descriptor_path, kMaximumDescriptorSize);
        model.source_hash = digest(descriptor);
        model.archive_entries = inventory_directory(path);
        model.warnings.push_back(
            "directory import hashes modelDescription.xml only; archive hash requires a .fmu file");
    } else {
        const auto archive = read_text(path, kMaximumArchiveSize);
        model.source_hash = digest(archive);
        const auto entries = zip_entries(archive);
        const auto iterator = std::find_if(entries.begin(), entries.end(), [](const auto& entry) {
            return entry.name == "modelDescription.xml";
        });
        if (iterator == entries.end()) throw std::invalid_argument("FMU lacks modelDescription.xml");
        descriptor = extract_zip_entry(archive, *iterator);
        for (const auto& entry : entries) model.archive_entries.push_back(entry.name);
        std::sort(model.archive_entries.begin(), model.archive_entries.end());
    }
    parse_model_description(descriptor, model);
    derive_binary_inventory(model);
    derive_capability_warnings(model);
    model.warnings.push_back(
        "blackbox-degraded: shared libraries were inventoried but never loaded or executed");
    model.warnings.push_back(
        "equation-level residual validation and Direct expert permission are forbidden");
    model.validate();
    return model;
}

FmiSmokeResult smoke_fmi2_co_simulation(
    const std::filesystem::path& path,
    double end_time,
    double step_size,
    const std::map<std::string, double>& inputs,
    bool allow_native_execution,
    std::size_t asynchronous_timeout_ms) {
    if (!allow_native_execution) {
        throw std::invalid_argument(
            "native FMU execution requires explicit --allow-native-execution");
    }
    if (std::filesystem::is_directory(path)) {
        throw std::invalid_argument(
            "native FMU execution requires a .fmu archive so source_hash binds the binary");
    }
    if (!(end_time > 0.0) || !(step_size > 0.0) ||
        !std::isfinite(end_time) || !std::isfinite(step_size)) {
        throw std::invalid_argument(
            "FMI smoke end time and step size must be finite and positive");
    }
    if (asynchronous_timeout_ms == 0 || asynchronous_timeout_ms > 60000U) {
        throw std::invalid_argument(
            "FMI 2 asynchronous timeout must be between 1 and 60000 milliseconds");
    }
    const double step_count = end_time / step_size;
    const auto rounded_steps = static_cast<std::size_t>(std::llround(step_count));
    if (rounded_steps == 0 || rounded_steps > 1000000U ||
        std::abs(step_count - static_cast<double>(rounded_steps)) >
            1.0e-10 * std::max(1.0, step_count)) {
        throw std::invalid_argument(
            "FMI smoke end time must be an integer multiple of step size");
    }

    const auto model = import_fmu(path);
    if (model.fmi_version != "2.0") {
        throw std::invalid_argument("FMI 2 native smoke requires FMI 2.0 metadata");
    }
    const auto& interface = co_simulation_interface(model);
    if (!model.host_binary_candidate_available) {
        throw std::runtime_error("no complete host CoSimulation binary candidate");
    }

    std::vector<Fmi2ValueReference> input_references;
    std::vector<Fmi2Real> input_values;
    for (const auto& [name, value] : inputs) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument(name + ": FMI input is NaN/Inf");
        }
        const auto& variable = scalar_real_variable(
            model, name, {"input", "parameter"});
        input_references.push_back(
            static_cast<Fmi2ValueReference>(variable.value_reference));
        input_values.push_back(value);
    }
    std::vector<const FmiVariableIR*> outputs;
    for (const auto& variable : model.variables) {
        if (variable.causality == "output" && variable.type == "Real" &&
            variable.dimensions == 0 &&
            variable.value_reference <=
                std::numeric_limits<Fmi2ValueReference>::max()) {
            outputs.push_back(&variable);
        }
    }
    if (outputs.empty()) {
        throw std::invalid_argument(
            "FMI smoke requires at least one scalar Real output");
    }
    std::vector<Fmi2ValueReference> output_references;
    for (const auto* variable : outputs) {
        output_references.push_back(
            static_cast<Fmi2ValueReference>(variable->value_reference));
    }

    std::unique_ptr<TemporaryDirectory> temporary;
    const auto directory = prepare_fmu_directory(path, model, interface, temporary);
    DynamicLibrary library(directory / interface_binary_relative(model, interface));
    const auto get_version = library.symbol<Fmi2GetVersion>("fmi2GetVersion");
    const auto instantiate = library.symbol<Fmi2Instantiate>("fmi2Instantiate");
    const auto free_instance = library.symbol<Fmi2FreeInstance>("fmi2FreeInstance");
    const auto setup_experiment =
        library.symbol<Fmi2SetupExperiment>("fmi2SetupExperiment");
    const auto enter_initialization = library.symbol<Fmi2EnterInitializationMode>(
        "fmi2EnterInitializationMode");
    const auto exit_initialization = library.symbol<Fmi2ExitInitializationMode>(
        "fmi2ExitInitializationMode");
    const auto terminate = library.symbol<Fmi2Terminate>("fmi2Terminate");
    const auto set_real = library.symbol<Fmi2SetReal>("fmi2SetReal");
    const auto get_real = library.symbol<Fmi2GetReal>("fmi2GetReal");
    const auto do_step = library.symbol<Fmi2DoStep>("fmi2DoStep");
    const auto get_real_status = library.optional_symbol<Fmi2GetRealStatus>(
        "fmi2GetRealStatus");
    const auto get_status = library.optional_symbol<Fmi2GetStatus>(
        "fmi2GetStatus");
    const auto cancel_step = library.optional_symbol<Fmi2CancelStep>(
        "fmi2CancelStep");
    const bool variable_step_enabled = capability_enabled(
        interface, "canHandleVariableCommunicationStepSize");
    const bool asynchronous_enabled = capability_enabled(
        interface, "canRunAsynchronuously");
    const bool state_enabled = capability_enabled(interface, "canGetAndSetFMUstate");
    Fmi2GetFmuState get_state{};
    Fmi2SetFmuState set_state{};
    Fmi2FreeFmuState free_state{};
    Fmi2SerializedFmuStateSize serialized_state_size{};
    Fmi2SerializeFmuState serialize_state{};
    Fmi2DeserializeFmuState deserialize_state{};
    if (state_enabled) {
        get_state = library.symbol<Fmi2GetFmuState>("fmi2GetFMUstate");
        set_state = library.symbol<Fmi2SetFmuState>("fmi2SetFMUstate");
        free_state = library.symbol<Fmi2FreeFmuState>("fmi2FreeFMUstate");
    }
    const bool serialization_enabled =
        capability_enabled(interface, "canSerializeFMUstate");
    if (serialization_enabled) {
        if (!state_enabled) {
            throw std::invalid_argument(
                "canSerializeFMUstate requires canGetAndSetFMUstate");
        }
        serialized_state_size = library.symbol<Fmi2SerializedFmuStateSize>(
            "fmi2SerializedFMUstateSize");
        serialize_state = library.symbol<Fmi2SerializeFmuState>(
            "fmi2SerializeFMUstate");
        deserialize_state = library.symbol<Fmi2DeserializeFmuState>(
            "fmi2DeSerializeFMUstate");
    }
    const char* runtime_version = get_version();
    if (runtime_version == nullptr || std::string_view(runtime_version) != "2.0") {
        throw std::runtime_error("FMU runtime version does not match FMI 2.0 metadata");
    }

    Fmi2AsyncContext asynchronous_context;
    Fmi2CallbackFunctions callbacks;
    callbacks.allocate_memory = std::calloc;
    callbacks.free_memory = std::free;
    callbacks.step_finished = fmi2_step_finished;
    callbacks.component_environment = &asynchronous_context;
    const auto resource_path = file_uri(directory / "resources");
    Fmi2Component instance = instantiate(
        "smave-smoke", Fmi2Type::co_simulation,
        model.instantiation_token.c_str(), resource_path.c_str(), &callbacks,
        false, false);
    if (instance == nullptr) {
        throw std::runtime_error("fmi2Instantiate returned null");
    }
    struct InstanceCleanup {
        Fmi2Component instance;
        Fmi2FreeInstance free_instance;
        ~InstanceCleanup() { if (instance != nullptr) free_instance(instance); }
    } cleanup{instance, free_instance};

    FmiSmokeResult result;
    result.model_name = model.model_name;
    result.source_hash = model.source_hash;
    result.interface_kind = interface.kind;
    result.model_identifier = interface.model_identifier;
    result.end_time = end_time;
    result.step_size = step_size;
    result.asynchronous_timeout_ms = asynchronous_timeout_ms;
    result.state_roundtrip_attempted = state_enabled;
    result.state_serialization_attempted = serialization_enabled;
    require_status(
        setup_experiment(instance, true, 1.0e-6, 0.0, true, end_time),
        result.warnings, "fmi2SetupExperiment");
    require_status(
        enter_initialization(instance), result.warnings,
        "fmi2EnterInitializationMode");
    if (!input_references.empty()) {
        require_status(
            set_real(instance, input_references.data(), input_references.size(),
                     input_values.data()),
            result.warnings, "fmi2SetReal");
    }
    require_status(
        exit_initialization(instance), result.warnings,
        "fmi2ExitInitializationMode");

    const auto sample_outputs = [&](double time) {
        std::vector<Fmi2Real> values(outputs.size());
        require_status(
            get_real(instance, output_references.data(), output_references.size(),
                     values.data()),
            result.warnings, "fmi2GetReal");
        FmiSmokeSample sample;
        sample.time = time;
        for (std::size_t index = 0; index < outputs.size(); ++index) {
            if (!std::isfinite(values[index])) {
                throw std::runtime_error(
                    outputs[index]->name + ": FMI output is NaN/Inf");
            }
            sample.outputs.emplace(outputs[index]->name, values[index]);
        }
        return sample;
    };
    result.samples.push_back(sample_outputs(0.0));

    Fmi2FmuState saved_state{};
    std::map<std::string, double> first_outputs;
    const auto advance_to = [&](double begin, double target) {
        double current_time = begin;
        std::size_t subdivisions{};
        while (current_time < target - 1.0e-12) {
            if (++subdivisions > 1024U) {
                throw std::runtime_error(
                    "too many FMI 2 Co-Simulation events in one communication step");
            }
            const auto callbacks_before = asynchronous_context.callbacks.load(
                std::memory_order_acquire);
            asynchronous_context.caller_thread = std::this_thread::get_id();
            asynchronous_context.cross_thread.store(
                false, std::memory_order_release);
            asynchronous_context.status.store(
                static_cast<int>(Fmi2Status::pending),
                std::memory_order_release);
            auto status = do_step(
                instance, current_time, target - current_time, true);
            ++result.do_step_calls;
            if (status == Fmi2Status::pending) {
                if (!asynchronous_enabled || get_status == nullptr ||
                    cancel_step == nullptr) {
                    throw std::runtime_error(
                        "FMI 2 Pending step requires declared asynchronous execution, fmi2GetStatus and fmi2CancelStep");
                }
                ++result.pending_steps;
                const auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(asynchronous_timeout_ms);
                Fmi2Status polled_status = Fmi2Status::pending;
                while (std::chrono::steady_clock::now() < deadline) {
                    require_status(
                        get_status(
                            instance, Fmi2StatusKind::do_step_status,
                            &polled_status),
                        result.warnings, "fmi2GetStatus(doStepStatus)");
                    if (polled_status != Fmi2Status::pending) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                if (polled_status == Fmi2Status::pending) {
                    require_status(
                        cancel_step(instance), result.warnings,
                        "fmi2CancelStep");
                    ++result.cancelled_steps;
                    throw std::runtime_error(
                        "FMI 2 asynchronous step timed out and was cancelled");
                }
                status = polled_status;
                while (asynchronous_context.callbacks.load(
                           std::memory_order_acquire) == callbacks_before &&
                       std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                const auto callbacks_after = asynchronous_context.callbacks.load(
                    std::memory_order_acquire);
                result.step_finished_callbacks += callbacks_after - callbacks_before;
                if (asynchronous_context.cross_thread.load(
                        std::memory_order_acquire)) {
                    ++result.cross_thread_callbacks;
                }
                if (callbacks_after == callbacks_before ||
                    static_cast<Fmi2Status>(asynchronous_context.status.load(
                        std::memory_order_acquire)) != status) {
                    throw std::runtime_error(
                        "FMI 2 asynchronous completion callback is missing or inconsistent");
                }
            }
            if (status == Fmi2Status::ok) {
                current_time = target;
                continue;
            }
            if (status == Fmi2Status::warning) {
                ++result.warnings;
                current_time = target;
                continue;
            }
            if (status != Fmi2Status::discard) {
                throw std::runtime_error(
                    "fmi2DoStep returned unsupported status " +
                    std::to_string(static_cast<int>(status)));
            }
            if (get_real_status == nullptr) {
                throw std::runtime_error(
                    "FMI 2 discarded step requires fmi2GetRealStatus");
            }
            if (!variable_step_enabled) {
                throw std::runtime_error(
                    "FMI 2 discarded step requires canHandleVariableCommunicationStepSize");
            }
            double last_successful_time{};
            require_status(
                get_real_status(
                    instance, Fmi2StatusKind::last_successful_time,
                    &last_successful_time),
                result.warnings, "fmi2GetRealStatus(lastSuccessfulTime)");
            if (!std::isfinite(last_successful_time) ||
                last_successful_time <= current_time + 1.0e-12 ||
                last_successful_time >= target - 1.0e-12) {
                throw std::runtime_error(
                    "FMI 2 discarded step did not report bounded interior progress");
            }
            current_time = last_successful_time;
            ++result.discard_recoveries;
        }
    };
    for (std::size_t step = 0; step < rounded_steps; ++step) {
        const double current_time = static_cast<double>(step) * step_size;
        if (step == 0 && state_enabled) {
            require_status(
                get_state(instance, &saved_state), result.warnings,
                "fmi2GetFMUstate");
            if (saved_state == nullptr) {
                throw std::runtime_error("fmi2GetFMUstate returned null");
            }
            if (serialization_enabled) {
                std::size_t serialized_size{};
                require_status(
                    serialized_state_size(instance, saved_state, &serialized_size),
                    result.warnings, "fmi2SerializedFMUstateSize");
                if (serialized_size == 0 || serialized_size > 16U * 1024U * 1024U) {
                    throw std::runtime_error(
                        "FMI serialized state size must be within 1 byte and 16 MiB");
                }
                std::vector<std::uint8_t> serialized(serialized_size);
                require_status(
                    serialize_state(instance, saved_state, serialized.data(),
                                    serialized.size()),
                    result.warnings, "fmi2SerializeFMUstate");
                require_status(
                    free_state(instance, &saved_state), result.warnings,
                    "fmi2FreeFMUstate before deserialization");
                require_status(
                    deserialize_state(instance, serialized.data(), serialized.size(),
                                      &saved_state),
                    result.warnings, "fmi2DeSerializeFMUstate");
                if (saved_state == nullptr) {
                    throw std::runtime_error("fmi2DeSerializeFMUstate returned null");
                }
                result.serialized_state_bytes = serialized.size();
            }
        }
        advance_to(current_time, current_time + step_size);
        result.samples.push_back(sample_outputs(current_time + step_size));
        if (step == 0 && state_enabled) {
            first_outputs = result.samples.back().outputs;
            require_status(
                set_state(instance, saved_state), result.warnings,
                "fmi2SetFMUstate");
            advance_to(0.0, step_size);
            const auto replay = sample_outputs(step_size);
            for (const auto& [name, expected] : first_outputs) {
                result.maximum_state_replay_error = std::max(
                    result.maximum_state_replay_error,
                    std::abs(replay.outputs.at(name) - expected));
            }
            result.state_roundtrip_passed =
                result.maximum_state_replay_error <= 1.0e-12;
            result.state_serialization_passed =
                serialization_enabled && result.state_roundtrip_passed;
            require_status(
                free_state(instance, &saved_state), result.warnings,
                "fmi2FreeFMUstate");
            if (saved_state != nullptr) {
                throw std::runtime_error(
                    "fmi2FreeFMUstate did not clear state");
            }
            if (!result.state_roundtrip_passed) {
                throw std::runtime_error("FMI state replay output mismatch");
            }
        }
    }
    require_status(terminate(instance), result.warnings, "fmi2Terminate");
    result.success = true;
    if (result.pending_steps > 0) {
        result.message =
            "opt-in FMI 2.0 Co-Simulation asynchronous Pending smoke and state replay completed";
    } else if (result.discard_recoveries > 0) {
        result.message =
            "opt-in FMI 2.0 Co-Simulation communication-point event smoke and state replay completed";
    } else {
        result.message =
            "opt-in FMI 2.0 Co-Simulation fixed-step smoke and state replay completed";
    }
    return result;
}

SspSimulationResult simulate_ssp(
    const std::filesystem::path& path,
    double end_time,
    double step_size,
    bool allow_native_execution) {
    if (!allow_native_execution) {
        throw std::invalid_argument(
            "native SSP execution requires explicit --allow-native-execution");
    }
    if (std::filesystem::is_directory(path)) {
        throw std::invalid_argument("SSP execution requires a .ssp ZIP archive");
    }
    if (!(end_time > 0.0) || !(step_size > 0.0) ||
        !std::isfinite(end_time) || !std::isfinite(step_size)) {
        throw std::invalid_argument("SSP end time and step size must be finite and positive");
    }
    const double step_count = end_time / step_size;
    const auto rounded_steps = static_cast<std::size_t>(std::llround(step_count));
    if (rounded_steps == 0 || rounded_steps > 1000000U ||
        std::abs(step_count - static_cast<double>(rounded_steps)) >
            1.0e-10 * std::max(1.0, step_count)) {
        throw std::invalid_argument("SSP end time must be an integer multiple of step size");
    }
    const auto archive = read_text(path, kMaximumArchiveSize);
    const auto entries = zip_entries(archive);
    const auto descriptor_entry = std::find_if(
        entries.begin(), entries.end(), [](const auto& entry) {
            return entry.name == "SystemStructure.ssd";
        });
    if (descriptor_entry == entries.end()) {
        throw std::invalid_argument("SSP archive lacks SystemStructure.ssd");
    }
    auto system = parse_ssp_system(extract_zip_entry(archive, *descriptor_entry));
    std::unordered_map<std::string, std::size_t> component_indices;
    for (std::size_t index = 0; index < system.components.size(); ++index) {
        const auto& component = system.components[index];
        if (!component_indices.emplace(component.name, index).second) {
            throw std::invalid_argument("duplicate SSP component name");
        }
        const std::filesystem::path source(component.source);
        if (!component.source.starts_with("resources/") ||
            source.extension() != ".fmu" || source.filename().empty()) {
            throw std::invalid_argument(
                component.name + ": SSP component source must be resources/*.fmu");
        }
    }
    std::vector<std::vector<std::size_t>> outgoing(system.components.size());
    std::vector<std::size_t> indegree(system.components.size(), 0);
    std::set<std::pair<std::size_t, std::string>> driven_inputs;
    for (std::size_t connection_index = 0;
         connection_index < system.connections.size(); ++connection_index) {
        auto& connection = system.connections[connection_index];
        const auto source = component_indices.find(connection.source_component);
        const auto target = component_indices.find(connection.target_component);
        if (source == component_indices.end() || target == component_indices.end()) {
            throw std::invalid_argument("SSP Connection references an unknown component");
        }
        if (source->second == target->second) {
            throw std::invalid_argument("SSP self-connections are unsupported");
        }
        const auto source_connector =
            system.components[source->second].connectors.find(connection.source_connector);
        const auto target_connector =
            system.components[target->second].connectors.find(connection.target_connector);
        if (source_connector == system.components[source->second].connectors.end() ||
            source_connector->second.kind != "output" ||
            target_connector == system.components[target->second].connectors.end() ||
            target_connector->second.kind != "input") {
            throw std::invalid_argument("SSP Connection must connect output to input");
        }
        if (!driven_inputs.emplace(target->second, connection.target_connector).second) {
            throw std::invalid_argument("SSP input connector has multiple drivers");
        }
        connection.source_unit = source_connector->second.unit;
        connection.target_unit = target_connector->second.unit;
        const auto resolve_unit = [&](const std::string& name) -> const SspUnitDefinition* {
            if (name.empty()) return nullptr;
            const auto unit = system.units.find(name);
            if (unit == system.units.end()) {
                throw std::invalid_argument("SSP connector references an unknown Unit");
            }
            return &unit->second;
        };
        const auto* source_unit = resolve_unit(connection.source_unit);
        const auto* target_unit = resolve_unit(connection.target_unit);
        if (!connection.suppress_unit_conversion &&
            (source_unit != nullptr || target_unit != nullptr)) {
            if (source_unit == nullptr || target_unit == nullptr) {
                throw std::invalid_argument(
                    "SSP automatic unit conversion requires units on both connectors");
            }
            if (source_unit->exponents != target_unit->exponents) {
                throw std::invalid_argument(
                    "SSP automatic unit conversion requires compatible BaseUnit dimensions");
            }
            connection.unit_factor = source_unit->factor / target_unit->factor;
            connection.unit_offset =
                (source_unit->offset - target_unit->offset) / target_unit->factor;
            if (!std::isfinite(connection.unit_factor) ||
                !std::isfinite(connection.unit_offset)) {
                throw std::invalid_argument(
                    "SSP automatic unit conversion coefficients must be finite");
            }
        }
        outgoing[source->second].push_back(connection_index);
        ++indegree[target->second];
    }
    if (system.connections.empty()) {
        throw std::invalid_argument("SSP master requires at least one component connection");
    }
    for (std::size_t index = 0; index < system.components.size(); ++index) {
        for (const auto& __entry : system.components[index].connectors) {

            const auto& name = __entry.first;

            const auto& connector = __entry.second;
            if (connector.kind == "input" && !driven_inputs.contains({index, name})) {
                throw std::invalid_argument("SSP input connector is unconnected");
            }
        }
    }
    std::vector<std::size_t> ready;
    for (std::size_t index = 0; index < indegree.size(); ++index) {
        if (indegree[index] == 0) ready.push_back(index);
    }
    std::vector<std::size_t> order;
    while (!ready.empty()) {
        const auto selected = ready.front();
        ready.erase(ready.begin());
        order.push_back(selected);
        for (const auto connection_index : outgoing[selected]) {
            const auto target = component_indices.at(
                system.connections[connection_index].target_component);
            if (--indegree[target] == 0) {
                const auto position = std::lower_bound(ready.begin(), ready.end(), target);
                ready.insert(position, target);
            }
        }
    }
    if (order.size() != system.components.size()) {
        throw std::invalid_argument("SSP feed-forward master rejects algebraic connection loops");
    }

    TemporaryDirectory temporary(digest(archive));
    std::vector<std::filesystem::path> component_paths;
    component_paths.reserve(system.components.size());
    std::size_t embedded_fmu_bytes{};
    for (const auto& component : system.components) {
        const auto entry = std::find_if(
            entries.begin(), entries.end(), [&](const auto& candidate) {
                return candidate.name == component.source;
            });
        if (entry == entries.end()) {
            throw std::invalid_argument(component.name + ": SSP component FMU is missing");
        }
        if (entry->uncompressed_size > kMaximumArchiveSize - embedded_fmu_bytes) {
            throw std::runtime_error("SSP embedded FMUs exceed aggregate extraction limit");
        }
        embedded_fmu_bytes += entry->uncompressed_size;
        const auto destination = temporary.path() / component.source;
        write_binary_file(
            destination, extract_zip_entry(archive, *entry, kMaximumArchiveSize));
        component_paths.push_back(destination);
    }
    std::vector<std::unique_ptr<CoupledSession>> sessions;
    sessions.reserve(system.components.size());
    SspSimulationResult result;
    result.system_name = system.name;
    result.source_hash = digest(archive);
    result.end_time = end_time;
    result.step_size = step_size;
    result.communication_steps = rounded_steps;
    for (std::size_t index = 0; index < system.components.size(); ++index) {
        const auto metadata = import_fmu(component_paths[index]);
        const auto fmu_units = parse_unit_definitions(
            fmu_model_description(component_paths[index]));
        if (metadata.fmi_version == "2.0") {
            sessions.push_back(std::make_unique<Fmi2CoupledSession>(
                component_paths[index], system.components[index].name, end_time));
        } else if (metadata.fmi_version == "3.0") {
            sessions.push_back(std::make_unique<Fmi3CoupledSession>(
                component_paths[index], system.components[index].name, end_time));
        } else {
            throw std::invalid_argument("SSP component requires FMI 2.0 or 3.0 metadata");
        }
        result.components.push_back({
            system.components[index].name,
            system.components[index].source,
            sessions.back()->model().fmi_version,
            sessions.back()->model().model_name,
            sessions.back()->model().source_hash,
            0,
            0,
            0});
        for (const auto& __entry : system.components[index].connectors) {

            const auto& name = __entry.first;

            const auto& connector = __entry.second;
            sessions.back()->validate_connector(name, connector.kind, connector.unit);
            if (!connector.unit.empty()) {
                const auto ssp_unit = system.units.find(connector.unit);
                const auto fmu_unit = fmu_units.find(connector.unit);
                if (ssp_unit == system.units.end() || fmu_unit == fmu_units.end() ||
                    ssp_unit->second.exponents != fmu_unit->second.exponents ||
                    ssp_unit->second.factor != fmu_unit->second.factor ||
                    ssp_unit->second.offset != fmu_unit->second.offset) {
                    throw std::invalid_argument(
                        system.components[index].name + "." + name +
                        ": SSP Unit does not match embedded FMU UnitDefinitions");
                }
            }
        }
    }
    for (const auto& connection : system.connections) {
        result.connections.push_back({
            connection.source_component, connection.source_connector,
            connection.target_component, connection.target_connector,
            connection.source_unit, connection.target_unit,
            connection.unit_factor, connection.unit_offset,
            connection.factor, connection.offset});
    }
    for (const auto index : order) {
        result.step_order.push_back(system.components[index].name);
    }
    const auto exchange_and_sample = [&](double time) {
        for (const auto source_index : order) {
            for (const auto connection_index : outgoing[source_index]) {
                const auto& connection = system.connections[connection_index];
                const double source_value = sessions[source_index]->output(
                    connection.source_connector);
                const double converted_value = std::fma(
                    connection.unit_factor, source_value, connection.unit_offset);
                const double value = std::fma(
                    connection.factor, converted_value, connection.offset);
                if (!std::isfinite(value)) {
                    throw std::runtime_error(
                        "SSP LinearTransformation produced non-finite signal");
                }
                sessions[component_indices.at(connection.target_component)]->set_input(
                    connection.target_connector, value);
                ++result.signal_exchanges;
            }
        }
        SspComponentSample sample;
        sample.time = time;
        for (std::size_t index = 0; index < system.components.size(); ++index) {
            for (const auto& [name, connector] : system.components[index].connectors) {
                if (connector.kind == "output") {
                    sample.outputs.emplace(
                        system.components[index].name + "." + name,
                        sessions[index]->output(name));
                }
            }
        }
        result.samples.push_back(std::move(sample));
    };
    exchange_and_sample(0.0);
    for (auto& session : sessions) session->finish_initialization();
    for (std::size_t step = 0; step < rounded_steps; ++step) {
        double current_time = static_cast<double>(step) * step_size;
        const double communication_time = step + 1 == rounded_steps
            ? end_time : static_cast<double>(step + 1) * step_size;
        for (std::size_t substep = 0;
             current_time < communication_time - 1.0e-12; ++substep) {
            if (substep >= 1024) {
                throw std::runtime_error(
                    "SSP time-event splitting exceeded 1024 substeps per communication step");
            }
            double target_time = communication_time;
            for (const auto& session : sessions) {
                const auto next_event = session->next_event_time();
                if (next_event && *next_event > current_time + 1.0e-12 &&
                    *next_event < target_time - 1.0e-12) {
                    target_time = *next_event;
                }
            }
            const bool internal_event_point =
                target_time < communication_time - 1.0e-12;
            if (internal_event_point && std::any_of(
                    sessions.begin(), sessions.end(), [](const auto& session) {
                        return !session->supports_variable_communication_step();
                    })) {
                throw std::runtime_error(
                    "SSP time-event splitting requires variable communication steps on every component");
            }
            for (const auto index : order) {
                sessions[index]->step(current_time, target_time - current_time);
            }
            exchange_and_sample(target_time);
            if (internal_event_point) ++result.time_event_splits;
            current_time = target_time;
        }
    }
    for (auto& session : sessions) session->terminate();
    for (std::size_t index = 0; index < sessions.size(); ++index) {
        result.components[index].event_mode_entries =
            sessions[index]->event_mode_entries();
        result.components[index].discrete_update_iterations =
            sessions[index]->discrete_update_iterations();
        result.components[index].time_events = sessions[index]->time_events();
        result.event_mode_entries += result.components[index].event_mode_entries;
        result.discrete_update_iterations +=
            result.components[index].discrete_update_iterations;
        result.time_events += result.components[index].time_events;
    }
    result.success = true;
    result.message =
        "restricted SSP 1.0 macro-grid FMI 2/3 feed-forward master completed";
    return result;
}

FmiSmokeResult smoke_fmi3_co_simulation(
    const std::filesystem::path& path,
    double end_time,
    double step_size,
    const std::map<std::string, double>& inputs,
    bool allow_native_execution) {
    if (!allow_native_execution) {
        throw std::invalid_argument(
            "native FMU execution requires explicit --allow-native-execution");
    }
    if (std::filesystem::is_directory(path)) {
        throw std::invalid_argument(
            "native FMU execution requires a .fmu archive so source_hash binds the binary");
    }
    if (!(end_time > 0.0) || !(step_size > 0.0) ||
        !std::isfinite(end_time) || !std::isfinite(step_size)) {
        throw std::invalid_argument("FMI smoke end time and step size must be finite and positive");
    }
    const double step_count = end_time / step_size;
    const auto rounded_steps = static_cast<std::size_t>(std::llround(step_count));
    if (rounded_steps == 0 || rounded_steps > 1000000U ||
        std::abs(step_count - static_cast<double>(rounded_steps)) >
            1.0e-10 * std::max(1.0, step_count)) {
        throw std::invalid_argument("FMI smoke end time must be an integer multiple of step size");
    }

    const auto model = import_fmu(path);
    if (model.fmi_version != "3.0") {
        throw std::invalid_argument("native smoke currently supports only FMI 3.0");
    }
    const auto& interface = co_simulation_interface(model);
    if (!model.host_binary_candidate_available) {
        throw std::runtime_error("no complete host CoSimulation binary candidate");
    }
    std::vector<Fmi3ValueReference> input_references;
    std::vector<Fmi3Float64> input_values;
    for (const auto& [name, value] : inputs) {
        if (!std::isfinite(value)) throw std::invalid_argument(name + ": FMI input is NaN/Inf");
        const auto& variable = scalar_float64_variable(
            model, name, {"input", "parameter"});
        input_references.push_back(static_cast<Fmi3ValueReference>(variable.value_reference));
        input_values.push_back(value);
    }
    std::vector<const FmiVariableIR*> outputs;
    for (const auto& variable : model.variables) {
        if (variable.causality == "output" && variable.type == "Float64" &&
            variable.dimensions == 0 &&
            variable.value_reference <= std::numeric_limits<Fmi3ValueReference>::max()) {
            outputs.push_back(&variable);
        }
    }
    if (outputs.empty()) {
        throw std::invalid_argument("FMI smoke requires at least one scalar Float64 output");
    }
    std::vector<Fmi3ValueReference> output_references;
    for (const auto* variable : outputs) {
        output_references.push_back(static_cast<Fmi3ValueReference>(variable->value_reference));
    }

    std::unique_ptr<TemporaryDirectory> temporary;
    const auto directory = prepare_fmu_directory(path, model, interface, temporary);
    const auto binary = directory / interface_binary_relative(model, interface);
    DynamicLibrary library(binary);
    const auto instantiate = library.symbol<Fmi3InstantiateCoSimulation>(
        "fmi3InstantiateCoSimulation");
    const auto get_version = library.symbol<Fmi3GetVersion>("fmi3GetVersion");
    const auto free_instance = library.symbol<Fmi3FreeInstance>("fmi3FreeInstance");
    const auto enter_initialization = library.symbol<Fmi3EnterInitializationMode>(
        "fmi3EnterInitializationMode");
    const auto exit_initialization = library.symbol<Fmi3ExitInitializationMode>(
        "fmi3ExitInitializationMode");
    const auto terminate = library.symbol<Fmi3Terminate>("fmi3Terminate");
    const auto set_float64 = library.symbol<Fmi3SetFloat64>("fmi3SetFloat64");
    const auto get_float64 = library.symbol<Fmi3GetFloat64>("fmi3GetFloat64");
    const auto do_step = library.symbol<Fmi3DoStep>("fmi3DoStep");
    const bool event_mode_enabled = capability_enabled(interface, "hasEventMode");
    const bool early_return_enabled = capability_enabled(
        interface, "canReturnEarlyAfterIntermediateUpdate");
    Fmi3EnterEventMode enter_event_mode{};
    Fmi3UpdateDiscreteStates update_discrete_states{};
    Fmi3EnterStepMode enter_step_mode{};
    if (event_mode_enabled) {
        enter_event_mode = library.symbol<Fmi3EnterEventMode>("fmi3EnterEventMode");
        update_discrete_states = library.symbol<Fmi3UpdateDiscreteStates>(
            "fmi3UpdateDiscreteStates");
        enter_step_mode = library.symbol<Fmi3EnterStepMode>("fmi3EnterStepMode");
    }
    const char* runtime_version = get_version();
    if (runtime_version == nullptr || std::string_view(runtime_version) != "3.0") {
        throw std::runtime_error("FMU runtime version does not match FMI 3.0 metadata");
    }

    const auto resource_path = file_uri(directory / "resources");
    IntermediateUpdateContext intermediate_update_context;
    Fmi3Instance instance = instantiate(
        "smave-smoke", model.instantiation_token.c_str(), resource_path.c_str(),
        false, false, event_mode_enabled, early_return_enabled, nullptr, 0,
        early_return_enabled ? &intermediate_update_context : nullptr, nullptr,
        early_return_enabled ? intermediate_update_callback : nullptr);
    if (instance == nullptr) throw std::runtime_error("fmi3InstantiateCoSimulation returned null");
    struct InstanceCleanup {
        Fmi3Instance instance;
        Fmi3FreeInstance free_instance;
        ~InstanceCleanup() { if (instance != nullptr) free_instance(instance); }
    } cleanup{instance, free_instance};

    FmiSmokeResult result;
    result.model_name = model.model_name;
    result.source_hash = model.source_hash;
    result.interface_kind = interface.kind;
    result.model_identifier = interface.model_identifier;
    result.end_time = end_time;
    result.step_size = step_size;
    require_status(
        enter_initialization(instance, true, 1.0e-6, 0.0, true, end_time),
        result.warnings, "fmi3EnterInitializationMode");
    if (!input_references.empty()) {
        require_status(
            set_float64(instance, input_references.data(), input_references.size(),
                        input_values.data(), input_values.size()),
            result.warnings, "fmi3SetFloat64");
    }
    require_status(
        exit_initialization(instance), result.warnings, "fmi3ExitInitializationMode");

    const auto sample_outputs = [&](double time) {
        std::vector<Fmi3Float64> values(outputs.size());
        require_status(
            get_float64(instance, output_references.data(), output_references.size(),
                        values.data(), values.size()),
            result.warnings, "fmi3GetFloat64");
        FmiSmokeSample sample;
        sample.time = time;
        for (std::size_t index = 0; index < outputs.size(); ++index) {
            if (!std::isfinite(values[index])) {
                throw std::runtime_error(outputs[index]->name + ": FMU output is NaN/Inf");
            }
            sample.outputs.emplace(outputs[index]->name, values[index]);
        }
        return sample;
    };
    result.samples.push_back(sample_outputs(0.0));

    std::optional<double> next_time_event;
    const auto process_event_mode = [&](double event_time) {
        if (!event_mode_enabled) {
            throw std::runtime_error(
                "FMU requested event handling without declaring CoSimulation hasEventMode");
        }
        require_status(
            enter_event_mode(instance), result.warnings, "fmi3EnterEventMode");
        ++result.event_mode_entries;
        Fmi3Boolean update_needed = true;
        std::size_t iterations{};
        std::optional<double> updated_next_time;
        while (update_needed) {
            if (++iterations > 1024U) {
                throw std::runtime_error("FMI event iteration did not reach a fixed point");
            }
            Fmi3Boolean terminate_simulation{};
            Fmi3Boolean nominals_changed{};
            Fmi3Boolean continuous_states_changed{};
            Fmi3Boolean next_event_time_defined{};
            Fmi3Float64 next_event_time{};
            require_status(
                update_discrete_states(
                    instance, &update_needed, &terminate_simulation, &nominals_changed,
                    &continuous_states_changed, &next_event_time_defined, &next_event_time),
                result.warnings, "fmi3UpdateDiscreteStates");
            ++result.discrete_update_iterations;
            if (terminate_simulation || nominals_changed || continuous_states_changed) {
                throw std::runtime_error(
                    "FMI CoSimulation event smoke does not accept termination or continuous-state changes");
            }
            if (next_event_time_defined) {
                if (!std::isfinite(next_event_time) ||
                    next_event_time <= event_time + 1.0e-12 ||
                    next_event_time > end_time + 1.0e-12) {
                    throw std::runtime_error(
                        "FMI nextEventTime must be finite, future, and within the smoke horizon");
                }
                updated_next_time = next_event_time;
            } else {
                updated_next_time.reset();
            }
        }
        next_time_event = updated_next_time;
        require_status(
            enter_step_mode(instance), result.warnings, "fmi3EnterStepMode");
    };

    Fmi3GetFmuState get_state{};
    Fmi3SetFmuState set_state{};
    Fmi3FreeFmuState free_state{};
    Fmi3SerializedFmuStateSize serialized_state_size{};
    Fmi3SerializeFmuState serialize_state{};
    Fmi3DeserializeFmuState deserialize_state{};
    if (capability_enabled(interface, "canGetAndSetFMUState")) {
        get_state = library.symbol<Fmi3GetFmuState>("fmi3GetFMUState");
        set_state = library.symbol<Fmi3SetFmuState>("fmi3SetFMUState");
        free_state = library.symbol<Fmi3FreeFmuState>("fmi3FreeFMUState");
        result.state_roundtrip_attempted = true;
    }
    const bool serialization_enabled =
        capability_enabled(interface, "canSerializeFMUState");
    if (serialization_enabled) {
        if (!result.state_roundtrip_attempted) {
            throw std::invalid_argument(
                "canSerializeFMUState requires canGetAndSetFMUState");
        }
        serialized_state_size = library.symbol<Fmi3SerializedFmuStateSize>(
            "fmi3SerializedFMUStateSize");
        serialize_state = library.symbol<Fmi3SerializeFmuState>(
            "fmi3SerializeFMUState");
        deserialize_state = library.symbol<Fmi3DeserializeFmuState>(
            "fmi3DeserializeFMUState");
        result.state_serialization_attempted = true;
    }
    Fmi3FmuState saved_state{};
    struct StateCleanup {
        Fmi3Instance instance{};
        Fmi3FreeFmuState free_state{};
        Fmi3FmuState* state{};
        ~StateCleanup() {
            if (free_state != nullptr && state != nullptr && *state != nullptr) {
                (void)free_state(instance, state);
            }
        }
    } state_cleanup{instance, free_state, &saved_state};
    const auto advance_macro_step = [&](double start_time, double target_time) {
        double current_time = start_time;
        std::size_t substeps{};
        while (current_time < target_time - 1.0e-12) {
            if (++substeps > 1024U) {
                throw std::runtime_error(
                    "FMI early-return substeps did not reach the communication point");
            }
            const bool reaching_time_event = next_time_event.has_value() &&
                *next_time_event <= target_time + 1.0e-12;
            const double requested_target = reaching_time_event
                ? *next_time_event
                : target_time;
            if (reaching_time_event) ++result.time_event_splits;
            Fmi3Boolean event_handling_needed{};
            Fmi3Boolean terminate_simulation{};
            Fmi3Boolean early_return{};
            Fmi3Float64 last_successful_time{};
            require_status(
                do_step(instance, current_time, requested_target - current_time, false,
                        &event_handling_needed, &terminate_simulation, &early_return,
                        &last_successful_time),
                result.warnings, "fmi3DoStep");
            ++result.do_step_calls;
            if (terminate_simulation) {
                throw std::runtime_error("FMI smoke does not accept termination requests");
            }
            if (!std::isfinite(last_successful_time) ||
                last_successful_time <= current_time + 1.0e-12 ||
                last_successful_time > requested_target + 1.0e-12) {
                throw std::runtime_error(
                    "FMI doStep did not make bounded forward progress");
            }
            if (early_return) {
                if (!early_return_enabled) {
                    throw std::runtime_error(
                        "FMU returned early without declaring canReturnEarlyAfterIntermediateUpdate");
                }
                if (last_successful_time >= requested_target - 1.0e-12) {
                    throw std::runtime_error(
                        "FMU reported early return at the requested communication point");
                }
                ++result.early_returns;
            } else if (std::abs(last_successful_time - requested_target) > 1.0e-12) {
                throw std::runtime_error(
                    "FMI doStep returned an incomplete step without earlyReturn");
            }
            current_time = last_successful_time;
            const bool reached_scheduled_event = reaching_time_event &&
                std::abs(current_time - requested_target) <= 1.0e-12;
            if (reached_scheduled_event && !event_handling_needed) {
                throw std::runtime_error(
                    "FMU did not request event handling at nextEventTime");
            }
            if (event_handling_needed) {
                if (reached_scheduled_event) ++result.time_events;
                process_event_mode(current_time);
            }
        }
    };
    std::map<std::string, double> first_outputs;
    std::optional<double> saved_next_time_event;
    for (std::size_t step = 0; step < rounded_steps; ++step) {
        const double current_time = static_cast<double>(step) * step_size;
        if (step == 0 && result.state_roundtrip_attempted) {
            require_status(
                get_state(instance, &saved_state), result.warnings, "fmi3GetFMUState");
            if (saved_state == nullptr) throw std::runtime_error("fmi3GetFMUState returned null");
            if (serialization_enabled) {
                std::size_t serialized_size{};
                require_status(
                    serialized_state_size(instance, saved_state, &serialized_size),
                    result.warnings, "fmi3SerializedFMUStateSize");
                if (serialized_size == 0 || serialized_size > 16U * 1024U * 1024U) {
                    throw std::runtime_error(
                        "FMI serialized state size must be within 1 byte and 16 MiB");
                }
                std::vector<std::uint8_t> serialized(serialized_size);
                require_status(
                    serialize_state(instance, saved_state, serialized.data(),
                                    serialized.size()),
                    result.warnings, "fmi3SerializeFMUState");
                require_status(
                    free_state(instance, &saved_state), result.warnings,
                    "fmi3FreeFMUState before deserialization");
                require_status(
                    deserialize_state(instance, serialized.data(), serialized.size(),
                                      &saved_state),
                    result.warnings, "fmi3DeserializeFMUState");
                if (saved_state == nullptr) {
                    throw std::runtime_error("fmi3DeserializeFMUState returned null");
                }
                result.serialized_state_bytes = serialized.size();
            }
            saved_next_time_event = next_time_event;
        }
        advance_macro_step(current_time, current_time + step_size);
        result.samples.push_back(sample_outputs(current_time + step_size));
        if (step == 0 && result.state_roundtrip_attempted) {
            first_outputs = result.samples.back().outputs;
            require_status(
                set_state(instance, saved_state), result.warnings, "fmi3SetFMUState");
            next_time_event = saved_next_time_event;
            advance_macro_step(0.0, step_size);
            const auto replay = sample_outputs(step_size);
            for (const auto& [name, expected] : first_outputs) {
                result.maximum_state_replay_error = std::max(
                    result.maximum_state_replay_error,
                    std::abs(replay.outputs.at(name) - expected));
            }
            result.state_roundtrip_passed = result.maximum_state_replay_error <= 1.0e-12;
            result.state_serialization_passed =
                serialization_enabled && result.state_roundtrip_passed;
            require_status(
                free_state(instance, &saved_state), result.warnings, "fmi3FreeFMUState");
            if (saved_state != nullptr) throw std::runtime_error("fmi3FreeFMUState did not clear state");
            if (!result.state_roundtrip_passed) {
                throw std::runtime_error("FMI state replay output mismatch");
            }
        }
    }
    require_status(terminate(instance), result.warnings, "fmi3Terminate");
    result.success = true;
    result.message =
        "opt-in FMI 3.0 Co-Simulation fixed-step smoke and state replay completed";
    return result;
}

FmiSmokeResult smoke_fmi3_scheduled_execution(
    const std::filesystem::path& path,
    double end_time,
    double interval,
    const std::map<std::string, double>& inputs,
    bool allow_native_execution) {
    if (!allow_native_execution) {
        throw std::invalid_argument(
            "native FMU execution requires explicit --allow-native-execution");
    }
    if (std::filesystem::is_directory(path)) {
        throw std::invalid_argument(
            "native FMU execution requires a .fmu archive so source_hash binds the binary");
    }
    if (!(end_time > 0.0) || !(interval > 0.0) ||
        !std::isfinite(end_time) || !std::isfinite(interval)) {
        throw std::invalid_argument(
            "FMI Scheduled Execution end time and interval must be finite and positive");
    }
    const double step_count = end_time / interval;
    const auto rounded_steps = static_cast<std::size_t>(std::llround(step_count));
    if (rounded_steps == 0 || rounded_steps > 1000000U ||
        std::abs(step_count - static_cast<double>(rounded_steps)) >
            1.0e-10 * std::max(1.0, step_count)) {
        throw std::invalid_argument(
            "FMI Scheduled Execution end time must be an integer multiple of interval");
    }

    const auto model = import_fmu(path);
    if (model.fmi_version != "3.0") {
        throw std::invalid_argument("Scheduled Execution requires FMI 3.0");
    }
    const auto& interface = scheduled_execution_interface(model);
    if (!model.host_binary_candidate_available) {
        throw std::runtime_error("no complete host ScheduledExecution binary candidate");
    }

    std::vector<const FmiVariableIR*> clocks;
    for (const auto& variable : model.variables) {
        if (variable.type != "Clock") continue;
        if (variable.causality != "input") continue;
        if (variable.dimensions != 0 ||
            variable.value_reference > std::numeric_limits<Fmi3ValueReference>::max()) {
            throw std::invalid_argument(
                "Scheduled Execution smoke requires scalar input Clocks");
        }
        clocks.push_back(&variable);
    }
    if (clocks.empty()) {
        throw std::invalid_argument(
            "Scheduled Execution smoke requires at least one scalar input Clock");
    }
    std::sort(clocks.begin(), clocks.end(), [](const auto* left, const auto* right) {
        if (left->value_reference != right->value_reference) {
            return left->value_reference < right->value_reference;
        }
        return left->name < right->name;
    });

    std::vector<Fmi3ValueReference> input_references;
    std::vector<Fmi3Float64> input_values;
    for (const auto& [name, value] : inputs) {
        if (!std::isfinite(value)) throw std::invalid_argument(name + ": FMI input is NaN/Inf");
        const auto& variable = scalar_float64_variable(
            model, name, {"input", "parameter"});
        input_references.push_back(static_cast<Fmi3ValueReference>(variable.value_reference));
        input_values.push_back(value);
    }
    std::vector<const FmiVariableIR*> outputs;
    std::vector<Fmi3ValueReference> output_references;
    for (const auto& variable : model.variables) {
        if (variable.causality == "output" && variable.type == "Float64" &&
            variable.dimensions == 0 &&
            variable.value_reference <= std::numeric_limits<Fmi3ValueReference>::max()) {
            outputs.push_back(&variable);
            output_references.push_back(
                static_cast<Fmi3ValueReference>(variable.value_reference));
        }
    }
    if (outputs.empty()) {
        throw std::invalid_argument(
            "Scheduled Execution smoke requires at least one scalar Float64 output");
    }

    std::unique_ptr<TemporaryDirectory> temporary;
    const auto directory = prepare_fmu_directory(path, model, interface, temporary);
    DynamicLibrary library(directory / interface_binary_relative(model, interface));
    const auto instantiate = library.symbol<Fmi3InstantiateScheduledExecution>(
        "fmi3InstantiateScheduledExecution");
    const auto get_version = library.symbol<Fmi3GetVersion>("fmi3GetVersion");
    const auto free_instance = library.symbol<Fmi3FreeInstance>("fmi3FreeInstance");
    const auto enter_initialization = library.symbol<Fmi3EnterInitializationMode>(
        "fmi3EnterInitializationMode");
    const auto exit_initialization = library.symbol<Fmi3ExitInitializationMode>(
        "fmi3ExitInitializationMode");
    const auto terminate = library.symbol<Fmi3Terminate>("fmi3Terminate");
    const auto set_float64 = library.symbol<Fmi3SetFloat64>("fmi3SetFloat64");
    const auto get_float64 = library.symbol<Fmi3GetFloat64>("fmi3GetFloat64");
    const auto get_interval = library.symbol<Fmi3GetIntervalDecimal>(
        "fmi3GetIntervalDecimal");
    const auto get_shift = library.symbol<Fmi3GetShiftDecimal>(
        "fmi3GetShiftDecimal");
    const auto activate_partition = library.symbol<Fmi3ActivateModelPartition>(
        "fmi3ActivateModelPartition");
    const char* runtime_version = get_version();
    if (runtime_version == nullptr || std::string_view(runtime_version) != "3.0") {
        throw std::runtime_error("FMU runtime version does not match FMI 3.0 metadata");
    }

    ScheduledExecutionCallbackContext callback_context;
    if (scheduled_execution_callback_context != nullptr) {
        throw std::runtime_error(
            "nested Scheduled Execution smoke is unsupported on one thread");
    }
    scheduled_execution_callback_context = &callback_context;
    struct CallbackContextCleanup {
        ~CallbackContextCleanup() { scheduled_execution_callback_context = nullptr; }
    } callback_context_cleanup;
    const auto resource_path = file_uri(directory / "resources");
    Fmi3Instance instance = instantiate(
        "smave-scheduled-smoke", model.instantiation_token.c_str(),
        resource_path.c_str(), false, false, &callback_context, nullptr,
        clock_update_callback, lock_preemption_callback,
        unlock_preemption_callback);
    if (instance == nullptr) {
        throw std::runtime_error("fmi3InstantiateScheduledExecution returned null");
    }
    struct InstanceCleanup {
        Fmi3Instance instance;
        Fmi3FreeInstance free_instance;
        ~InstanceCleanup() { if (instance != nullptr) free_instance(instance); }
    } cleanup{instance, free_instance};

    FmiSmokeResult result;
    result.model_name = model.model_name;
    result.source_hash = model.source_hash;
    result.interface_kind = interface.kind;
    result.model_identifier = interface.model_identifier;
    result.end_time = end_time;
    result.step_size = interval;
    require_status(
        enter_initialization(instance, true, 1.0e-6, 0.0, true, end_time),
        result.warnings, "fmi3EnterInitializationMode");
    if (!input_references.empty()) {
        require_status(
            set_float64(instance, input_references.data(), input_references.size(),
                        input_values.data(), input_values.size()),
            result.warnings, "fmi3SetFloat64");
    }
    require_status(
        exit_initialization(instance), result.warnings, "fmi3ExitInitializationMode");

    std::vector<Fmi3ValueReference> clock_references;
    clock_references.reserve(clocks.size());
    for (const auto* clock : clocks) {
        clock_references.push_back(
            static_cast<Fmi3ValueReference>(clock->value_reference));
    }
    std::vector<Fmi3Float64> declared_intervals(clocks.size());
    std::vector<Fmi3IntervalQualifier> qualifiers(clocks.size());
    require_status(
        get_interval(instance, clock_references.data(), clock_references.size(),
                     declared_intervals.data(), qualifiers.data()),
        result.warnings, "fmi3GetIntervalDecimal");
    std::vector<Fmi3Float64> shifts(clocks.size());
    require_status(
        get_shift(instance, clock_references.data(), clock_references.size(),
                  shifts.data()),
        result.warnings, "fmi3GetShiftDecimal");
    double minimum_interval = std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < clocks.size(); ++index) {
        const double declared_interval = declared_intervals[index];
        const double shift = shifts[index];
        const auto qualifier = qualifiers[index];
        if (!std::isfinite(declared_interval) || declared_interval <= 0.0 ||
            !std::isfinite(shift) || shift < 0.0 || shift >= declared_interval ||
            qualifier == Fmi3IntervalQualifier::not_yet_known) {
            throw std::runtime_error(
                "Scheduled Execution Clock interval contract is invalid");
        }
        minimum_interval = std::min(minimum_interval, declared_interval);
        result.clock_intervals.emplace(clocks[index]->name, declared_interval);
        result.clock_shifts.emplace(clocks[index]->name, shift);
        result.clock_priorities.emplace(
            clocks[index]->name, *clocks[index]->clock_priority);
        result.clock_interval_qualifiers.emplace(
            clocks[index]->name,
            qualifier == Fmi3IntervalQualifier::changed ? "changed" : "unchanged");
    }
    if (std::abs(minimum_interval - interval) >
        1.0e-10 * std::max(1.0, interval)) {
        throw std::invalid_argument(
            "Scheduled Execution CLI interval must match the fastest FMU Clock");
    }

    const auto sample_outputs = [&](double time) {
        std::vector<Fmi3Float64> values(outputs.size());
        require_status(
            get_float64(instance, output_references.data(), output_references.size(),
                        values.data(), values.size()),
            result.warnings, "fmi3GetFloat64");
        FmiSmokeSample sample;
        sample.time = time;
        for (std::size_t index = 0; index < outputs.size(); ++index) {
            if (!std::isfinite(values[index])) {
                throw std::runtime_error(outputs[index]->name + ": FMU output is NaN/Inf");
            }
            sample.outputs.emplace(outputs[index]->name, values[index]);
        }
        return sample;
    };
    std::vector<FmiPartitionActivation> activations;
    for (std::size_t clock_index = 0; clock_index < clocks.size(); ++clock_index) {
        const double clock_interval = declared_intervals[clock_index];
        const double shift = shifts[clock_index];
        for (std::size_t tick = 0; tick <= 1000000U; ++tick) {
            const double activation_time = shift + static_cast<double>(tick) * clock_interval;
            if (activation_time > end_time + 1.0e-12 * std::max(1.0, end_time)) break;
            activations.push_back({
                activation_time, clocks[clock_index]->name,
                clocks[clock_index]->value_reference,
                *clocks[clock_index]->clock_priority});
        }
    }
    if (activations.size() > 1000000U) {
        throw std::invalid_argument(
            "Scheduled Execution activation schedule exceeds safety limit");
    }
    std::sort(activations.begin(), activations.end(), [](const auto& left, const auto& right) {
        if (left.time != right.time) return left.time < right.time;
        if (left.clock_priority != right.clock_priority) {
            return left.clock_priority < right.clock_priority;
        }
        if (left.clock_value_reference != right.clock_value_reference) {
            return left.clock_value_reference < right.clock_value_reference;
        }
        return left.clock_name < right.clock_name;
    });
    for (const auto& activation : activations) {
        require_status(
            activate_partition(
                instance,
                static_cast<Fmi3ValueReference>(activation.clock_value_reference),
                activation.time),
            result.warnings, "fmi3ActivateModelPartition");
        ++result.model_partition_activations;
        result.partition_activation_order.push_back(activation);
        result.samples.push_back(sample_outputs(activation.time));
    }
    result.clock_update_callbacks = callback_context.clock_updates;
    result.lock_preemption_callbacks = callback_context.locks;
    result.unlock_preemption_callbacks = callback_context.unlocks;
    if (callback_context.invalid_unlock || callback_context.lock_depth != 0 ||
        callback_context.locks != callback_context.unlocks) {
        throw std::runtime_error(
            "Scheduled Execution preemption callbacks are not balanced");
    }
    require_status(terminate(instance), result.warnings, "fmi3Terminate");
    result.success = true;
    result.message =
        "opt-in FMI 3.0 Scheduled Execution deterministic periodic partition smoke completed";
    return result;
}

FmiSmokeResult smoke_fmi2_model_exchange(
    const std::filesystem::path& path,
    double end_time,
    double step_size,
    const std::map<std::string, double>& inputs,
    bool allow_native_execution,
    const std::map<std::string, std::string>& string_inputs) {
    if (!allow_native_execution) {
        throw std::invalid_argument(
            "native FMU execution requires explicit --allow-native-execution");
    }
    if (std::filesystem::is_directory(path)) {
        throw std::invalid_argument(
            "native FMU execution requires a .fmu archive so source_hash binds the binary");
    }
    if (!(end_time > 0.0) || !(step_size > 0.0) ||
        !std::isfinite(end_time) || !std::isfinite(step_size)) {
        throw std::invalid_argument(
            "FMI smoke end time and step size must be finite and positive");
    }
    const double step_count = end_time / step_size;
    const auto rounded_steps = static_cast<std::size_t>(std::llround(step_count));
    if (rounded_steps == 0 || rounded_steps > 1000000U ||
        std::abs(step_count - static_cast<double>(rounded_steps)) >
            1.0e-10 * std::max(1.0, step_count)) {
        throw std::invalid_argument(
            "FMI smoke end time must be an integer multiple of step size");
    }

    const auto model = import_fmu(path);
    if (model.fmi_version != "2.0") {
        throw std::invalid_argument("FMI 2 Model Exchange requires FMI 2.0 metadata");
    }
    const auto& interface = model_exchange_interface(model);
    if (!model.host_binary_candidate_available) {
        throw std::runtime_error("no complete host ModelExchange binary candidate");
    }

    std::vector<Fmi2ValueReference> real_input_references;
    std::vector<Fmi2Real> real_input_values;
    std::vector<Fmi2ValueReference> integer_input_references;
    std::vector<Fmi2Integer> integer_input_values;
    std::vector<Fmi2ValueReference> boolean_input_references;
    std::vector<Fmi2Boolean> boolean_input_values;
    std::vector<Fmi2ValueReference> string_input_references;
    std::vector<Fmi2String> string_input_values;
    for (const auto& [name, value] : inputs) {
        const auto& variable = scalar_input_variable(
            model, name, {"Real", "Integer", "Boolean", "Enumeration"}, "2");
        const auto reference = static_cast<Fmi2ValueReference>(variable.value_reference);
        if (variable.type == "Real") {
            if (!std::isfinite(value)) {
                throw std::invalid_argument(name + ": FMI input is NaN/Inf");
            }
            real_input_references.push_back(reference);
            real_input_values.push_back(value);
        } else if (variable.type == "Integer" || variable.type == "Enumeration") {
            integer_input_references.push_back(reference);
            integer_input_values.push_back(exact_integer_input<Fmi2Integer>(value, name));
        } else {
            boolean_input_references.push_back(reference);
            boolean_input_values.push_back(exact_boolean_input(value, name));
        }
    }
    for (const auto& [name, value] : string_inputs) {
        const auto& variable = scalar_input_variable(model, name, {"String"}, "2");
        string_input_references.push_back(
            static_cast<Fmi2ValueReference>(variable.value_reference));
        string_input_values.push_back(value.c_str());
    }
    std::vector<const FmiVariableIR*> real_outputs;
    std::vector<Fmi2ValueReference> real_output_references;
    std::vector<const FmiVariableIR*> integer_outputs;
    std::vector<Fmi2ValueReference> integer_output_references;
    std::vector<const FmiVariableIR*> boolean_outputs;
    std::vector<Fmi2ValueReference> boolean_output_references;
    std::vector<const FmiVariableIR*> string_outputs;
    std::vector<Fmi2ValueReference> string_output_references;
    std::vector<std::size_t> state_variable_indices;
    for (std::size_t index = 0; index < model.variables.size(); ++index) {
        const auto& variable = model.variables[index];
        if (variable.causality == "output" && variable.dimensions == 0 &&
            variable.value_reference <= std::numeric_limits<Fmi2ValueReference>::max()) {
            const auto reference = static_cast<Fmi2ValueReference>(variable.value_reference);
            if (variable.type == "Real") {
                real_outputs.push_back(&variable);
                real_output_references.push_back(reference);
            } else if (variable.type == "Integer" || variable.type == "Enumeration") {
                integer_outputs.push_back(&variable);
                integer_output_references.push_back(reference);
            } else if (variable.type == "Boolean") {
                boolean_outputs.push_back(&variable);
                boolean_output_references.push_back(reference);
            } else if (variable.type == "String") {
                string_outputs.push_back(&variable);
                string_output_references.push_back(reference);
            }
        }
    }
    if (real_outputs.empty() && integer_outputs.empty() && boolean_outputs.empty() &&
        string_outputs.empty()) {
        throw std::invalid_argument(
            "FMI smoke requires at least one supported scalar FMI 2 output");
    }
    if (model.derivative_variable_order.empty() ||
        model.derivative_variable_order.size() > 1000000U) {
        throw std::invalid_argument(
            "FMI 2 Model Exchange requires ModelStructure derivative ordering");
    }
    std::unordered_set<std::size_t> unique_states;
    for (const auto derivative_index : model.derivative_variable_order) {
        const auto& derivative = model.variables[derivative_index - 1];
        const auto state_index = derivative.derivative_of - 1;
        if (derivative.type != "Real" || derivative.dimensions != 0 ||
            model.variables[state_index].type != "Real" ||
            model.variables[state_index].dimensions != 0 ||
            !unique_states.insert(state_index).second) {
            throw std::invalid_argument("duplicate FMI 2 derivative state reference");
        }
        state_variable_indices.push_back(state_index);
    }
    if (model.number_of_event_indicators > 1024U) {
        throw std::invalid_argument("FMI smoke supports at most 1024 event indicators");
    }

    std::unique_ptr<TemporaryDirectory> temporary;
    const auto directory = prepare_fmu_directory(path, model, interface, temporary);
    DynamicLibrary library(directory / interface_binary_relative(model, interface));
    const auto get_version = library.symbol<Fmi2GetVersion>("fmi2GetVersion");
    const auto instantiate = library.symbol<Fmi2Instantiate>("fmi2Instantiate");
    const auto free_instance = library.symbol<Fmi2FreeInstance>("fmi2FreeInstance");
    const auto setup_experiment =
        library.symbol<Fmi2SetupExperiment>("fmi2SetupExperiment");
    const auto enter_initialization = library.symbol<Fmi2EnterInitializationMode>(
        "fmi2EnterInitializationMode");
    const auto exit_initialization = library.symbol<Fmi2ExitInitializationMode>(
        "fmi2ExitInitializationMode");
    const auto enter_event_mode =
        library.symbol<Fmi2EnterEventMode>("fmi2EnterEventMode");
    const auto new_discrete_states =
        library.symbol<Fmi2NewDiscreteStates>("fmi2NewDiscreteStates");
    const auto enter_continuous = library.symbol<Fmi2EnterContinuousTimeMode>(
        "fmi2EnterContinuousTimeMode");
    const auto completed_step = library.symbol<Fmi2CompletedIntegratorStep>(
        "fmi2CompletedIntegratorStep");
    const auto set_time = library.symbol<Fmi2SetTime>("fmi2SetTime");
    const auto set_states = library.symbol<Fmi2SetContinuousStates>(
        "fmi2SetContinuousStates");
    const auto get_derivatives =
        library.symbol<Fmi2GetDerivatives>("fmi2GetDerivatives");
    const auto get_states = library.symbol<Fmi2GetContinuousStates>(
        "fmi2GetContinuousStates");
    const auto get_nominals = library.symbol<Fmi2GetNominalsOfContinuousStates>(
        "fmi2GetNominalsOfContinuousStates");
    Fmi2GetEventIndicators get_indicators{};
    if (model.number_of_event_indicators != 0) {
        get_indicators = library.symbol<Fmi2GetEventIndicators>(
            "fmi2GetEventIndicators");
    }
    const auto terminate = library.symbol<Fmi2Terminate>("fmi2Terminate");
    const auto set_real = library.symbol<Fmi2SetReal>("fmi2SetReal");
    const auto get_real = library.symbol<Fmi2GetReal>("fmi2GetReal");
    Fmi2SetInteger set_integer{};
    Fmi2GetInteger get_integer{};
    Fmi2SetBoolean set_boolean{};
    Fmi2GetBoolean get_boolean{};
    Fmi2SetString set_string{};
    Fmi2GetString get_string{};
    if (!integer_input_references.empty()) {
        set_integer = library.symbol<Fmi2SetInteger>("fmi2SetInteger");
    }
    if (!integer_output_references.empty()) {
        get_integer = library.symbol<Fmi2GetInteger>("fmi2GetInteger");
    }
    if (!boolean_input_references.empty()) {
        set_boolean = library.symbol<Fmi2SetBoolean>("fmi2SetBoolean");
    }
    if (!boolean_output_references.empty()) {
        get_boolean = library.symbol<Fmi2GetBoolean>("fmi2GetBoolean");
    }
    if (!string_input_references.empty()) {
        set_string = library.symbol<Fmi2SetString>("fmi2SetString");
    }
    if (!string_output_references.empty()) {
        get_string = library.symbol<Fmi2GetString>("fmi2GetString");
    }
    const bool state_enabled = capability_enabled(interface, "canGetAndSetFMUstate");
    Fmi2GetFmuState get_fmu_state{};
    Fmi2SetFmuState set_fmu_state{};
    Fmi2FreeFmuState free_fmu_state{};
    Fmi2SerializedFmuStateSize serialized_fmu_state_size{};
    Fmi2SerializeFmuState serialize_fmu_state{};
    Fmi2DeserializeFmuState deserialize_fmu_state{};
    if (state_enabled) {
        get_fmu_state = library.symbol<Fmi2GetFmuState>("fmi2GetFMUstate");
        set_fmu_state = library.symbol<Fmi2SetFmuState>("fmi2SetFMUstate");
        free_fmu_state = library.symbol<Fmi2FreeFmuState>("fmi2FreeFMUstate");
    }
    const bool serialization_enabled =
        capability_enabled(interface, "canSerializeFMUstate");
    if (serialization_enabled) {
        if (!state_enabled) {
            throw std::invalid_argument(
                "canSerializeFMUstate requires canGetAndSetFMUstate");
        }
        serialized_fmu_state_size = library.symbol<Fmi2SerializedFmuStateSize>(
            "fmi2SerializedFMUstateSize");
        serialize_fmu_state = library.symbol<Fmi2SerializeFmuState>(
            "fmi2SerializeFMUstate");
        deserialize_fmu_state = library.symbol<Fmi2DeserializeFmuState>(
            "fmi2DeSerializeFMUstate");
    }
    const char* runtime_version = get_version();
    if (runtime_version == nullptr || std::string_view(runtime_version) != "2.0") {
        throw std::runtime_error("FMU runtime version does not match FMI 2.0 metadata");
    }

    Fmi2CallbackFunctions callbacks;
    callbacks.allocate_memory = std::calloc;
    callbacks.free_memory = std::free;
    const auto resource_path = file_uri(directory / "resources");
    Fmi2Component instance = instantiate(
        "smave-me-smoke", Fmi2Type::model_exchange,
        model.instantiation_token.c_str(), resource_path.c_str(), &callbacks,
        false, false);
    if (instance == nullptr) throw std::runtime_error("fmi2Instantiate returned null");
    struct InstanceCleanup {
        Fmi2Component instance;
        Fmi2FreeInstance free_instance;
        ~InstanceCleanup() { if (instance != nullptr) free_instance(instance); }
    } cleanup{instance, free_instance};

    FmiSmokeResult result;
    result.model_name = model.model_name;
    result.source_hash = model.source_hash;
    result.interface_kind = interface.kind;
    result.model_identifier = interface.model_identifier;
    result.end_time = end_time;
    result.step_size = step_size;
    result.state_roundtrip_attempted = state_enabled;
    result.state_serialization_attempted = serialization_enabled;
    require_status(
        setup_experiment(instance, true, 1.0e-6, 0.0, true, end_time),
        result.warnings, "fmi2SetupExperiment");
    require_status(
        enter_initialization(instance), result.warnings,
        "fmi2EnterInitializationMode");
    if (!real_input_references.empty()) {
        require_status(
            set_real(instance, real_input_references.data(), real_input_references.size(),
                     real_input_values.data()),
            result.warnings, "fmi2SetReal");
    }
    if (!integer_input_references.empty()) {
        require_status(
            set_integer(instance, integer_input_references.data(),
                        integer_input_references.size(), integer_input_values.data()),
            result.warnings, "fmi2SetInteger");
    }
    if (!boolean_input_references.empty()) {
        require_status(
            set_boolean(instance, boolean_input_references.data(),
                        boolean_input_references.size(), boolean_input_values.data()),
            result.warnings, "fmi2SetBoolean");
    }
    if (!string_input_references.empty()) {
        require_status(
            set_string(instance, string_input_references.data(),
                       string_input_references.size(), string_input_values.data()),
            result.warnings, "fmi2SetString");
    }
    require_status(
        exit_initialization(instance), result.warnings,
        "fmi2ExitInitializationMode");

    std::optional<double> next_time_event;
    const auto record_nominals = [&]() {
        std::vector<double> nominals(state_variable_indices.size());
        require_status(
            get_nominals(instance, nominals.data(), nominals.size()), result.warnings,
            "fmi2GetNominalsOfContinuousStates");
        for (const double nominal : nominals) {
            if (!std::isfinite(nominal) || !(nominal > 0.0)) {
                throw std::runtime_error(
                    "FMI 2 continuous-state nominal must be finite and positive");
            }
        }
        const auto [minimum, maximum] = std::minmax_element(
            nominals.begin(), nominals.end());
        if (result.continuous_state_nominal_updates == 0) {
            result.minimum_continuous_state_nominal = *minimum;
            result.maximum_continuous_state_nominal = *maximum;
        } else {
            result.minimum_continuous_state_nominal = std::min(
                result.minimum_continuous_state_nominal, *minimum);
            result.maximum_continuous_state_nominal = std::max(
                result.maximum_continuous_state_nominal, *maximum);
        }
        ++result.continuous_state_nominal_updates;
    };
    const auto update_discrete = [&](double event_time) {
        Fmi2EventInfo event_info;
        event_info.new_discrete_states_needed = true;
        for (std::size_t iteration = 0;
             event_info.new_discrete_states_needed; ++iteration) {
            if (iteration >= 1024U) {
                throw std::runtime_error("FMI 2 discrete update did not converge");
            }
            require_status(
                new_discrete_states(instance, &event_info), result.warnings,
                "fmi2NewDiscreteStates");
            ++result.discrete_update_iterations;
            if (event_info.terminate_simulation) {
                throw std::runtime_error("FMU requested termination during event update");
            }
            if (event_info.nominals_of_continuous_states_changed) {
                record_nominals();
            }
        }
        next_time_event.reset();
        if (event_info.next_event_time_defined) {
            if (!std::isfinite(event_info.next_event_time) ||
                event_info.next_event_time <= event_time + 1.0e-12 ||
                event_info.next_event_time > end_time + 1.0e-12) {
                throw std::runtime_error("FMI 2 nextEventTime is invalid");
            }
            next_time_event = event_info.next_event_time;
        }
        return event_info.values_of_continuous_states_changed != 0;
    };
    (void)update_discrete(0.0);
    require_status(
        enter_continuous(instance), result.warnings,
        "fmi2EnterContinuousTimeMode");

    std::vector<double> states(state_variable_indices.size());
    require_status(
        get_states(instance, states.data(), states.size()), result.warnings,
        "fmi2GetContinuousStates");
    const auto require_finite_vector = [](const auto& values, std::string_view name) {
        if (!std::all_of(values.begin(), values.end(), [](double value) {
                return std::isfinite(value);
            })) {
            throw std::runtime_error(std::string(name) + " contains NaN/Inf");
        }
    };
    require_finite_vector(states, "FMI continuous state");

    const auto set_point = [&](double time, const std::vector<double>& point) {
        require_status(set_time(instance, time), result.warnings, "fmi2SetTime");
        require_status(
            set_states(instance, point.data(), point.size()), result.warnings,
            "fmi2SetContinuousStates");
    };
    const auto derivatives_at = [&](double time, const std::vector<double>& point) {
        set_point(time, point);
        std::vector<double> derivatives(point.size());
        require_status(
            get_derivatives(instance, derivatives.data(), derivatives.size()),
            result.warnings, "fmi2GetDerivatives");
        require_finite_vector(derivatives, "FMI derivative");
        return derivatives;
    };
    const auto rk4 = [&](double start_time, const std::vector<double>& start,
                         double duration) {
        const auto first = derivatives_at(start_time, start);
        std::vector<double> point(start.size());
        for (std::size_t index = 0; index < point.size(); ++index) {
            point[index] = start[index] + 0.5 * duration * first[index];
        }
        const auto second = derivatives_at(start_time + 0.5 * duration, point);
        for (std::size_t index = 0; index < point.size(); ++index) {
            point[index] = start[index] + 0.5 * duration * second[index];
        }
        const auto third = derivatives_at(start_time + 0.5 * duration, point);
        for (std::size_t index = 0; index < point.size(); ++index) {
            point[index] = start[index] + duration * third[index];
        }
        const auto fourth = derivatives_at(start_time + duration, point);
        std::vector<double> end(start.size());
        for (std::size_t index = 0; index < end.size(); ++index) {
            end[index] = start[index] + duration *
                (first[index] + 2.0 * second[index] + 2.0 * third[index] +
                 fourth[index]) / 6.0;
        }
        require_finite_vector(end, "FMI integrated state");
        set_point(start_time + duration, end);
        return end;
    };
    const auto indicators_at = [&](double time, const std::vector<double>& point) {
        std::vector<double> indicators(model.number_of_event_indicators);
        if (!indicators.empty()) {
            set_point(time, point);
            require_status(
                get_indicators(instance, indicators.data(), indicators.size()),
                result.warnings, "fmi2GetEventIndicators");
            require_finite_vector(indicators, "FMI event indicator");
        }
        return indicators;
    };
    const auto sample_outputs = [&](double time) {
        FmiSmokeSample sample;
        sample.time = time;
        if (!real_outputs.empty()) {
            std::vector<double> values(real_outputs.size());
            require_status(
                get_real(instance, real_output_references.data(),
                         real_output_references.size(), values.data()),
                result.warnings, "fmi2GetReal");
            require_finite_vector(values, "FMI output");
            for (std::size_t index = 0; index < real_outputs.size(); ++index) {
                sample.outputs.emplace(real_outputs[index]->name, values[index]);
            }
        }
        if (!integer_outputs.empty()) {
            std::vector<Fmi2Integer> values(integer_outputs.size());
            require_status(
                get_integer(instance, integer_output_references.data(),
                            integer_output_references.size(), values.data()),
                result.warnings, "fmi2GetInteger");
            for (std::size_t index = 0; index < integer_outputs.size(); ++index) {
                sample.outputs.emplace(
                    integer_outputs[index]->name, static_cast<double>(values[index]));
            }
        }
        if (!boolean_outputs.empty()) {
            std::vector<Fmi2Boolean> values(boolean_outputs.size());
            require_status(
                get_boolean(instance, boolean_output_references.data(),
                            boolean_output_references.size(), values.data()),
                result.warnings, "fmi2GetBoolean");
            for (std::size_t index = 0; index < boolean_outputs.size(); ++index) {
                sample.outputs.emplace(boolean_outputs[index]->name, values[index] ? 1.0 : 0.0);
            }
        }
        if (!string_outputs.empty()) {
            std::vector<Fmi2String> values(string_outputs.size());
            require_status(
                get_string(instance, string_output_references.data(),
                           string_output_references.size(), values.data()),
                result.warnings, "fmi2GetString");
            for (std::size_t index = 0; index < string_outputs.size(); ++index) {
                if (values[index] == nullptr) {
                    throw std::runtime_error(
                        string_outputs[index]->name + ": FMU String output is null");
                }
                sample.string_outputs.emplace(string_outputs[index]->name, values[index]);
            }
        }
        return sample;
    };
    set_point(0.0, states);
    auto indicators = indicators_at(0.0, states);
    result.samples.push_back(sample_outputs(0.0));

    const auto advance_interval = [&](double interval_start,
                                      double interval_end,
                                      auto&& advance_interval_ref) -> void {
        double current_time = interval_start;
        std::size_t event_count{};
        while (current_time < interval_end - 1.0e-12) {
            if (++event_count > 1024U) {
                throw std::runtime_error("too many FMI 2 ME events in one macro step");
            }
            double target = interval_end;
            bool scheduled_time_event{};
            if (next_time_event && *next_time_event < target - 1.0e-12) {
                target = *next_time_event;
                scheduled_time_event = true;
                ++result.time_event_splits;
            } else if (next_time_event &&
                       std::abs(*next_time_event - target) <= 1.0e-12) {
                scheduled_time_event = true;
            }
            const auto start_states = states;
            const auto start_indicators = indicators;
            const auto candidate = rk4(current_time, start_states, target - current_time);
            const auto candidate_indicators = indicators_at(target, candidate);

            std::optional<double> root_time;
            std::vector<double> root_states;
            std::vector<double> root_indicators;
            bool grazing_root{};
            for (std::size_t indicator = 0;
                 indicator < candidate_indicators.size(); ++indicator) {
                const double left_value = start_indicators[indicator];
                const double right_value = candidate_indicators[indicator];
                if ((left_value < 0.0 && right_value < 0.0) ||
                    (left_value > 0.0 && right_value > 0.0) ||
                    (left_value == 0.0 && right_value == 0.0)) {
                    if (left_value * right_value <= 0.0 ||
                        std::abs(left_value) <= 1.0e-8 ||
                        std::abs(right_value) <= 1.0e-8) {
                        continue;
                    }
                    constexpr double inverse_golden = 0.6180339887498948482;
                    double search_left = current_time;
                    double search_right = target;
                    double first = search_right -
                        inverse_golden * (search_right - search_left);
                    double second = search_left +
                        inverse_golden * (search_right - search_left);
                    const auto magnitude_at = [&](double time) {
                        const auto point = rk4(
                            current_time, start_states, time - current_time);
                        return std::abs(indicators_at(time, point)[indicator]);
                    };
                    double first_value = magnitude_at(first);
                    double second_value = magnitude_at(second);
                    while (search_right - search_left > 1.0e-12) {
                        if (first_value > second_value) {
                            search_left = first;
                            first = second;
                            first_value = second_value;
                            second = search_left +
                                inverse_golden * (search_right - search_left);
                            second_value = magnitude_at(second);
                        } else {
                            search_right = second;
                            second = first;
                            second_value = first_value;
                            first = search_right -
                                inverse_golden * (search_right - search_left);
                            first_value = magnitude_at(first);
                        }
                    }
                    const double grazing_time = 0.5 * (search_left + search_right);
                    const double probe = std::max(
                        4.0e-12, (target - current_time) / 64.0);
                    if (grazing_time <= current_time + probe ||
                        grazing_time + probe >= target) {
                        continue;
                    }
                    auto grazing_states = rk4(
                        current_time, start_states, grazing_time - current_time);
                    auto grazing_indicators = indicators_at(
                        grazing_time, grazing_states);
                    const double minimum = std::abs(
                        grazing_indicators[indicator]);
                    const double prominence = std::min(
                        magnitude_at(grazing_time - probe) - minimum,
                        magnitude_at(grazing_time + probe) - minimum);
                    if (minimum <= 1.0e-8 && prominence > 1.0e-8 &&
                        (!root_time || grazing_time < *root_time - 1.0e-12)) {
                        root_time = grazing_time;
                        root_states = std::move(grazing_states);
                        root_indicators = std::move(grazing_indicators);
                        grazing_root = true;
                    }
                    continue;
                }
                double left = current_time;
                double right = target;
                double left_indicator = left_value;
                std::vector<double> best_states = candidate;
                std::vector<double> best_indicators = candidate_indicators;
                for (std::size_t iteration = 0;
                     iteration < 80U && right - left > 1.0e-12; ++iteration) {
                    const double middle = 0.5 * (left + right);
                    const auto middle_states =
                        rk4(current_time, start_states, middle - current_time);
                    const auto middle_indicators =
                        indicators_at(middle, middle_states);
                    const double middle_indicator = middle_indicators[indicator];
                    const bool left_half =
                        (left_indicator <= 0.0 && middle_indicator >= 0.0) ||
                        (left_indicator >= 0.0 && middle_indicator <= 0.0);
                    if (left_half) {
                        right = middle;
                        best_states = middle_states;
                        best_indicators = middle_indicators;
                    } else {
                        left = middle;
                        left_indicator = middle_indicator;
                    }
                }
                if (!root_time || right < *root_time - 1.0e-12) {
                    root_time = right;
                    root_states = std::move(best_states);
                    root_indicators = std::move(best_indicators);
                    grazing_root = false;
                }
            }

            const bool root_before_target = root_time &&
                *root_time < target - 1.0e-12;
            if (root_before_target) {
                target = *root_time;
                states = std::move(root_states);
                indicators = std::move(root_indicators);
                scheduled_time_event = false;
                ++result.model_exchange_roots;
                if (grazing_root) ++result.model_exchange_grazing_roots;
            } else {
                states = candidate;
                indicators = candidate_indicators;
                if (root_time) {
                    ++result.model_exchange_roots;
                    if (grazing_root) ++result.model_exchange_grazing_roots;
                }
            }
            set_point(target, states);
            Fmi2Boolean enter_event{};
            Fmi2Boolean terminate_requested{};
            require_status(
                completed_step(instance, true, &enter_event, &terminate_requested),
                result.warnings, "fmi2CompletedIntegratorStep");
            if (terminate_requested) {
                throw std::runtime_error("FMU requested termination after integrator step");
            }
            if (enter_event) {
                throw std::runtime_error("FMI 2 step events are unsupported");
            }
            const bool process_event = root_time.has_value() || scheduled_time_event;
            if (process_event) {
                require_status(
                    enter_event_mode(instance), result.warnings,
                    "fmi2EnterEventMode");
                ++result.event_mode_entries;
                const bool state_changed = update_discrete(target);
                if (scheduled_time_event) ++result.time_events;
                if (state_changed) {
                    require_status(
                        get_states(instance, states.data(), states.size()),
                        result.warnings, "fmi2GetContinuousStates after event");
                    require_finite_vector(states, "FMI event state");
                }
                require_status(
                    enter_continuous(instance), result.warnings,
                    "fmi2EnterContinuousTimeMode");
                indicators = indicators_at(target, states);
            }
            current_time = target;
            if (!process_event && current_time < interval_end - 1.0e-12) {
                advance_interval_ref(current_time, interval_end, advance_interval_ref);
                return;
            }
        }
    };

    Fmi2FmuState saved_state{};
    std::vector<double> saved_states;
    std::vector<double> saved_indicators;
    std::optional<double> saved_next_time_event;
    std::map<std::string, double> first_outputs;
    for (std::size_t step = 0; step < rounded_steps; ++step) {
        const double start = static_cast<double>(step) * step_size;
        if (step == 0 && state_enabled) {
            require_status(
                get_fmu_state(instance, &saved_state), result.warnings,
                "fmi2GetFMUstate");
            if (saved_state == nullptr) {
                throw std::runtime_error("fmi2GetFMUstate returned null");
            }
            if (serialization_enabled) {
                std::size_t serialized_size{};
                require_status(
                    serialized_fmu_state_size(instance, saved_state, &serialized_size),
                    result.warnings, "fmi2SerializedFMUstateSize");
                if (serialized_size == 0 || serialized_size > 16U * 1024U * 1024U) {
                    throw std::runtime_error(
                        "FMI serialized state size must be within 1 byte and 16 MiB");
                }
                std::vector<std::uint8_t> serialized(serialized_size);
                require_status(
                    serialize_fmu_state(instance, saved_state, serialized.data(),
                                        serialized.size()),
                    result.warnings, "fmi2SerializeFMUstate");
                require_status(
                    free_fmu_state(instance, &saved_state), result.warnings,
                    "fmi2FreeFMUstate before deserialization");
                require_status(
                    deserialize_fmu_state(instance, serialized.data(), serialized.size(),
                                          &saved_state),
                    result.warnings, "fmi2DeSerializeFMUstate");
                if (saved_state == nullptr) {
                    throw std::runtime_error("fmi2DeSerializeFMUstate returned null");
                }
                result.serialized_state_bytes = serialized.size();
            }
            saved_states = states;
            saved_indicators = indicators;
            saved_next_time_event = next_time_event;
        }
        advance_interval(start, start + step_size, advance_interval);
        set_point(start + step_size, states);
        result.samples.push_back(sample_outputs(start + step_size));
        if (step == 0 && state_enabled) {
            first_outputs = result.samples.back().outputs;
            require_status(
                set_fmu_state(instance, saved_state), result.warnings,
                "fmi2SetFMUstate");
            states = saved_states;
            indicators = saved_indicators;
            next_time_event = saved_next_time_event;
            advance_interval(0.0, step_size, advance_interval);
            set_point(step_size, states);
            const auto replay = sample_outputs(step_size);
            for (const auto& [name, expected] : first_outputs) {
                result.maximum_state_replay_error = std::max(
                    result.maximum_state_replay_error,
                    std::abs(replay.outputs.at(name) - expected));
            }
            if (replay.string_outputs != result.samples[1].string_outputs) {
                throw std::runtime_error("FMI state replay String output mismatch");
            }
            result.state_roundtrip_passed =
                result.maximum_state_replay_error <= 1.0e-12;
            result.state_serialization_passed =
                serialization_enabled && result.state_roundtrip_passed;
            require_status(
                free_fmu_state(instance, &saved_state), result.warnings,
                "fmi2FreeFMUstate");
            if (saved_state != nullptr) {
                throw std::runtime_error("fmi2FreeFMUstate did not clear state");
            }
            if (!result.state_roundtrip_passed) {
                throw std::runtime_error("FMI state replay output mismatch");
            }
        }
    }
    require_status(terminate(instance), result.warnings, "fmi2Terminate");
    result.success = true;
    result.message =
        "opt-in FMI 2.0 ModelExchange RK4/event smoke and state replay completed";
    return result;
}

FmiSmokeResult smoke_fmi3_model_exchange(
    const std::filesystem::path& path,
    double end_time,
    double step_size,
    const std::map<std::string, double>& inputs,
    bool allow_native_execution,
    const std::map<std::string, std::string>& string_inputs,
    const std::map<std::string, std::vector<std::uint8_t>>& binary_inputs,
    const std::map<std::string, std::vector<double>>& array_inputs,
    const std::map<std::string, std::vector<std::string>>& string_array_inputs,
    const std::map<std::string, std::vector<std::vector<std::uint8_t>>>&
        binary_array_inputs) {
    if (!allow_native_execution) {
        throw std::invalid_argument(
            "native FMU execution requires explicit --allow-native-execution");
    }
    if (std::filesystem::is_directory(path)) {
        throw std::invalid_argument(
            "native FMU execution requires a .fmu archive so source_hash binds the binary");
    }
    if (!(end_time > 0.0) || !(step_size > 0.0) ||
        !std::isfinite(end_time) || !std::isfinite(step_size)) {
        throw std::invalid_argument("FMI smoke end time and step size must be finite and positive");
    }
    const double step_count = end_time / step_size;
    const auto rounded_steps = static_cast<std::size_t>(std::llround(step_count));
    if (rounded_steps == 0 || rounded_steps > 1000000U ||
        std::abs(step_count - static_cast<double>(rounded_steps)) >
            1.0e-10 * std::max(1.0, step_count)) {
        throw std::invalid_argument("FMI smoke end time must be an integer multiple of step size");
    }

    const auto model = import_fmu(path);
    if (model.fmi_version != "3.0") {
        throw std::invalid_argument("native smoke currently supports only FMI 3.0");
    }
    const auto& interface = model_exchange_interface(model);
    if (!model.host_binary_candidate_available) {
        throw std::runtime_error("no complete host ModelExchange binary candidate");
    }
    std::vector<Fmi3ValueReference> float_input_references;
    std::vector<Fmi3Float64> float_input_values;
    std::vector<Fmi3ValueReference> array_input_references;
    std::vector<Fmi3Float64> array_input_values;
    Fmi3NumericIo<Fmi3Float32> float32{"Float32"};
    Fmi3NumericIo<Fmi3Int8> int8{"Int8"};
    Fmi3NumericIo<Fmi3UInt8> uint8{"UInt8"};
    Fmi3NumericIo<Fmi3Int16> int16{"Int16"};
    Fmi3NumericIo<Fmi3UInt16> uint16{"UInt16"};
    Fmi3NumericIo<Fmi3Int32> int32{"Int32"};
    Fmi3NumericIo<Fmi3UInt32> uint32{"UInt32"};
    Fmi3NumericIo<Fmi3Int64> int64{"Int64"};
    Fmi3NumericIo<Fmi3UInt64> uint64{"UInt64"};
    std::vector<Fmi3ValueReference> boolean_input_references;
    std::vector<Fmi3Boolean> boolean_input_values;
    std::vector<Fmi3ValueReference> boolean_array_input_references;
    std::vector<Fmi3Boolean> boolean_array_input_values;
    std::vector<Fmi3ValueReference> clock_input_references;
    std::vector<Fmi3Clock> clock_input_values;
    std::vector<Fmi3ValueReference> string_input_references;
    std::vector<Fmi3String> string_input_values;
    std::vector<Fmi3ValueReference> string_array_input_references;
    std::vector<Fmi3String> string_array_input_values;
    std::vector<Fmi3ValueReference> binary_input_references;
    std::vector<std::size_t> binary_input_sizes;
    std::vector<const std::uint8_t*> binary_input_values;
    std::vector<Fmi3ValueReference> binary_array_input_references;
    std::vector<std::size_t> binary_array_input_sizes;
    std::vector<const std::uint8_t*> binary_array_input_values;
    constexpr std::size_t maximum_array_elements = 1000000U;
    const auto array_extent = [&](const FmiVariableIR& variable) {
        if (variable.dimension_descriptors.size() != variable.dimensions ||
            variable.dimensions == 0) {
            throw std::invalid_argument(
                variable.name + ": array dimension metadata is unavailable");
        }
        std::size_t extent = 1;
        for (const auto& dimension : variable.dimension_descriptors) {
            std::size_t dimension_extent{};
            if (dimension.fixed_extent) {
                dimension_extent = *dimension.fixed_extent;
            } else {
                const auto extent_variable = std::find_if(
                    model.variables.begin(), model.variables.end(), [&](const auto& candidate) {
                        return candidate.value_reference ==
                                *dimension.extent_value_reference &&
                            candidate.causality == "structuralParameter";
                    });
                if (extent_variable == model.variables.end() ||
                    extent_variable->variability != "fixed" ||
                    extent_variable->dimensions != 0 || extent_variable->start.empty() ||
                    (extent_variable->type != "UInt64" &&
                     extent_variable->type != "UInt32" &&
                     extent_variable->type != "UInt16" &&
                     extent_variable->type != "UInt8")) {
                    throw std::invalid_argument(
                        variable.name +
                        ": dynamic dimension requires a fixed unsigned scalar structuralParameter start");
                }
                const auto parsed = unsigned_value(
                    extent_variable->start, extent_variable->name + " structural dimension start");
                if (parsed == 0 || parsed > maximum_array_elements) {
                    throw std::invalid_argument(
                        variable.name + ": dynamic dimension extent is out of range");
                }
                dimension_extent = static_cast<std::size_t>(parsed);
            }
            if (dimension_extent > maximum_array_elements / extent) {
                throw std::invalid_argument(
                    variable.name + ": flattened FMI array is too large");
            }
            extent *= dimension_extent;
        }
        return extent;
    };
    for (const auto& __entry : inputs) {

        const auto& name = __entry.first;

        const auto& value = __entry.second;
        const auto& variable = scalar_input_variable(
            model, name,
            {"Float32", "Float64", "Int8", "UInt8", "Int16", "UInt16",
             "Int32", "UInt32", "Int64", "UInt64", "Boolean", "Enumeration", "Clock"},
            "3");
        const auto reference = static_cast<Fmi3ValueReference>(variable.value_reference);
        if (variable.type == "Float64") {
            if (!std::isfinite(value)) {
                throw std::invalid_argument(name + ": FMI input is NaN/Inf");
            }
            float_input_references.push_back(reference);
            float_input_values.push_back(value);
        } else if (variable.type == "Float32") {
            if (!std::isfinite(value) ||
                std::abs(value) > static_cast<double>(std::numeric_limits<Fmi3Float32>::max())) {
                throw std::invalid_argument(
                    name + ": FMI Float32 input must be finite and in range");
            }
            float32.input_references.push_back(reference);
            float32.input_values.push_back(static_cast<Fmi3Float32>(value));
        } else if (variable.type == "Int8") {
            int8.input_references.push_back(reference);
            int8.input_values.push_back(exact_integer_input<Fmi3Int8>(value, name));
        } else if (variable.type == "UInt8") {
            uint8.input_references.push_back(reference);
            uint8.input_values.push_back(exact_integer_input<Fmi3UInt8>(value, name));
        } else if (variable.type == "Int16") {
            int16.input_references.push_back(reference);
            int16.input_values.push_back(exact_integer_input<Fmi3Int16>(value, name));
        } else if (variable.type == "UInt16") {
            uint16.input_references.push_back(reference);
            uint16.input_values.push_back(exact_integer_input<Fmi3UInt16>(value, name));
        } else if (variable.type == "Int32") {
            int32.input_references.push_back(reference);
            int32.input_values.push_back(exact_integer_input<Fmi3Int32>(value, name));
        } else if (variable.type == "UInt32") {
            uint32.input_references.push_back(reference);
            uint32.input_values.push_back(exact_integer_input<Fmi3UInt32>(value, name));
        } else if (variable.type == "Int64" || variable.type == "Enumeration") {
            int64.input_references.push_back(reference);
            int64.input_values.push_back(exact_integer_input<Fmi3Int64>(value, name));
        } else if (variable.type == "UInt64") {
            uint64.input_references.push_back(reference);
            uint64.input_values.push_back(exact_integer_input<Fmi3UInt64>(value, name));
        } else if (variable.type == "Boolean") {
            boolean_input_references.push_back(reference);
            boolean_input_values.push_back(exact_boolean_input(value, name));
        } else {
            clock_input_references.push_back(reference);
            clock_input_values.push_back(exact_boolean_input(value, name));
        }
    }
    for (const auto& array_input : array_inputs) {
        const auto& name = array_input.first;
        const auto& values = array_input.second;
        const auto iterator = std::find_if(
            model.variables.begin(), model.variables.end(), [&](const auto& variable) {
                return variable.name == name;
            });
        if (iterator == model.variables.end()) {
            throw std::invalid_argument("unknown FMI variable: " + name);
        }
        if ((iterator->causality != "input" && iterator->causality != "parameter") ||
            iterator->value_reference > std::numeric_limits<Fmi3ValueReference>::max()) {
            throw std::invalid_argument(
                name + ": array smoke requires a supported FMI 3 input/parameter array");
        }
        const auto extent = array_extent(*iterator);
        if (values.size() != extent) {
            throw std::invalid_argument(
                name + ": array input element count does not match fixed shape");
        }
        const auto reference = static_cast<Fmi3ValueReference>(iterator->value_reference);
        const auto append_numeric = [&](auto& group, auto convert) {
            group.array_input_references.push_back(reference);
            for (const auto value : values) {
                group.array_input_values.push_back(convert(value));
            }
        };
        if (iterator->type == "Float64") {
            if (!std::all_of(values.begin(), values.end(), [](double value) {
                    return std::isfinite(value);
                })) {
                throw std::invalid_argument(name + ": FMI array input contains NaN/Inf");
            }
            array_input_references.push_back(reference);
            array_input_values.insert(array_input_values.end(), values.begin(), values.end());
        } else if (iterator->type == "Float32") {
            append_numeric(float32, [&](double value) {
                if (!std::isfinite(value) ||
                    std::abs(value) >
                        static_cast<double>(std::numeric_limits<Fmi3Float32>::max())) {
                    throw std::invalid_argument(
                        name + ": FMI Float32 array input must be finite and in range");
                }
                return static_cast<Fmi3Float32>(value);
            });
        } else if (iterator->type == "Int8") {
            append_numeric(int8, [&](double value) {
                return exact_integer_input<Fmi3Int8>(value, name);
            });
        } else if (iterator->type == "UInt8") {
            append_numeric(uint8, [&](double value) {
                return exact_integer_input<Fmi3UInt8>(value, name);
            });
        } else if (iterator->type == "Int16") {
            append_numeric(int16, [&](double value) {
                return exact_integer_input<Fmi3Int16>(value, name);
            });
        } else if (iterator->type == "UInt16") {
            append_numeric(uint16, [&](double value) {
                return exact_integer_input<Fmi3UInt16>(value, name);
            });
        } else if (iterator->type == "Int32") {
            append_numeric(int32, [&](double value) {
                return exact_integer_input<Fmi3Int32>(value, name);
            });
        } else if (iterator->type == "UInt32") {
            append_numeric(uint32, [&](double value) {
                return exact_integer_input<Fmi3UInt32>(value, name);
            });
        } else if (iterator->type == "Int64" || iterator->type == "Enumeration") {
            append_numeric(int64, [&](double value) {
                return exact_integer_input<Fmi3Int64>(value, name);
            });
        } else if (iterator->type == "UInt64") {
            append_numeric(uint64, [&](double value) {
                return exact_integer_input<Fmi3UInt64>(value, name);
            });
        } else if (iterator->type == "Boolean") {
            boolean_array_input_references.push_back(reference);
            for (const auto value : values) {
                boolean_array_input_values.push_back(exact_boolean_input(value, name));
            }
        } else {
            throw std::invalid_argument(
                name + ": array smoke supports only FMI 3 numeric, Enumeration, or Boolean arrays");
        }
    }
    for (const auto& __entry : string_inputs) {

        const auto& name = __entry.first;

        const auto& value = __entry.second;
        const auto& variable = scalar_input_variable(model, name, {"String"}, "3");
        string_input_references.push_back(
            static_cast<Fmi3ValueReference>(variable.value_reference));
        string_input_values.push_back(value.c_str());
    }
    for (const auto& __entry : binary_inputs) {

        const auto& name = __entry.first;

        const auto& value = __entry.second;
        if (value.size() > 16U * 1024U * 1024U) {
            throw std::invalid_argument(name + ": FMI Binary input exceeds 16 MiB");
        }
        const auto& variable = scalar_input_variable(model, name, {"Binary"}, "3");
        binary_input_references.push_back(
            static_cast<Fmi3ValueReference>(variable.value_reference));
        binary_input_sizes.push_back(value.size());
        binary_input_values.push_back(value.empty() ? nullptr : value.data());
    }
    for (const auto& array_input : string_array_inputs) {
        const auto& name = array_input.first;
        const auto& values = array_input.second;
        const auto iterator = std::find_if(
            model.variables.begin(), model.variables.end(), [&](const auto& variable) {
                return variable.name == name;
            });
        if (iterator == model.variables.end()) {
            throw std::invalid_argument("unknown FMI variable: " + name);
        }
        if (iterator->type != "String" ||
            (iterator->causality != "input" && iterator->causality != "parameter") ||
            iterator->value_reference > std::numeric_limits<Fmi3ValueReference>::max()) {
            throw std::invalid_argument(
                name + ": String array input requires a compatible FMI 3 variable");
        }
        if (values.size() != array_extent(*iterator)) {
            throw std::invalid_argument(
                name + ": String array input element count does not match shape");
        }
        string_array_input_references.push_back(
            static_cast<Fmi3ValueReference>(iterator->value_reference));
        for (const auto& value : values) string_array_input_values.push_back(value.c_str());
    }
    for (const auto& array_input : binary_array_inputs) {
        const auto& name = array_input.first;
        const auto& values = array_input.second;
        const auto iterator = std::find_if(
            model.variables.begin(), model.variables.end(), [&](const auto& variable) {
                return variable.name == name;
            });
        if (iterator == model.variables.end()) {
            throw std::invalid_argument("unknown FMI variable: " + name);
        }
        if (iterator->type != "Binary" ||
            (iterator->causality != "input" && iterator->causality != "parameter") ||
            iterator->value_reference > std::numeric_limits<Fmi3ValueReference>::max()) {
            throw std::invalid_argument(
                name + ": Binary array input requires a compatible FMI 3 variable");
        }
        if (values.size() != array_extent(*iterator)) {
            throw std::invalid_argument(
                name + ": Binary array input element count does not match shape");
        }
        binary_array_input_references.push_back(
            static_cast<Fmi3ValueReference>(iterator->value_reference));
        for (const auto& value : values) {
            if (value.size() > 16U * 1024U * 1024U) {
                throw std::invalid_argument(name + ": FMI Binary array element exceeds 16 MiB");
            }
            binary_array_input_sizes.push_back(value.size());
            binary_array_input_values.push_back(value.empty() ? nullptr : value.data());
        }
    }
    std::vector<const FmiVariableIR*> float_outputs;
    std::vector<Fmi3ValueReference> float_output_references;
    std::vector<const FmiVariableIR*> array_outputs;
    std::vector<Fmi3ValueReference> array_output_references;
    std::vector<std::size_t> array_output_extents;
    std::vector<const FmiVariableIR*> boolean_outputs;
    std::vector<Fmi3ValueReference> boolean_output_references;
    std::vector<const FmiVariableIR*> boolean_array_outputs;
    std::vector<Fmi3ValueReference> boolean_array_output_references;
    std::vector<std::size_t> boolean_array_output_extents;
    std::vector<const FmiVariableIR*> clock_outputs;
    std::vector<Fmi3ValueReference> clock_output_references;
    std::vector<const FmiVariableIR*> string_outputs;
    std::vector<Fmi3ValueReference> string_output_references;
    std::vector<const FmiVariableIR*> string_array_outputs;
    std::vector<Fmi3ValueReference> string_array_output_references;
    std::vector<std::size_t> string_array_output_extents;
    std::vector<const FmiVariableIR*> binary_outputs;
    std::vector<Fmi3ValueReference> binary_output_references;
    std::vector<const FmiVariableIR*> binary_array_outputs;
    std::vector<Fmi3ValueReference> binary_array_output_references;
    std::vector<std::size_t> binary_array_output_extents;
    for (const auto& variable : model.variables) {
        if (variable.causality == "output" && variable.dimensions != 0) {
            if (variable.value_reference > std::numeric_limits<Fmi3ValueReference>::max()) {
                throw std::invalid_argument(
                    variable.name + ": FMI 3 array value reference is out of range");
            }
            const auto reference =
                static_cast<Fmi3ValueReference>(variable.value_reference);
            const auto extent = array_extent(variable);
            const auto append_numeric = [&](auto& group) {
                group.array_outputs.push_back(&variable);
                group.array_output_references.push_back(reference);
                group.array_output_extents.push_back(extent);
            };
            if (variable.type == "Float64") {
                array_outputs.push_back(&variable);
                array_output_references.push_back(reference);
                array_output_extents.push_back(extent);
            } else if (variable.type == "Float32") {
                append_numeric(float32);
            } else if (variable.type == "Int8") {
                append_numeric(int8);
            } else if (variable.type == "UInt8") {
                append_numeric(uint8);
            } else if (variable.type == "Int16") {
                append_numeric(int16);
            } else if (variable.type == "UInt16") {
                append_numeric(uint16);
            } else if (variable.type == "Int32") {
                append_numeric(int32);
            } else if (variable.type == "UInt32") {
                append_numeric(uint32);
            } else if (variable.type == "Int64" || variable.type == "Enumeration") {
                append_numeric(int64);
            } else if (variable.type == "UInt64") {
                append_numeric(uint64);
            } else if (variable.type == "Boolean") {
                boolean_array_outputs.push_back(&variable);
                boolean_array_output_references.push_back(reference);
                boolean_array_output_extents.push_back(extent);
            } else if (variable.type == "String") {
                string_array_outputs.push_back(&variable);
                string_array_output_references.push_back(reference);
                string_array_output_extents.push_back(extent);
            } else if (variable.type == "Binary") {
                binary_array_outputs.push_back(&variable);
                binary_array_output_references.push_back(reference);
                binary_array_output_extents.push_back(extent);
            } else {
                throw std::invalid_argument(
                    variable.name +
                    ": array smoke does not support this FMI 3 output type");
            }
            continue;
        }
        if (variable.causality == "output" && variable.dimensions == 0 &&
            variable.value_reference <= std::numeric_limits<Fmi3ValueReference>::max()) {
            const auto reference = static_cast<Fmi3ValueReference>(variable.value_reference);
            if (variable.type == "Float64") {
                float_outputs.push_back(&variable);
                float_output_references.push_back(reference);
            } else if (variable.type == "Float32") {
                float32.outputs.push_back(&variable);
                float32.output_references.push_back(reference);
            } else if (variable.type == "Int8") {
                int8.outputs.push_back(&variable);
                int8.output_references.push_back(reference);
            } else if (variable.type == "UInt8") {
                uint8.outputs.push_back(&variable);
                uint8.output_references.push_back(reference);
            } else if (variable.type == "Int16") {
                int16.outputs.push_back(&variable);
                int16.output_references.push_back(reference);
            } else if (variable.type == "UInt16") {
                uint16.outputs.push_back(&variable);
                uint16.output_references.push_back(reference);
            } else if (variable.type == "Int32") {
                int32.outputs.push_back(&variable);
                int32.output_references.push_back(reference);
            } else if (variable.type == "UInt32") {
                uint32.outputs.push_back(&variable);
                uint32.output_references.push_back(reference);
            } else if (variable.type == "Int64" || variable.type == "Enumeration") {
                int64.outputs.push_back(&variable);
                int64.output_references.push_back(reference);
            } else if (variable.type == "UInt64") {
                uint64.outputs.push_back(&variable);
                uint64.output_references.push_back(reference);
            } else if (variable.type == "Boolean") {
                boolean_outputs.push_back(&variable);
                boolean_output_references.push_back(reference);
            } else if (variable.type == "String") {
                string_outputs.push_back(&variable);
                string_output_references.push_back(reference);
            } else if (variable.type == "Binary") {
                binary_outputs.push_back(&variable);
                binary_output_references.push_back(reference);
            } else if (variable.type == "Clock") {
                clock_outputs.push_back(&variable);
                clock_output_references.push_back(reference);
            }
        }
    }
    if (float_outputs.empty() && float32.outputs.empty() && int8.outputs.empty() &&
        uint8.outputs.empty() && int16.outputs.empty() && uint16.outputs.empty() &&
        int32.outputs.empty() && uint32.outputs.empty() && int64.outputs.empty() &&
        uint64.outputs.empty() && boolean_outputs.empty() && string_outputs.empty() &&
        binary_outputs.empty() && array_outputs.empty() &&
        float32.array_outputs.empty() && int8.array_outputs.empty() &&
        uint8.array_outputs.empty() && int16.array_outputs.empty() &&
        uint16.array_outputs.empty() && int32.array_outputs.empty() &&
        uint32.array_outputs.empty() && int64.array_outputs.empty() &&
        uint64.array_outputs.empty() && boolean_array_outputs.empty() &&
        string_array_outputs.empty() && binary_array_outputs.empty() &&
        clock_outputs.empty()) {
        throw std::invalid_argument(
            "FMI smoke requires at least one supported FMI 3 output");
    }

    std::unique_ptr<TemporaryDirectory> temporary;
    const auto directory = prepare_fmu_directory(path, model, interface, temporary);
    DynamicLibrary library(directory / interface_binary_relative(model, interface));
    const auto get_version = library.symbol<Fmi3GetVersion>("fmi3GetVersion");
    const auto instantiate = library.symbol<Fmi3InstantiateModelExchange>(
        "fmi3InstantiateModelExchange");
    const auto free_instance = library.symbol<Fmi3FreeInstance>("fmi3FreeInstance");
    const auto enter_initialization = library.symbol<Fmi3EnterInitializationMode>(
        "fmi3EnterInitializationMode");
    const auto exit_initialization = library.symbol<Fmi3ExitInitializationMode>(
        "fmi3ExitInitializationMode");
    const auto enter_continuous = library.symbol<Fmi3EnterContinuousTimeMode>(
        "fmi3EnterContinuousTimeMode");
    const auto completed_step = library.symbol<Fmi3CompletedIntegratorStep>(
        "fmi3CompletedIntegratorStep");
    const auto set_time = library.symbol<Fmi3SetTime>("fmi3SetTime");
    const auto set_states = library.symbol<Fmi3SetContinuousStates>(
        "fmi3SetContinuousStates");
    const auto get_derivatives = library.symbol<Fmi3GetContinuousStateDerivatives>(
        "fmi3GetContinuousStateDerivatives");
    const auto get_states = library.symbol<Fmi3GetContinuousStates>(
        "fmi3GetContinuousStates");
    const auto get_nominals = library.symbol<Fmi3GetNominalsOfContinuousStates>(
        "fmi3GetNominalsOfContinuousStates");
    const auto get_state_count = library.symbol<Fmi3GetNumberOfContinuousStates>(
        "fmi3GetNumberOfContinuousStates");
    const auto get_indicator_count = library.symbol<Fmi3GetNumberOfEventIndicators>(
        "fmi3GetNumberOfEventIndicators");
    Fmi3GetEventIndicators get_event_indicators{};
    const auto enter_event_mode = library.symbol<Fmi3EnterEventMode>(
        "fmi3EnterEventMode");
    const auto update_discrete_states = library.symbol<Fmi3UpdateDiscreteStates>(
        "fmi3UpdateDiscreteStates");
    const auto terminate = library.symbol<Fmi3Terminate>("fmi3Terminate");
    const auto set_float64 = library.symbol<Fmi3SetFloat64>("fmi3SetFloat64");
    const auto get_float64 = library.symbol<Fmi3GetFloat64>("fmi3GetFloat64");
    Fmi3SetBoolean set_boolean{};
    Fmi3GetBoolean get_boolean{};
    Fmi3SetClock set_clock{};
    Fmi3GetClock get_clock{};
    Fmi3SetString set_string{};
    Fmi3GetString get_string{};
    Fmi3SetBinary set_binary{};
    Fmi3GetBinary get_binary{};
    const auto load_numeric = [&](auto& group) {
        if (!group.input_references.empty() || !group.array_input_references.empty()) {
            const std::string symbol = "fmi3Set" + group.symbol_suffix;
            group.set = library.symbol<decltype(group.set)>(symbol.c_str());
        }
        if (!group.output_references.empty() || !group.array_output_references.empty()) {
            const std::string symbol = "fmi3Get" + group.symbol_suffix;
            group.get = library.symbol<decltype(group.get)>(symbol.c_str());
        }
    };
    load_numeric(float32);
    load_numeric(int8);
    load_numeric(uint8);
    load_numeric(int16);
    load_numeric(uint16);
    load_numeric(int32);
    load_numeric(uint32);
    load_numeric(int64);
    load_numeric(uint64);
    if (!boolean_input_references.empty() || !boolean_array_input_references.empty()) {
        set_boolean = library.symbol<Fmi3SetBoolean>("fmi3SetBoolean");
    }
    if (!boolean_output_references.empty() || !boolean_array_output_references.empty()) {
        get_boolean = library.symbol<Fmi3GetBoolean>("fmi3GetBoolean");
    }
    if (!clock_input_references.empty()) {
        set_clock = library.symbol<Fmi3SetClock>("fmi3SetClock");
    }
    Fmi3GetIntervalDecimal get_interval_decimal{};
    Fmi3GetShiftDecimal get_shift_decimal{};
    if (!clock_output_references.empty()) {
        get_clock = library.symbol<Fmi3GetClock>("fmi3GetClock");
        get_interval_decimal = library.symbol<Fmi3GetIntervalDecimal>(
            "fmi3GetIntervalDecimal");
        get_shift_decimal = library.symbol<Fmi3GetShiftDecimal>(
            "fmi3GetShiftDecimal");
    }
    if (!string_input_references.empty() || !string_array_input_references.empty()) {
        set_string = library.symbol<Fmi3SetString>("fmi3SetString");
    }
    if (!string_output_references.empty() || !string_array_output_references.empty()) {
        get_string = library.symbol<Fmi3GetString>("fmi3GetString");
    }
    if (!binary_input_references.empty() || !binary_array_input_references.empty()) {
        set_binary = library.symbol<Fmi3SetBinary>("fmi3SetBinary");
    }
    if (!binary_output_references.empty() || !binary_array_output_references.empty()) {
        get_binary = library.symbol<Fmi3GetBinary>("fmi3GetBinary");
    }
    const char* runtime_version = get_version();
    if (runtime_version == nullptr || std::string_view(runtime_version) != "3.0") {
        throw std::runtime_error("FMU runtime version does not match FMI 3.0 metadata");
    }

    const auto resource_path = file_uri(directory / "resources");
    Fmi3Instance instance = instantiate(
        "smave-me-smoke", model.instantiation_token.c_str(), resource_path.c_str(),
        false, false, nullptr, nullptr);
    if (instance == nullptr) throw std::runtime_error("fmi3InstantiateModelExchange returned null");
    struct InstanceCleanup {
        Fmi3Instance instance;
        Fmi3FreeInstance free_instance;
        ~InstanceCleanup() { if (instance != nullptr) free_instance(instance); }
    } cleanup{instance, free_instance};

    FmiSmokeResult result;
    result.model_name = model.model_name;
    result.source_hash = model.source_hash;
    result.interface_kind = interface.kind;
    result.model_identifier = interface.model_identifier;
    result.end_time = end_time;
    result.step_size = step_size;
    require_status(
        enter_initialization(instance, true, 1.0e-6, 0.0, true, end_time),
        result.warnings, "fmi3EnterInitializationMode");
    if (!float_input_references.empty()) {
        require_status(
            set_float64(instance, float_input_references.data(),
                        float_input_references.size(), float_input_values.data(),
                        float_input_values.size()),
            result.warnings, "fmi3SetFloat64");
    }
    if (!array_input_references.empty()) {
        require_status(
            set_float64(instance, array_input_references.data(),
                        array_input_references.size(), array_input_values.data(),
                        array_input_values.size()),
            result.warnings, "fmi3SetFloat64 arrays");
    }
    const auto set_numeric = [&](const auto& group) {
        if (group.input_references.empty() && group.array_input_references.empty()) return;
        auto references = group.input_references;
        references.insert(references.end(), group.array_input_references.begin(),
                          group.array_input_references.end());
        auto values = group.input_values;
        values.insert(values.end(), group.array_input_values.begin(),
                      group.array_input_values.end());
        require_status(
            group.set(instance, references.data(), references.size(), values.data(),
                      values.size()),
            result.warnings, "fmi3Set" + group.symbol_suffix);
    };
    set_numeric(float32);
    set_numeric(int8);
    set_numeric(uint8);
    set_numeric(int16);
    set_numeric(uint16);
    set_numeric(int32);
    set_numeric(uint32);
    set_numeric(int64);
    set_numeric(uint64);
    if (!boolean_input_references.empty()) {
        auto values = std::make_unique<Fmi3Boolean[]>(boolean_input_values.size());
        std::copy(boolean_input_values.begin(), boolean_input_values.end(), values.get());
        require_status(
            set_boolean(instance, boolean_input_references.data(),
                        boolean_input_references.size(), values.get(),
                        boolean_input_values.size()),
            result.warnings, "fmi3SetBoolean");
    }
    if (!boolean_array_input_references.empty()) {
        auto values = std::make_unique<Fmi3Boolean[]>(boolean_array_input_values.size());
        std::copy(boolean_array_input_values.begin(), boolean_array_input_values.end(),
                  values.get());
        require_status(
            set_boolean(instance, boolean_array_input_references.data(),
                        boolean_array_input_references.size(), values.get(),
                        boolean_array_input_values.size()),
            result.warnings, "fmi3SetBoolean arrays");
    }
    if (!clock_input_references.empty()) {
        auto values = std::make_unique<Fmi3Clock[]>(clock_input_values.size());
        std::copy(clock_input_values.begin(), clock_input_values.end(), values.get());
        require_status(
            set_clock(instance, clock_input_references.data(), clock_input_references.size(),
                      values.get(), clock_input_values.size()),
            result.warnings, "fmi3SetClock");
    }
    if (!string_input_references.empty()) {
        require_status(
            set_string(instance, string_input_references.data(),
                       string_input_references.size(), string_input_values.data(),
                       string_input_values.size()),
            result.warnings, "fmi3SetString");
    }
    if (!string_array_input_references.empty()) {
        require_status(
            set_string(instance, string_array_input_references.data(),
                       string_array_input_references.size(),
                       string_array_input_values.data(), string_array_input_values.size()),
            result.warnings, "fmi3SetString arrays");
    }
    if (!binary_input_references.empty()) {
        require_status(
            set_binary(instance, binary_input_references.data(),
                       binary_input_references.size(), binary_input_sizes.data(),
                       binary_input_values.data(), binary_input_values.size()),
            result.warnings, "fmi3SetBinary");
    }
    if (!binary_array_input_references.empty()) {
        require_status(
            set_binary(instance, binary_array_input_references.data(),
                       binary_array_input_references.size(),
                       binary_array_input_sizes.data(), binary_array_input_values.data(),
                       binary_array_input_values.size()),
            result.warnings, "fmi3SetBinary arrays");
    }
    require_status(
        exit_initialization(instance), result.warnings, "fmi3ExitInitializationMode");
    std::size_t state_count{};
    require_status(
        get_state_count(instance, &state_count), result.warnings,
        "fmi3GetNumberOfContinuousStates");
    if (state_count == 0 || state_count > 1000000U) {
        throw std::runtime_error("FMI ModelExchange continuous state count is unsupported");
    }
    const auto record_nominals = [&]() {
        std::vector<double> nominals(state_count);
        require_status(
            get_nominals(instance, nominals.data(), nominals.size()), result.warnings,
            "fmi3GetNominalsOfContinuousStates");
        for (const double nominal : nominals) {
            if (!std::isfinite(nominal) || !(nominal > 0.0)) {
                throw std::runtime_error(
                    "FMI 3 continuous-state nominal must be finite and positive");
            }
        }
        const auto [minimum, maximum] = std::minmax_element(
            nominals.begin(), nominals.end());
        if (result.continuous_state_nominal_updates == 0) {
            result.minimum_continuous_state_nominal = *minimum;
            result.maximum_continuous_state_nominal = *maximum;
        } else {
            result.minimum_continuous_state_nominal = std::min(
                result.minimum_continuous_state_nominal, *minimum);
            result.maximum_continuous_state_nominal = std::max(
                result.maximum_continuous_state_nominal, *maximum);
        }
        ++result.continuous_state_nominal_updates;
    };
    std::optional<double> scheduled_event_time;
    {
        std::optional<double> updated_scheduled_event_time;
        Fmi3Boolean update_needed = true;
        std::size_t iterations{};
        ++result.event_mode_entries;
        while (update_needed) {
            if (++iterations > 1024U) {
                throw std::runtime_error(
                    "FMI ModelExchange initialization event iteration did not reach a fixed point");
            }
            Fmi3Boolean terminate_simulation{};
            Fmi3Boolean nominals_changed{};
            Fmi3Boolean continuous_states_changed{};
            Fmi3Boolean next_event_time_defined{};
            Fmi3Float64 next_event_time{};
            require_status(
                update_discrete_states(
                    instance, &update_needed, &terminate_simulation, &nominals_changed,
                    &continuous_states_changed, &next_event_time_defined, &next_event_time),
                result.warnings, "fmi3UpdateDiscreteStates after initialization");
            ++result.discrete_update_iterations;
            if (terminate_simulation) {
                throw std::runtime_error(
                    "FMI ModelExchange initialization rejects termination");
            }
            if (nominals_changed) record_nominals();
            if (next_event_time_defined) {
                if (!std::isfinite(next_event_time) || next_event_time <= 1.0e-12 ||
                    next_event_time > end_time + 1.0e-12) {
                    throw std::runtime_error(
                        "FMI ModelExchange nextEventTime must be finite, future, and within the smoke horizon");
                }
                updated_scheduled_event_time = next_event_time;
            } else {
                updated_scheduled_event_time.reset();
            }
        }
        scheduled_event_time = updated_scheduled_event_time;
    }
    if (!clock_outputs.empty()) {
        std::vector<Fmi3Float64> intervals(clock_outputs.size());
        std::vector<Fmi3IntervalQualifier> qualifiers(clock_outputs.size());
        std::vector<Fmi3Float64> shifts(clock_outputs.size());
        require_status(
            get_interval_decimal(instance, clock_output_references.data(),
                                 clock_output_references.size(), intervals.data(),
                                 qualifiers.data()),
            result.warnings, "fmi3GetIntervalDecimal");
        require_status(
            get_shift_decimal(instance, clock_output_references.data(),
                              clock_output_references.size(), shifts.data()),
            result.warnings, "fmi3GetShiftDecimal");
        for (std::size_t index = 0; index < clock_outputs.size(); ++index) {
            const auto& name = clock_outputs[index]->name;
            const auto qualifier = qualifiers[index];
            std::string qualifier_name;
            if (qualifier == Fmi3IntervalQualifier::not_yet_known) {
                qualifier_name = "notYetKnown";
            } else if (qualifier == Fmi3IntervalQualifier::unchanged) {
                qualifier_name = "unchanged";
            } else if (qualifier == Fmi3IntervalQualifier::changed) {
                qualifier_name = "changed";
            } else {
                throw std::runtime_error(
                    "fmi3GetIntervalDecimal returned an invalid qualifier");
            }
            if (!std::isfinite(shifts[index]) || shifts[index] < 0.0) {
                throw std::runtime_error(
                    name + ": FMI Clock shift must be finite and non-negative");
            }
            result.clock_interval_qualifiers.emplace(name, qualifier_name);
            result.clock_shifts.emplace(name, shifts[index]);
            if (qualifier != Fmi3IntervalQualifier::not_yet_known) {
                if (!std::isfinite(intervals[index]) || !(intervals[index] > 0.0) ||
                    shifts[index] >= intervals[index]) {
                    throw std::runtime_error(
                        name + ": FMI Clock interval/shift is invalid");
                }
                result.clock_intervals.emplace(name, intervals[index]);
            }
        }
    }
    require_status(
        enter_continuous(instance), result.warnings, "fmi3EnterContinuousTimeMode");
    std::size_t indicator_count{};
    require_status(
        get_indicator_count(instance, &indicator_count), result.warnings,
        "fmi3GetNumberOfEventIndicators");
    if (indicator_count != model.number_of_event_indicators) {
        throw std::runtime_error(
            "FMI ModelExchange runtime event indicator count " +
            std::to_string(indicator_count) + " does not match metadata " +
            std::to_string(model.number_of_event_indicators));
    }
    if (indicator_count > 1024U) {
        throw std::runtime_error(
            "FMI ModelExchange event indicator count exceeds the smoke limit");
    }
    if (indicator_count > 0) {
        get_event_indicators = library.symbol<Fmi3GetEventIndicators>(
            "fmi3GetEventIndicators");
    }
    std::vector<double> state(state_count);
    require_status(
        get_states(instance, state.data(), state.size()), result.warnings,
        "fmi3GetContinuousStates");

    const auto set_point = [&](double time, const std::vector<double>& values) {
        require_status(set_time(instance, time), result.warnings, "fmi3SetTime");
        require_status(
            set_states(instance, values.data(), values.size()), result.warnings,
            "fmi3SetContinuousStates");
    };
    const auto derivatives = [&](double time, const std::vector<double>& values) {
        set_point(time, values);
        std::vector<double> result_values(values.size());
        require_status(
            get_derivatives(instance, result_values.data(), result_values.size()),
            result.warnings, "fmi3GetContinuousStateDerivatives");
        for (const double value : result_values) {
            if (!std::isfinite(value)) throw std::runtime_error("FMU derivative is NaN/Inf");
        }
        return result_values;
    };
    const auto combine_state = [](
        const std::vector<double>& base,
        const std::vector<double>& increment,
        double factor) {
        std::vector<double> combined(base.size());
        for (std::size_t index = 0; index < base.size(); ++index) {
            combined[index] = base[index] + factor * increment[index];
        }
        return combined;
    };
    const auto rk4_state = [&](double time, const std::vector<double>& values, double interval) {
        const auto first = derivatives(time, values);
        const auto second = derivatives(
            time + interval * 0.5, combine_state(values, first, interval * 0.5));
        const auto third = derivatives(
            time + interval * 0.5, combine_state(values, second, interval * 0.5));
        const auto fourth = derivatives(
            time + interval, combine_state(values, third, interval));
        std::vector<double> next(values.size());
        for (std::size_t index = 0; index < values.size(); ++index) {
            next[index] = values[index] + interval / 6.0 *
                (first[index] + 2.0 * second[index] +
                 2.0 * third[index] + fourth[index]);
            if (!std::isfinite(next[index])) {
                throw std::runtime_error("FMI ModelExchange integrator produced NaN/Inf");
            }
        }
        return next;
    };
    const auto complete_step = [&]() {
        Fmi3Boolean enter_event_mode{};
        Fmi3Boolean terminate_simulation{};
        require_status(
            completed_step(instance, false, &enter_event_mode, &terminate_simulation),
            result.warnings, "fmi3CompletedIntegratorStep");
        if (enter_event_mode || terminate_simulation) {
            throw std::runtime_error(
                "FMI ModelExchange completedIntegratorStep requested unsupported step event or termination");
        }
    };
    const auto event_indicators = [&](double time, const std::vector<double>& values) {
        set_point(time, values);
        std::vector<double> indicators(indicator_count);
        require_status(
            get_event_indicators(instance, indicators.data(), indicators.size()), result.warnings,
            "fmi3GetEventIndicators");
        for (const double indicator : indicators) {
            if (!std::isfinite(indicator)) {
                throw std::runtime_error("FMI ModelExchange event indicator is NaN/Inf");
            }
        }
        return indicators;
    };
    const auto process_me_event = [&](double time, std::vector<double> values) {
        set_point(time, values);
        require_status(
            enter_event_mode(instance), result.warnings, "fmi3EnterEventMode");
        ++result.event_mode_entries;
        Fmi3Boolean update_needed = true;
        bool states_changed{};
        std::optional<double> updated_scheduled_event_time;
        std::size_t iterations{};
        while (update_needed) {
            if (++iterations > 1024U) {
                throw std::runtime_error(
                    "FMI ModelExchange event iteration did not reach a fixed point");
            }
            Fmi3Boolean terminate_simulation{};
            Fmi3Boolean nominals_changed{};
            Fmi3Boolean continuous_states_changed{};
            Fmi3Boolean next_event_time_defined{};
            Fmi3Float64 next_event_time{};
            require_status(
                update_discrete_states(
                    instance, &update_needed, &terminate_simulation, &nominals_changed,
                    &continuous_states_changed, &next_event_time_defined, &next_event_time),
                result.warnings, "fmi3UpdateDiscreteStates");
            ++result.discrete_update_iterations;
            states_changed = states_changed || continuous_states_changed;
            if (terminate_simulation) {
                throw std::runtime_error(
                    "FMI ModelExchange event smoke rejects termination");
            }
            if (nominals_changed) record_nominals();
            if (next_event_time_defined) {
                if (!std::isfinite(next_event_time) ||
                    next_event_time <= time + 1.0e-12 ||
                    next_event_time > end_time + 1.0e-12) {
                    throw std::runtime_error(
                        "FMI ModelExchange nextEventTime must be finite, future, and within the smoke horizon");
                }
                updated_scheduled_event_time = next_event_time;
            } else {
                updated_scheduled_event_time.reset();
            }
        }
        scheduled_event_time = updated_scheduled_event_time;
        if (states_changed) {
            require_status(
                get_states(instance, values.data(), values.size()), result.warnings,
                "fmi3GetContinuousStates after event");
        }
        require_status(
            enter_continuous(instance), result.warnings, "fmi3EnterContinuousTimeMode");
        return values;
    };
    const auto advance = [&](double start_time, std::vector<double> values, double interval) {
        const double target_time = start_time + interval;
        double current_time = start_time;
        std::size_t events{};
        while (current_time < target_time - 1.0e-12) {
            if (++events > 1024U) {
                throw std::runtime_error(
                    "FMI ModelExchange event processing did not reach the macro endpoint");
            }
            const bool time_event_in_interval = scheduled_event_time.has_value() &&
                *scheduled_event_time <= target_time + 1.0e-12;
            const double integration_target = time_event_in_interval
                ? std::min(target_time, *scheduled_event_time) : target_time;
            const double remaining = integration_target - current_time;
            auto candidate = rk4_state(current_time, values, remaining);
            if (indicator_count == 0) {
                set_point(integration_target, candidate);
                complete_step();
                current_time = integration_target;
                values = std::move(candidate);
                if (time_event_in_interval &&
                    std::abs(current_time - *scheduled_event_time) <= 1.0e-12) {
                    scheduled_event_time.reset();
                    ++result.time_event_splits;
                    ++result.time_events;
                    values = process_me_event(current_time, std::move(values));
                    continue;
                }
                return values;
            }
            const auto start_indicators = event_indicators(current_time, values);
            const auto end_indicators = event_indicators(integration_target, candidate);
            struct RootCandidate {
                double time{};
                std::size_t indicator{};
                std::vector<double> state;
                bool grazing{};
            };
            std::optional<RootCandidate> earliest;
            for (std::size_t indicator_index = 0;
                    indicator_index < indicator_count; ++indicator_index) {
                const double start_indicator = start_indicators[indicator_index];
                const double end_indicator = end_indicators[indicator_index];
                const bool crossing =
                    (start_indicator < 0.0 && end_indicator >= 0.0) ||
                    (start_indicator > 0.0 && end_indicator <= 0.0);
                if (!crossing) {
                    if (start_indicator * end_indicator <= 0.0 ||
                        std::abs(start_indicator) <= 1.0e-8 ||
                        std::abs(end_indicator) <= 1.0e-8) {
                        continue;
                    }
                    constexpr double inverse_golden = 0.6180339887498948482;
                    double search_left = current_time;
                    double search_right = integration_target;
                    double first = search_right -
                        inverse_golden * (search_right - search_left);
                    double second = search_left +
                        inverse_golden * (search_right - search_left);
                    const auto magnitude_at = [&](double time) {
                        const auto point = rk4_state(
                            current_time, values, time - current_time);
                        return std::abs(
                            event_indicators(time, point)[indicator_index]);
                    };
                    double first_value = magnitude_at(first);
                    double second_value = magnitude_at(second);
                    while (search_right - search_left > 1.0e-12) {
                        if (first_value > second_value) {
                            search_left = first;
                            first = second;
                            first_value = second_value;
                            second = search_left +
                                inverse_golden * (search_right - search_left);
                            second_value = magnitude_at(second);
                        } else {
                            search_right = second;
                            second = first;
                            second_value = first_value;
                            first = search_right -
                                inverse_golden * (search_right - search_left);
                            first_value = magnitude_at(first);
                        }
                    }
                    const double grazing_time = 0.5 * (search_left + search_right);
                    const double probe = std::max(
                        4.0e-12, (integration_target - current_time) / 64.0);
                    if (grazing_time <= current_time + probe ||
                        grazing_time + probe >= integration_target) {
                        continue;
                    }
                    auto grazing_state = rk4_state(
                        current_time, values, grazing_time - current_time);
                    const auto grazing_indicators = event_indicators(
                        grazing_time, grazing_state);
                    const double minimum = std::abs(
                        grazing_indicators[indicator_index]);
                    const double prominence = std::min(
                        magnitude_at(grazing_time - probe) - minimum,
                        magnitude_at(grazing_time + probe) - minimum);
                    if (minimum <= 1.0e-8 && prominence > 1.0e-8 &&
                        (!earliest.has_value() ||
                         grazing_time < earliest->time - 1.0e-12 ||
                         (std::abs(grazing_time - earliest->time) <= 1.0e-12 &&
                          indicator_index < earliest->indicator))) {
                        earliest = RootCandidate{
                            grazing_time, indicator_index,
                            std::move(grazing_state), true};
                    }
                    continue;
                }
                double left_time = current_time;
                double right_time = integration_target;
                double left_indicator = start_indicator;
                std::vector<double> right_state = candidate;
                for (std::size_t iteration = 0; iteration < 80U &&
                        right_time - left_time > 1.0e-12; ++iteration) {
                    const double midpoint = left_time + (right_time - left_time) * 0.5;
                    auto midpoint_state = rk4_state(
                        current_time, values, midpoint - current_time);
                    const auto midpoint_indicators = event_indicators(midpoint, midpoint_state);
                    const double midpoint_indicator = midpoint_indicators[indicator_index];
                    const bool left_crossing =
                        (left_indicator < 0.0 && midpoint_indicator >= 0.0) ||
                        (left_indicator > 0.0 && midpoint_indicator <= 0.0);
                    if (left_crossing) {
                        right_time = midpoint;
                        right_state = std::move(midpoint_state);
                    } else {
                        left_time = midpoint;
                        left_indicator = midpoint_indicator;
                    }
                }
                if (!earliest.has_value() || right_time < earliest->time - 1.0e-12 ||
                    (std::abs(right_time - earliest->time) <= 1.0e-12 &&
                     indicator_index < earliest->indicator)) {
                    earliest = RootCandidate{
                        right_time, indicator_index, std::move(right_state), false};
                }
            }
            if (!earliest.has_value()) {
                set_point(integration_target, candidate);
                complete_step();
                current_time = integration_target;
                values = std::move(candidate);
                if (time_event_in_interval &&
                    std::abs(current_time - *scheduled_event_time) <= 1.0e-12) {
                    scheduled_event_time.reset();
                    ++result.time_event_splits;
                    ++result.time_events;
                    values = process_me_event(current_time, std::move(values));
                    continue;
                }
                return values;
            }
            current_time = earliest->time;
            if (scheduled_event_time.has_value() &&
                std::abs(current_time - *scheduled_event_time) <= 1.0e-12) {
                scheduled_event_time.reset();
                ++result.time_event_splits;
                ++result.time_events;
            }
            values = process_me_event(current_time, std::move(earliest->state));
            ++result.model_exchange_roots;
            if (earliest->grazing) ++result.model_exchange_grazing_roots;
        }
        return values;
    };
    const auto sample_outputs = [&](double time) {
        FmiSmokeSample sample;
        sample.time = time;
        if (!float_outputs.empty()) {
            std::vector<double> values(float_outputs.size());
            require_status(
                get_float64(instance, float_output_references.data(),
                            float_output_references.size(), values.data(), values.size()),
                result.warnings, "fmi3GetFloat64");
            for (std::size_t index = 0; index < float_outputs.size(); ++index) {
                if (!std::isfinite(values[index])) {
                    throw std::runtime_error(
                        float_outputs[index]->name + ": FMU output is NaN/Inf");
                }
                sample.outputs.emplace(float_outputs[index]->name, values[index]);
            }
        }
        if (!array_outputs.empty()) {
            const auto element_count = std::accumulate(
                array_output_extents.begin(), array_output_extents.end(), std::size_t{},
                std::plus<>());
            std::vector<double> values(element_count);
            require_status(
                get_float64(instance, array_output_references.data(),
                            array_output_references.size(), values.data(), values.size()),
                result.warnings, "fmi3GetFloat64 arrays");
            std::size_t offset{};
            for (std::size_t index = 0; index < array_outputs.size(); ++index) {
                const auto extent = array_output_extents[index];
                auto& output = sample.array_outputs[array_outputs[index]->name];
                output.assign(values.begin() + static_cast<std::ptrdiff_t>(offset),
                              values.begin() + static_cast<std::ptrdiff_t>(offset + extent));
                if (!std::all_of(output.begin(), output.end(), [](double value) {
                        return std::isfinite(value);
                    })) {
                    throw std::runtime_error(
                        array_outputs[index]->name + ": FMU array output contains NaN/Inf");
                }
                offset += extent;
            }
        }
        const auto sample_numeric = [&](const auto& group) {
            if (group.outputs.empty() && group.array_outputs.empty()) return;
            using Value = typename std::decay_t<decltype(group)>::value_type;
            const auto convert = [&](Value value, const std::string& name) {
                const double converted = static_cast<double>(value);
                if (!std::isfinite(converted)) {
                    throw std::runtime_error(name + ": FMU output is NaN/Inf");
                }
                if constexpr (std::is_integral_v<Value>) {
                    constexpr double maximum_exact_integer = 9007199254740991.0;
                    if (converted < -maximum_exact_integer ||
                        converted > maximum_exact_integer) {
                        throw std::runtime_error(
                            name +
                            ": FMI integer output is not exactly representable as double");
                    }
                }
                return converted;
            };
            if (!group.outputs.empty()) {
                std::vector<Value> values(group.outputs.size());
                require_status(
                    group.get(instance, group.output_references.data(),
                              group.output_references.size(), values.data(), values.size()),
                    result.warnings, "fmi3Get" + group.symbol_suffix);
                for (std::size_t index = 0; index < group.outputs.size(); ++index) {
                    sample.outputs.emplace(
                        group.outputs[index]->name,
                        convert(values[index], group.outputs[index]->name));
                }
            }
            if (!group.array_outputs.empty()) {
                const auto element_count = std::accumulate(
                    group.array_output_extents.begin(), group.array_output_extents.end(),
                    std::size_t{}, std::plus<>());
                std::vector<Value> values(element_count);
                require_status(
                    group.get(instance, group.array_output_references.data(),
                              group.array_output_references.size(), values.data(), values.size()),
                    result.warnings, "fmi3Get" + group.symbol_suffix + " arrays");
                std::size_t offset{};
                for (std::size_t index = 0; index < group.array_outputs.size(); ++index) {
                    auto& output = sample.array_outputs[group.array_outputs[index]->name];
                    const auto extent = group.array_output_extents[index];
                    output.reserve(extent);
                    for (std::size_t element = 0; element < extent; ++element) {
                        output.push_back(convert(
                            values[offset + element], group.array_outputs[index]->name));
                    }
                    offset += extent;
                }
            }
        };
        sample_numeric(float32);
        sample_numeric(int8);
        sample_numeric(uint8);
        sample_numeric(int16);
        sample_numeric(uint16);
        sample_numeric(int32);
        sample_numeric(uint32);
        sample_numeric(int64);
        sample_numeric(uint64);
        if (!boolean_outputs.empty()) {
            auto values = std::make_unique<Fmi3Boolean[]>(boolean_outputs.size());
            require_status(
                get_boolean(instance, boolean_output_references.data(),
                            boolean_output_references.size(), values.get(),
                            boolean_outputs.size()),
                result.warnings, "fmi3GetBoolean");
            for (std::size_t index = 0; index < boolean_outputs.size(); ++index) {
                sample.outputs.emplace(boolean_outputs[index]->name, values[index] ? 1.0 : 0.0);
            }
        }
        if (!boolean_array_outputs.empty()) {
            const auto element_count = std::accumulate(
                boolean_array_output_extents.begin(), boolean_array_output_extents.end(),
                std::size_t{}, std::plus<>());
            auto values = std::make_unique<Fmi3Boolean[]>(element_count);
            require_status(
                get_boolean(instance, boolean_array_output_references.data(),
                            boolean_array_output_references.size(), values.get(),
                            element_count),
                result.warnings, "fmi3GetBoolean arrays");
            std::size_t offset{};
            for (std::size_t index = 0; index < boolean_array_outputs.size(); ++index) {
                auto& output = sample.array_outputs[boolean_array_outputs[index]->name];
                const auto extent = boolean_array_output_extents[index];
                output.reserve(extent);
                for (std::size_t element = 0; element < extent; ++element) {
                    output.push_back(values[offset + element] ? 1.0 : 0.0);
                }
                offset += extent;
            }
        }
        if (!clock_outputs.empty()) {
            auto values = std::make_unique<Fmi3Clock[]>(clock_outputs.size());
            require_status(
                get_clock(instance, clock_output_references.data(),
                          clock_output_references.size(), values.get(), clock_outputs.size()),
                result.warnings, "fmi3GetClock");
            for (std::size_t index = 0; index < clock_outputs.size(); ++index) {
                sample.outputs.emplace(clock_outputs[index]->name, values[index] ? 1.0 : 0.0);
            }
        }
        if (!string_outputs.empty()) {
            std::vector<Fmi3String> values(string_outputs.size());
            require_status(
                get_string(instance, string_output_references.data(),
                           string_output_references.size(), values.data(), values.size()),
                result.warnings, "fmi3GetString");
            for (std::size_t index = 0; index < string_outputs.size(); ++index) {
                if (values[index] == nullptr) {
                    throw std::runtime_error(
                        string_outputs[index]->name + ": FMU String output is null");
                }
                sample.string_outputs.emplace(string_outputs[index]->name, values[index]);
            }
        }
        if (!string_array_outputs.empty()) {
            const auto element_count = std::accumulate(
                string_array_output_extents.begin(), string_array_output_extents.end(),
                std::size_t{}, std::plus<>());
            std::vector<Fmi3String> values(element_count);
            require_status(
                get_string(instance, string_array_output_references.data(),
                           string_array_output_references.size(), values.data(), values.size()),
                result.warnings, "fmi3GetString arrays");
            std::size_t offset{};
            for (std::size_t index = 0; index < string_array_outputs.size(); ++index) {
                auto& output =
                    sample.string_array_outputs[string_array_outputs[index]->name];
                const auto extent = string_array_output_extents[index];
                output.reserve(extent);
                for (std::size_t element = 0; element < extent; ++element) {
                    const auto value = values[offset + element];
                    if (value == nullptr) {
                        throw std::runtime_error(
                            string_array_outputs[index]->name +
                            ": FMU String array output is null");
                    }
                    output.emplace_back(value);
                }
                offset += extent;
            }
        }
        if (!binary_outputs.empty()) {
            std::vector<std::size_t> sizes(binary_outputs.size());
            std::vector<const std::uint8_t*> values(binary_outputs.size());
            require_status(
                get_binary(instance, binary_output_references.data(),
                           binary_output_references.size(), sizes.data(), values.data(),
                           values.size()),
                result.warnings, "fmi3GetBinary");
            for (std::size_t index = 0; index < binary_outputs.size(); ++index) {
                if (sizes[index] > 16U * 1024U * 1024U ||
                    (sizes[index] != 0 && values[index] == nullptr)) {
                    throw std::runtime_error(
                        binary_outputs[index]->name + ": FMU Binary output is invalid");
                }
                auto& output = sample.binary_outputs[binary_outputs[index]->name];
                if (sizes[index] != 0) {
                    output.assign(values[index], values[index] + sizes[index]);
                }
            }
        }
        if (!binary_array_outputs.empty()) {
            const auto element_count = std::accumulate(
                binary_array_output_extents.begin(), binary_array_output_extents.end(),
                std::size_t{}, std::plus<>());
            std::vector<std::size_t> sizes(element_count);
            std::vector<const std::uint8_t*> values(element_count);
            require_status(
                get_binary(instance, binary_array_output_references.data(),
                           binary_array_output_references.size(), sizes.data(), values.data(),
                           values.size()),
                result.warnings, "fmi3GetBinary arrays");
            std::size_t offset{};
            for (std::size_t index = 0; index < binary_array_outputs.size(); ++index) {
                auto& output =
                    sample.binary_array_outputs[binary_array_outputs[index]->name];
                const auto extent = binary_array_output_extents[index];
                output.resize(extent);
                for (std::size_t element = 0; element < extent; ++element) {
                    const auto value_index = offset + element;
                    if (sizes[value_index] > 16U * 1024U * 1024U ||
                        (sizes[value_index] != 0 && values[value_index] == nullptr)) {
                        throw std::runtime_error(
                            binary_array_outputs[index]->name +
                            ": FMU Binary array output is invalid");
                    }
                    if (sizes[value_index] != 0) {
                        output[element].assign(
                            values[value_index], values[value_index] + sizes[value_index]);
                    }
                }
                offset += extent;
            }
        }
        return sample;
    };
    set_point(0.0, state);
    result.samples.push_back(sample_outputs(0.0));

    Fmi3GetFmuState get_fmu_state{};
    Fmi3SetFmuState set_fmu_state{};
    Fmi3FreeFmuState free_fmu_state{};
    Fmi3SerializedFmuStateSize serialized_fmu_state_size{};
    Fmi3SerializeFmuState serialize_fmu_state{};
    Fmi3DeserializeFmuState deserialize_fmu_state{};
    if (capability_enabled(interface, "canGetAndSetFMUState")) {
        get_fmu_state = library.symbol<Fmi3GetFmuState>("fmi3GetFMUState");
        set_fmu_state = library.symbol<Fmi3SetFmuState>("fmi3SetFMUState");
        free_fmu_state = library.symbol<Fmi3FreeFmuState>("fmi3FreeFMUState");
        result.state_roundtrip_attempted = true;
    }
    if (capability_enabled(interface, "canSerializeFMUState")) {
        if (!result.state_roundtrip_attempted) {
            throw std::invalid_argument(
                "canSerializeFMUState requires canGetAndSetFMUState");
        }
        serialized_fmu_state_size = library.symbol<Fmi3SerializedFmuStateSize>(
            "fmi3SerializedFMUStateSize");
        serialize_fmu_state = library.symbol<Fmi3SerializeFmuState>(
            "fmi3SerializeFMUState");
        deserialize_fmu_state = library.symbol<Fmi3DeserializeFmuState>(
            "fmi3DeserializeFMUState");
        result.state_serialization_attempted = true;
    }
    Fmi3FmuState saved_state{};
    struct StateCleanup {
        Fmi3Instance instance{};
        Fmi3FreeFmuState free_state{};
        Fmi3FmuState* state{};
        ~StateCleanup() {
            if (free_state != nullptr && state != nullptr && *state != nullptr) {
                (void)free_state(instance, state);
            }
        }
    } state_cleanup{instance, free_fmu_state, &saved_state};
    std::vector<double> saved_continuous_state;
    std::optional<double> saved_scheduled_event_time;
    for (std::size_t step = 0; step < rounded_steps; ++step) {
        const double current_time = static_cast<double>(step) * step_size;
        if (step == 0 && result.state_roundtrip_attempted) {
            saved_continuous_state = state;
            saved_scheduled_event_time = scheduled_event_time;
            require_status(
                get_fmu_state(instance, &saved_state), result.warnings,
                "fmi3GetFMUState");
            if (saved_state == nullptr) throw std::runtime_error("fmi3GetFMUState returned null");
            if (result.state_serialization_attempted) {
                std::size_t serialized_size{};
                require_status(
                    serialized_fmu_state_size(instance, saved_state, &serialized_size),
                    result.warnings, "fmi3SerializedFMUStateSize");
                if (serialized_size == 0 || serialized_size > 16U * 1024U * 1024U) {
                    throw std::runtime_error(
                        "FMI serialized state size must be within 1 byte and 16 MiB");
                }
                std::vector<std::uint8_t> serialized(serialized_size);
                require_status(
                    serialize_fmu_state(instance, saved_state, serialized.data(),
                                        serialized.size()),
                    result.warnings, "fmi3SerializeFMUState");
                require_status(
                    free_fmu_state(instance, &saved_state), result.warnings,
                    "fmi3FreeFMUState before deserialization");
                require_status(
                    deserialize_fmu_state(instance, serialized.data(), serialized.size(),
                                          &saved_state),
                    result.warnings, "fmi3DeserializeFMUState");
                if (saved_state == nullptr) {
                    throw std::runtime_error("fmi3DeserializeFMUState returned null");
                }
                result.serialized_state_bytes = serialized.size();
            }
        }
        state = advance(current_time, state, step_size);
        result.samples.push_back(sample_outputs(current_time + step_size));
        if (step == 0 && result.state_roundtrip_attempted) {
            const auto expected = result.samples.back().outputs;
            const auto expected_strings = result.samples.back().string_outputs;
            const auto expected_binaries = result.samples.back().binary_outputs;
            const auto expected_arrays = result.samples.back().array_outputs;
            const auto expected_string_arrays =
                result.samples.back().string_array_outputs;
            const auto expected_binary_arrays =
                result.samples.back().binary_array_outputs;
            require_status(
                set_fmu_state(instance, saved_state), result.warnings,
                "fmi3SetFMUState");
            scheduled_event_time = saved_scheduled_event_time;
            auto replay_state = advance(0.0, saved_continuous_state, step_size);
            const auto replay = sample_outputs(step_size);
            for (const auto& [name, value] : expected) {
                result.maximum_state_replay_error = std::max(
                    result.maximum_state_replay_error,
                    std::abs(replay.outputs.at(name) - value));
            }
            if (replay.string_outputs != expected_strings) {
                throw std::runtime_error("FMI state replay String output mismatch");
            }
            if (replay.binary_outputs != expected_binaries) {
                throw std::runtime_error("FMI state replay Binary output mismatch");
            }
            if (replay.array_outputs != expected_arrays) {
                throw std::runtime_error("FMI state replay array output mismatch");
            }
            if (replay.string_array_outputs != expected_string_arrays) {
                throw std::runtime_error("FMI state replay String array output mismatch");
            }
            if (replay.binary_array_outputs != expected_binary_arrays) {
                throw std::runtime_error("FMI state replay Binary array output mismatch");
            }
            state = std::move(replay_state);
            result.state_roundtrip_passed = result.maximum_state_replay_error <= 1.0e-12;
            result.state_serialization_passed =
                result.state_serialization_attempted && result.state_roundtrip_passed;
            require_status(
                free_fmu_state(instance, &saved_state), result.warnings,
                "fmi3FreeFMUState");
            if (saved_state != nullptr || !result.state_roundtrip_passed) {
                throw std::runtime_error("FMI ModelExchange state replay mismatch");
            }
        }
    }
    require_status(terminate(instance), result.warnings, "fmi3Terminate");
    result.success = true;
    result.message =
        "opt-in FMI 3.0 ModelExchange RK4/event smoke and state replay completed";
    return result;
}

void write_fmi_import_report(
    const FmiBlackboxIR& model,
    const std::filesystem::path& path) {
    model.validate();
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write FMI import report: " + path.string());
    output << "SMAVE_FMI_IMPORT_REPORT 1\n"
           << "MODEL " << std::quoted(model.model_name) << '\n'
           << "FMI_VERSION " << std::quoted(model.fmi_version) << '\n'
           << "SOURCE_HASH " << std::quoted(model.source_hash) << '\n'
           << "GENERATION_TOOL " << std::quoted(model.generation_tool) << '\n'
           << "GENERATION_TIME " << std::quoted(model.generation_date_time) << '\n'
           << "HOST_PLATFORM " << std::quoted(model.host_platform) << '\n'
           << "HOST_BINARY_CANDIDATE " << model.host_binary_candidate_available << '\n'
           << "DEFAULT_EXPERIMENT " << model.default_experiment.size();
    for (const auto& [name, value] : model.default_experiment) {
        output << ' ' << std::quoted(name) << ' ' << std::quoted(value);
    }
    output << '\n' << "INTERFACES " << model.interfaces.size() << '\n';
    for (const auto& interface : model.interfaces) {
        output << "INTERFACE " << std::quoted(interface.kind) << ' '
               << std::quoted(interface.model_identifier) << '\n';
        for (const auto& [name, value] : interface.capabilities) {
            output << "CAPABILITY " << std::quoted(interface.kind) << ' '
                   << std::quoted(name) << ' ' << std::quoted(value) << '\n';
        }
    }
    output << "VARIABLES " << model.variables.size() << '\n';
    std::map<std::string, std::size_t> causalities;
    for (const auto& variable : model.variables) {
        ++causalities[variable.causality];
        output << "VARIABLE " << std::quoted(variable.name) << ' '
               << std::quoted(variable.type) << ' ' << variable.value_reference << ' '
               << std::quoted(variable.causality) << ' '
               << std::quoted(variable.variability) << ' '
               << std::quoted(variable.initial) << ' ' << std::quoted(variable.unit) << ' '
               << std::quoted(variable.start) << ' ' << variable.dimensions << ' '
               << variable.derivative_of << '\n';
    }
    output << "DERIVATIVE_ORDER " << model.derivative_variable_order.size();
    for (const auto index : model.derivative_variable_order) output << ' ' << index;
    output << '\n';
    for (const auto& [causality, count] : causalities) {
        output << "CAUSALITY " << std::quoted(causality) << ' ' << count << '\n';
    }
    output << "BINARY_PLATFORMS " << model.binary_platforms.size();
    for (const auto& platform : model.binary_platforms) {
        output << ' ' << std::quoted(platform);
    }
    output << '\n' << "EVENT_INDICATORS " << model.number_of_event_indicators << '\n'
           << "TRAJECTORY_PROXY_ALLOWED " << model.trajectory_proxy_allowed << '\n'
           << "DIFFERENTIAL_TEST_ALLOWED " << model.differential_test_allowed << '\n'
           << "EQUATION_LEVEL_VALIDATION_ALLOWED "
           << model.equation_level_validation_allowed << '\n'
           << "DIRECT_EXPERT_ALLOWED " << model.direct_expert_allowed << '\n'
           << "WARNINGS " << model.warnings.size() << '\n';
    for (const auto& warning : model.warnings) {
        output << "WARNING " << std::quoted(warning) << '\n';
    }
    output << "END\n";
}

void write_fmi_smoke_report(
    const FmiBlackboxIR& model,
    const FmiSmokeResult& result,
    const std::filesystem::path& path) {
    model.validate();
    if (result.model_name != model.model_name || result.source_hash != model.source_hash) {
        throw std::invalid_argument("FMI smoke result targets a different imported model");
    }
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write FMI smoke report: " + path.string());
    const bool has_typed_outputs = std::any_of(
        result.samples.begin(), result.samples.end(), [](const auto& sample) {
            return !sample.array_outputs.empty() || !sample.string_outputs.empty() ||
                !sample.string_array_outputs.empty() || !sample.binary_outputs.empty() ||
                !sample.binary_array_outputs.empty();
        });
    output << "SMAVE_FMI_SMOKE_REPORT " << (has_typed_outputs ? 2 : 1) << '\n'
           << "MODEL " << std::quoted(result.model_name) << '\n'
           << "SOURCE_HASH " << std::quoted(result.source_hash) << '\n'
           << "INTERFACE " << std::quoted(result.interface_kind) << ' '
           << std::quoted(result.model_identifier) << '\n'
           << "SUCCESS " << result.success << '\n'
           << "START_TIME " << std::setprecision(17) << result.start_time << '\n'
           << "END_TIME " << result.end_time << '\n'
           << "STEP_SIZE " << result.step_size << '\n'
           << "SAMPLES " << result.samples.size() << '\n';
    for (const auto& sample : result.samples) {
        output << "SAMPLE " << std::setprecision(17) << sample.time << ' '
               << sample.outputs.size();
        for (const auto& [name, value] : sample.outputs) {
            output << ' ' << std::quoted(name) << ' ' << value;
        }
        output << '\n';
        if (has_typed_outputs) {
            output << "ARRAY_SAMPLE " << std::setprecision(17) << sample.time << ' '
                   << sample.array_outputs.size();
            for (const auto& [name, values] : sample.array_outputs) {
                output << ' ' << std::quoted(name) << ' ' << values.size();
                for (const auto value : values) output << ' ' << value;
            }
            output << '\n';
            output << "STRING_ARRAY_SAMPLE " << std::setprecision(17) << sample.time << ' '
                   << sample.string_array_outputs.size();
            for (const auto& [name, values] : sample.string_array_outputs) {
                output << ' ' << std::quoted(name) << ' ' << values.size();
                for (const auto& value : values) output << ' ' << std::quoted(value);
            }
            output << '\n';
            output << "STRING_SAMPLE " << std::setprecision(17) << sample.time << ' '
                   << sample.string_outputs.size();
            for (const auto& [name, value] : sample.string_outputs) {
                output << ' ' << std::quoted(name) << ' ' << std::quoted(value);
            }
            output << '\n';
            output << "BINARY_SAMPLE " << std::setprecision(17) << sample.time << ' '
                   << sample.binary_outputs.size();
            for (const auto& [name, value] : sample.binary_outputs) {
                output << ' ' << std::quoted(name) << ' ' << std::quoted(hexadecimal(value));
            }
            output << '\n';
            output << "BINARY_ARRAY_SAMPLE " << std::setprecision(17) << sample.time << ' '
                   << sample.binary_array_outputs.size();
            for (const auto& [name, values] : sample.binary_array_outputs) {
                output << ' ' << std::quoted(name) << ' ' << values.size();
                for (const auto& value : values) {
                    output << ' ' << std::quoted(hexadecimal(value));
                }
            }
            output << '\n';
        }
    }
    output << "STATE_ROUNDTRIP_ATTEMPTED " << result.state_roundtrip_attempted << '\n'
           << "STATE_ROUNDTRIP_PASSED " << result.state_roundtrip_passed << '\n'
           << "MAX_STATE_REPLAY_ERROR " << result.maximum_state_replay_error << '\n'
           << "STATE_SERIALIZATION_ATTEMPTED "
           << result.state_serialization_attempted << '\n'
           << "STATE_SERIALIZATION_PASSED "
           << result.state_serialization_passed << '\n'
           << "SERIALIZED_STATE_BYTES " << result.serialized_state_bytes << '\n'
           << "DO_STEP_CALLS " << result.do_step_calls << '\n'
           << "PENDING_STEPS " << result.pending_steps << '\n'
           << "STEP_FINISHED_CALLBACKS "
           << result.step_finished_callbacks << '\n'
           << "CROSS_THREAD_CALLBACKS "
           << result.cross_thread_callbacks << '\n'
           << "CANCELLED_STEPS " << result.cancelled_steps << '\n'
           << "ASYNCHRONOUS_TIMEOUT_MS "
           << result.asynchronous_timeout_ms << '\n'
           << "EARLY_RETURNS " << result.early_returns << '\n'
           << "DISCARD_RECOVERIES "
           << result.discard_recoveries << '\n'
           << "TIME_EVENT_SPLITS " << result.time_event_splits << '\n'
           << "TIME_EVENTS " << result.time_events << '\n'
           << "MODEL_EXCHANGE_ROOTS " << result.model_exchange_roots << '\n'
           << "MODEL_EXCHANGE_GRAZING_ROOTS "
           << result.model_exchange_grazing_roots << '\n'
           << "CONTINUOUS_STATE_NOMINAL_UPDATES "
           << result.continuous_state_nominal_updates << '\n'
           << "MIN_CONTINUOUS_STATE_NOMINAL "
           << result.minimum_continuous_state_nominal << '\n'
           << "MAX_CONTINUOUS_STATE_NOMINAL "
           << result.maximum_continuous_state_nominal << '\n'
           << "EVENT_MODE_ENTRIES " << result.event_mode_entries << '\n'
           << "DISCRETE_UPDATE_ITERATIONS " << result.discrete_update_iterations << '\n'
           << "MODEL_PARTITION_ACTIVATIONS "
           << result.model_partition_activations << '\n'
           << "PARTITION_ACTIVATION_ORDER "
           << result.partition_activation_order.size();
    for (const auto& activation : result.partition_activation_order) {
        output << ' ' << activation.time << ' ' << std::quoted(activation.clock_name)
               << ' ' << activation.clock_value_reference << ' '
               << activation.clock_priority;
    }
    output << '\n'
           << "CLOCK_UPDATE_CALLBACKS " << result.clock_update_callbacks << '\n'
           << "LOCK_PREEMPTION_CALLBACKS "
           << result.lock_preemption_callbacks << '\n'
           << "UNLOCK_PREEMPTION_CALLBACKS "
           << result.unlock_preemption_callbacks << '\n'
           << "CLOCK_INTERVALS " << result.clock_intervals.size();
    for (const auto& [name, interval] : result.clock_intervals) {
        output << ' ' << std::quoted(name) << ' ' << interval;
    }
    output << '\n' << "CLOCK_SHIFTS " << result.clock_shifts.size();
    for (const auto& [name, shift] : result.clock_shifts) {
        output << ' ' << std::quoted(name) << ' ' << shift;
    }
    output << '\n' << "CLOCK_PRIORITIES " << result.clock_priorities.size();
    for (const auto& [name, priority] : result.clock_priorities) {
        output << ' ' << std::quoted(name) << ' ' << priority;
    }
    output << '\n' << "CLOCK_INTERVAL_QUALIFIERS "
           << result.clock_interval_qualifiers.size();
    for (const auto& [name, qualifier] : result.clock_interval_qualifiers) {
        output << ' ' << std::quoted(name) << ' ' << std::quoted(qualifier);
    }
    output << '\n'
           << "WARNINGS " << result.warnings << '\n'
           << "EQUATION_LEVEL_VALIDATION_ALLOWED "
           << model.equation_level_validation_allowed << '\n'
           << "DIRECT_EXPERT_ALLOWED " << model.direct_expert_allowed << '\n'
           << "MESSAGE " << std::quoted(result.message) << "\nEND\n";
}

void write_ssp_report(
    const SspSimulationResult& result,
    const std::filesystem::path& path) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write SSP report");
    output << std::setprecision(17)
           << "SMAVE_SSP_REPORT 5\n"
           << "SYSTEM " << std::quoted(result.system_name) << '\n'
           << "SOURCE_HASH " << result.source_hash << '\n'
           << "SUCCESS " << result.success << '\n'
           << "END_TIME " << result.end_time << '\n'
           << "STEP_SIZE " << result.step_size << '\n'
           << "COMMUNICATION_STEPS " << result.communication_steps << '\n'
           << "SIGNAL_EXCHANGES " << result.signal_exchanges << '\n'
           << "EVENT_MODE_ENTRIES " << result.event_mode_entries << '\n'
           << "DISCRETE_UPDATE_ITERATIONS "
           << result.discrete_update_iterations << '\n'
           << "TIME_EVENT_SPLITS " << result.time_event_splits << '\n'
           << "TIME_EVENTS " << result.time_events << '\n'
           << "COMPONENTS " << result.components.size() << '\n';
    for (const auto& component : result.components) {
        output << "COMPONENT " << std::quoted(component.name) << ' '
               << std::quoted(component.source) << ' '
               << std::quoted(component.fmi_version) << ' '
               << std::quoted(component.model_name) << ' '
               << component.source_hash << ' '
               << component.event_mode_entries << ' '
               << component.discrete_update_iterations << ' '
               << component.time_events << '\n';
    }
    output << "CONNECTIONS " << result.connections.size() << '\n';
    for (const auto& connection : result.connections) {
        output << "CONNECTION " << std::quoted(connection.source_component) << ' '
               << std::quoted(connection.source_connector) << ' '
               << std::quoted(connection.target_component) << ' '
               << std::quoted(connection.target_connector) << ' '
               << std::quoted(connection.source_unit) << ' '
               << std::quoted(connection.target_unit) << ' '
               << connection.unit_factor << ' ' << connection.unit_offset << ' '
               << connection.factor << ' ' << connection.offset << '\n';
    }
    output << "STEP_ORDER " << result.step_order.size();
    for (const auto& component : result.step_order) {
        output << ' ' << std::quoted(component);
    }
    output << '\n' << "SAMPLES " << result.samples.size() << '\n';
    for (const auto& sample : result.samples) {
        output << "SAMPLE " << sample.time << ' ' << sample.outputs.size();
        for (const auto& [name, value] : sample.outputs) {
            output << ' ' << std::quoted(name) << ' ' << value;
        }
        output << '\n';
    }
    output << "MESSAGE " << std::quoted(result.message) << "\nEND\n";
}

}  // namespace smave
