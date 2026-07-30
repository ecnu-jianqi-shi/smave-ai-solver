#include "smave/block_graph.hpp"

#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace smave {
namespace {

std::string digest(std::string_view input) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : input) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

constexpr std::size_t kMaximumSlxArchiveSize = 128U * 1024U * 1024U;
constexpr std::size_t kMaximumSlxDiagramSize = 16U * 1024U * 1024U;

std::uint16_t little_u16(std::string_view bytes, std::size_t offset) {
    if (offset + 2 > bytes.size()) throw std::runtime_error("truncated SLX ZIP field");
    return static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[offset])) |
        static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8U;
}

std::uint32_t little_u32(std::string_view bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) throw std::runtime_error("truncated SLX ZIP field");
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset])) |
        static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8U |
        static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 2])) << 16U |
        static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 3])) << 24U;
}

bool safe_archive_path(const std::string& path) {
    if (path.empty() || path.front() == '/' || path.front() == '\\') return false;
    if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) &&
        path[1] == ':') return false;
    if (path.find('\\') != std::string::npos) return false;
    for (const auto& part : std::filesystem::path(path)) {
        if (part == "..") return false;
    }
    return true;
}

struct SlxZipEntry {
    std::string name;
    std::uint16_t flags{};
    std::uint16_t method{};
    std::uint32_t crc{};
    std::uint32_t compressed_size{};
    std::uint32_t uncompressed_size{};
    std::uint32_t local_offset{};
};

std::vector<SlxZipEntry> slx_zip_entries(const std::string& archive) {
    constexpr std::uint32_t end_signature = 0x06054b50U;
    constexpr std::uint32_t central_signature = 0x02014b50U;
    const std::size_t search_start = archive.size() > 65557U
        ? archive.size() - 65557U : 0U;
    std::size_t end_offset = std::string::npos;
    for (std::size_t offset = archive.size() >= 22U ? archive.size() - 22U : 0U;; --offset) {
        if (little_u32(archive, offset) == end_signature) {
            end_offset = offset;
            break;
        }
        if (offset == search_start) break;
    }
    if (end_offset == std::string::npos) throw std::invalid_argument("SLX is not a ZIP archive");
    const auto disk = little_u16(archive, end_offset + 4);
    const auto central_disk = little_u16(archive, end_offset + 6);
    const auto disk_entries = little_u16(archive, end_offset + 8);
    const auto total_entries = little_u16(archive, end_offset + 10);
    const auto central_size = little_u32(archive, end_offset + 12);
    const auto central_offset = little_u32(archive, end_offset + 16);
    if (disk != 0 || central_disk != 0 || disk_entries != total_entries) {
        throw std::invalid_argument("multi-disk SLX ZIP archives are unsupported");
    }
    if (total_entries == 0xffffU || central_size == 0xffffffffU ||
        central_offset == 0xffffffffU) {
        throw std::invalid_argument("ZIP64 SLX archives are unsupported");
    }
    if (static_cast<std::size_t>(central_offset) + central_size > archive.size()) {
        throw std::invalid_argument("invalid SLX ZIP central directory");
    }
    std::vector<SlxZipEntry> entries;
    std::unordered_set<std::string> names;
    std::size_t offset = central_offset;
    for (std::size_t index = 0; index < total_entries; ++index) {
        if (little_u32(archive, offset) != central_signature) {
            throw std::invalid_argument("invalid SLX ZIP central entry");
        }
        SlxZipEntry entry;
        entry.flags = little_u16(archive, offset + 8);
        entry.method = little_u16(archive, offset + 10);
        entry.crc = little_u32(archive, offset + 16);
        entry.compressed_size = little_u32(archive, offset + 20);
        entry.uncompressed_size = little_u32(archive, offset + 24);
        const auto name_size = little_u16(archive, offset + 28);
        const auto extra_size = little_u16(archive, offset + 30);
        const auto comment_size = little_u16(archive, offset + 32);
        entry.local_offset = little_u32(archive, offset + 42);
        const std::size_t next = offset + 46U + name_size + extra_size + comment_size;
        if (next > archive.size()) throw std::runtime_error("truncated SLX ZIP entry");
        entry.name = archive.substr(offset + 46U, name_size);
        if (!safe_archive_path(entry.name)) {
            throw std::invalid_argument("unsafe SLX ZIP entry path: " + entry.name);
        }
        if (!names.insert(entry.name).second) {
            throw std::invalid_argument("duplicate SLX ZIP entry: " + entry.name);
        }
        if ((entry.flags & 1U) != 0U) {
            throw std::invalid_argument("encrypted SLX ZIP entries are unsupported");
        }
        if (entry.method != 0U && entry.method != 8U) {
            throw std::invalid_argument("unsupported SLX ZIP compression method");
        }
        entries.push_back(std::move(entry));
        offset = next;
    }
    return entries;
}

std::string extract_slx_entry(const std::string& archive, const SlxZipEntry& entry) {
    constexpr std::uint32_t local_signature = 0x04034b50U;
    if (little_u32(archive, entry.local_offset) != local_signature) {
        throw std::invalid_argument("invalid SLX ZIP local entry");
    }
    const auto flags = little_u16(archive, entry.local_offset + 6);
    const auto method = little_u16(archive, entry.local_offset + 8);
    const auto name_size = little_u16(archive, entry.local_offset + 26);
    const auto extra_size = little_u16(archive, entry.local_offset + 28);
    const std::size_t data_offset = entry.local_offset + 30U + name_size + extra_size;
    if (flags != entry.flags || method != entry.method ||
        data_offset + entry.compressed_size > archive.size()) {
        throw std::invalid_argument("SLX ZIP central/local entry mismatch");
    }
    const auto local_name = archive.substr(entry.local_offset + 30U, name_size);
    if (local_name != entry.name) {
        throw std::invalid_argument("SLX ZIP central/local name mismatch");
    }
    if (entry.uncompressed_size > kMaximumSlxDiagramSize) {
        throw std::runtime_error("SLX block diagram exceeds extraction limit");
    }
    std::string output(entry.uncompressed_size, '\0');
    if (entry.method == 0U) {
        if (entry.compressed_size != entry.uncompressed_size) {
            throw std::invalid_argument("invalid stored SLX ZIP entry size");
        }
        output.assign(archive.data() + data_offset, entry.uncompressed_size);
    } else {
        z_stream stream{};
        stream.next_in = reinterpret_cast<Bytef*>(
            const_cast<char*>(archive.data() + data_offset));
        stream.avail_in = entry.compressed_size;
        stream.next_out = reinterpret_cast<Bytef*>(output.data());
        stream.avail_out = entry.uncompressed_size;
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            throw std::runtime_error("cannot initialize SLX ZIP inflater");
        }
        const int status = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);
        if (status != Z_STREAM_END || stream.total_out != entry.uncompressed_size) {
            throw std::invalid_argument("invalid deflated SLX ZIP entry");
        }
    }
    const auto actual_crc = crc32(
        0L, reinterpret_cast<const Bytef*>(output.data()), output.size());
    if (actual_crc != entry.crc) throw std::invalid_argument("SLX ZIP entry CRC mismatch");
    return output;
}

std::string decode_xml_entities(std::string value) {
    const std::pair<std::string_view, std::string_view> entities[]{
        {"&quot;", "\""}, {"&apos;", "'"}, {"&lt;", "<"},
        {"&gt;", ">"}, {"&amp;", "&"}};
    for (const auto& [encoded, decoded] : entities) {
        std::size_t position{};
        while ((position = value.find(encoded, position)) != std::string::npos) {
            value.replace(position, encoded.size(), decoded);
            position += decoded.size();
        }
    }
    return value;
}

struct SlxXmlEvent {
    enum class Kind { start, end, text } kind{Kind::text};
    std::string name;
    std::map<std::string, std::string> attributes;
    std::string text;
    bool self_closing{};
};

std::vector<SlxXmlEvent> slx_xml_events(const std::string& xml) {
    std::vector<SlxXmlEvent> events;
    std::size_t position{};
    while (position < xml.size()) {
        const auto opening = xml.find('<', position);
        if (opening == std::string::npos) {
            if (position < xml.size()) events.push_back({SlxXmlEvent::Kind::text, {}, {}, xml.substr(position)});
            break;
        }
        if (opening > position) {
            events.push_back({SlxXmlEvent::Kind::text, {}, {}, xml.substr(position, opening - position)});
        }
        if (xml.compare(opening, 4, "<!--") == 0) {
            const auto end = xml.find("-->", opening + 4);
            if (end == std::string::npos) throw std::invalid_argument("unterminated SLX XML comment");
            position = end + 3;
            continue;
        }
        const auto closing = xml.find('>', opening + 1);
        if (closing == std::string::npos) throw std::invalid_argument("unterminated SLX XML tag");
        std::string body = xml.substr(opening + 1, closing - opening - 1);
        if (!body.empty() && (body.front() == '?' || body.front() == '!')) {
            position = closing + 1;
            continue;
        }
        SlxXmlEvent event;
        if (!body.empty() && body.front() == '/') {
            event.kind = SlxXmlEvent::Kind::end;
            body.erase(body.begin());
        } else {
            event.kind = SlxXmlEvent::Kind::start;
            if (!body.empty() && body.back() == '/') {
                event.self_closing = true;
                body.pop_back();
            }
        }
        std::size_t cursor{};
        while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor]))) ++cursor;
        const auto name_begin = cursor;
        while (cursor < body.size() && !std::isspace(static_cast<unsigned char>(body[cursor]))) ++cursor;
        event.name = body.substr(name_begin, cursor - name_begin);
        while (event.kind == SlxXmlEvent::Kind::start && cursor < body.size()) {
            while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor]))) ++cursor;
            if (cursor == body.size()) break;
            const auto key_begin = cursor;
            while (cursor < body.size() && body[cursor] != '=' &&
                   !std::isspace(static_cast<unsigned char>(body[cursor]))) ++cursor;
            const auto key = body.substr(key_begin, cursor - key_begin);
            while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor]))) ++cursor;
            if (cursor == body.size() || body[cursor++] != '=') {
                throw std::invalid_argument("malformed SLX XML attribute");
            }
            while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor]))) ++cursor;
            if (cursor == body.size() || (body[cursor] != '\'' && body[cursor] != '\"')) {
                throw std::invalid_argument("SLX XML attribute must be quoted");
            }
            const char quote = body[cursor++];
            const auto value_end = body.find(quote, cursor);
            if (value_end == std::string::npos) throw std::invalid_argument("unterminated SLX XML attribute");
            event.attributes.emplace(
                key, decode_xml_entities(body.substr(cursor, value_end - cursor)));
            cursor = value_end + 1;
        }
        if (!event.name.empty()) events.push_back(std::move(event));
        position = closing + 1;
    }
    return events;
}

std::string slx_attribute(
    const std::map<std::string, std::string>& attributes,
    const std::string& name,
    const std::string& fallback = {}) {
    const auto iterator = attributes.find(name);
    return iterator == attributes.end() ? fallback : iterator->second;
}

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character);
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character);
    }).base();
    return first < last ? std::string(first, last) : std::string{};
}

double slx_number(const std::string& value, const std::string& purpose) {
    std::size_t consumed{};
    const double result = std::stod(trim(value), &consumed);
    const auto normalized = trim(value);
    if (consumed != normalized.size() || !std::isfinite(result)) {
        throw std::invalid_argument("invalid SLX " + purpose);
    }
    return result;
}

std::pair<double, double> slx_sample_time(const std::string& value) {
    const auto normalized = trim(value);
    if (normalized.empty()) return {0.0, 0.0};
    if (normalized == "-1") {
        throw std::invalid_argument("inherited SLX sample time is unsupported");
    }
    if (normalized.front() != '[') return {slx_number(normalized, "sample time"), 0.0};
    if (normalized.back() != ']') throw std::invalid_argument("invalid SLX sample time vector");
    std::string body = normalized.substr(1, normalized.size() - 2);
    std::replace(body.begin(), body.end(), ',', ' ');
    std::istringstream input(body);
    double period{};
    double offset{};
    std::string extra;
    if (!(input >> period >> offset) || input >> extra || !std::isfinite(period) ||
        !std::isfinite(offset)) {
        throw std::invalid_argument("invalid SLX sample time vector");
    }
    return {period, offset};
}

std::pair<std::string, std::size_t> slx_endpoint(const std::string& endpoint) {
    const auto marker = endpoint.find('#');
    const auto colon = endpoint.rfind(':');
    if (marker == std::string::npos || colon == std::string::npos || marker >= colon) {
        throw std::invalid_argument("invalid SLX line endpoint: " + endpoint);
    }
    const auto sid = endpoint.substr(0, marker);
    std::size_t consumed{};
    const auto port = std::stoull(endpoint.substr(colon + 1), &consumed);
    if (sid.empty() || consumed != endpoint.size() - colon - 1 || port == 0) {
        throw std::invalid_argument("invalid SLX line endpoint: " + endpoint);
    }
    return {sid, port};
}

void require_tag(std::istream& input, std::string_view expected) {
    std::string actual;
    input >> actual;
    if (!input || actual != expected) {
        throw std::runtime_error(
            "invalid block graph: expected " + std::string(expected) + ", got " + actual);
    }
}

bool supported_type(const std::string& type) {
    static const std::set<std::string> types{
        "constant", "gain", "sum", "algebraic_model", "unit_delay", "switch"};
    return types.contains(type);
}

std::vector<std::string> schedule(const BlockGraphIR& graph) {
    std::unordered_map<std::string, std::size_t> indegree;
    std::unordered_map<std::string, std::vector<std::string>> successors;
    std::unordered_map<std::string, std::size_t> source_order;
    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        indegree[graph.nodes[index].id] = 0;
        source_order[graph.nodes[index].id] = index;
    }
    for (const auto& connection : graph.connections) {
        const auto target = std::find_if(
            graph.nodes.begin(), graph.nodes.end(), [&](const BlockGraphNode& node) {
                return node.id == connection.target_node;
            });
        if (target->type == "unit_delay") continue;
        successors[connection.source_node].push_back(connection.target_node);
        ++indegree[connection.target_node];
    }
    auto compare = [&](const std::string& left, const std::string& right) {
        return source_order.at(left) > source_order.at(right);
    };
    std::priority_queue<std::string, std::vector<std::string>, decltype(compare)> ready(compare);
    for (const auto& node : graph.nodes) {
        if (indegree.at(node.id) == 0) ready.push(node.id);
    }
    std::vector<std::string> result;
    while (!ready.empty()) {
        auto node = ready.top();
        ready.pop();
        result.push_back(node);
        for (const auto& successor : successors[node]) {
            if (--indegree[successor] == 0) ready.push(successor);
        }
    }
    if (result.size() != graph.nodes.size()) {
        throw std::invalid_argument(
            "block graph contains an algebraic cycle without a unit_delay boundary");
    }
    return result;
}

BlockGraphIR import_slx_block_graph(const std::filesystem::path& path) {
    const auto archive_size = std::filesystem::file_size(path);
    if (archive_size > kMaximumSlxArchiveSize) {
        throw std::runtime_error("SLX archive exceeds import size limit");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read SLX archive: " + path.string());
    std::ostringstream raw;
    raw << input.rdbuf();
    const auto archive = raw.str();
    const auto entries = slx_zip_entries(archive);
    const auto diagram_entry = std::find_if(
        entries.begin(), entries.end(), [](const SlxZipEntry& entry) {
            return entry.name == "simulink/blockdiagram.xml";
        });
    if (diagram_entry == entries.end()) {
        throw std::invalid_argument("SLX is missing simulink/blockdiagram.xml");
    }
    const auto xml = extract_slx_entry(archive, *diagram_entry);
    const auto events = slx_xml_events(xml);
    BlockGraphIR graph;
    graph.source_hash = digest(archive);
    struct ParsedBlock {
        std::string sid;
        std::string name;
        std::string block_type;
        std::map<std::string, std::string> parameters;
    };
    struct ParsedLine {
        std::map<std::string, std::string> parameters;
        std::vector<std::map<std::string, std::string>> branch_parameters;
    };
    struct ParsedScope {
        std::string parent_sid;
        std::size_t parent_scope{};
        std::vector<ParsedBlock> blocks;
        std::vector<ParsedLine> lines;
    };
    std::vector<ParsedScope> scopes(1);
    std::vector<std::size_t> scope_stack;
    std::optional<std::pair<std::size_t, std::size_t>> current_block;
    std::optional<std::pair<std::size_t, std::size_t>> current_line;
    std::vector<std::pair<std::size_t, std::size_t>> parent_blocks;
    const auto block_at = [&]() -> ParsedBlock* {
        return current_block
            ? &scopes[current_block->first].blocks[current_block->second] : nullptr;
    };
    const auto line_at = [&]() -> ParsedLine* {
        return current_line
            ? &scopes[current_line->first].lines[current_line->second] : nullptr;
    };
    std::vector<std::size_t> branch_stack;
    std::string parameter_name;
    std::string parameter_text;
    std::size_t system_depth{};
    bool saw_system{};
    for (const auto& event : events) {
        if (event.kind == SlxXmlEvent::Kind::text) {
            if (!parameter_name.empty()) parameter_text += event.text;
            continue;
        }
        if (event.kind == SlxXmlEvent::Kind::end) {
            if (event.name == "P" && !parameter_name.empty()) {
                auto* block = block_at();
                auto* line = line_at();
                auto* parameters = block != nullptr
                    ? &block->parameters
                    : line != nullptr && !branch_stack.empty()
                        ? &line->branch_parameters[branch_stack.back()]
                        : line != nullptr ? &line->parameters : nullptr;
                if (parameters == nullptr ||
                    !parameters->emplace(
                        parameter_name,
                        decode_xml_entities(trim(parameter_text))).second) {
                    throw std::invalid_argument("duplicate or detached SLX parameter");
                }
                parameter_name.clear();
                parameter_text.clear();
            } else if (event.name == "Block") {
                current_block.reset();
            } else if (event.name == "Line") {
                if (!branch_stack.empty()) {
                    throw std::invalid_argument("SLX Line closed inside Branch");
                }
                current_line.reset();
            } else if (event.name == "Branch") {
                if (!current_line || branch_stack.empty()) {
                    throw std::invalid_argument("unbalanced SLX Branch tag");
                }
                branch_stack.pop_back();
            } else if (event.name == "System") {
                if (system_depth == 0) throw std::invalid_argument("unbalanced SLX System tag");
                if (scope_stack.empty()) throw std::invalid_argument("unbalanced SLX System scope");
                scope_stack.pop_back();
                --system_depth;
                if (!parent_blocks.empty()) {
                    current_block = parent_blocks.back();
                    parent_blocks.pop_back();
                }
            }
            continue;
        }
        if (event.name == "Model") {
            if (graph.model_id.empty()) graph.model_id = slx_attribute(event.attributes, "Name");
        } else if (event.name == "System") {
            ++system_depth;
            saw_system = true;
            if (system_depth > 64) {
                throw std::invalid_argument("SLX SubSystem nesting exceeds safety limit");
            }
            if (system_depth == 1) {
                if (!scope_stack.empty() || current_block || current_line) {
                    throw std::invalid_argument("multiple top-level SLX Systems are unsupported");
                }
                scope_stack.push_back(0);
            } else {
                auto* parent = block_at();
                if (parent == nullptr || current_line ||
                    parent->block_type != "SubSystem") {
                    throw std::invalid_argument(
                        "nested SLX System requires a SubSystem block");
                }
                if (std::any_of(
                        scopes.begin() + 1, scopes.end(), [&](const auto& scope) {
                            return scope.parent_scope == scope_stack.back() &&
                                scope.parent_sid == parent->sid;
                        })) {
                    throw std::invalid_argument(
                        parent->name + ": SubSystem must contain exactly one System");
                }
                parent_blocks.push_back(*current_block);
                scopes.push_back({parent->sid, scope_stack.back(), {}, {}});
                scope_stack.push_back(scopes.size() - 1);
                current_block.reset();
            }
        } else if (event.name == "Branch") {
            auto* line = line_at();
            if (line == nullptr || current_block) {
                throw std::invalid_argument("SLX Branch must be nested inside Line");
            }
            line->branch_parameters.emplace_back();
            branch_stack.push_back(line->branch_parameters.size() - 1);
        } else if (event.name == "Block") {
            if (scope_stack.empty() || current_block || current_line) {
                throw std::invalid_argument("malformed or nested SLX Block");
            }
            auto& scope_blocks = scopes[scope_stack.back()].blocks;
            scope_blocks.push_back({
                slx_attribute(event.attributes, "SID"),
                slx_attribute(event.attributes, "Name"),
                slx_attribute(event.attributes, "BlockType"), {}});
            current_block = std::pair{scope_stack.back(), scope_blocks.size() - 1};
            auto* block = block_at();
            if (block->sid.empty() || block->name.empty() || block->block_type.empty()) {
                throw std::invalid_argument("SLX Block requires SID, Name, and BlockType");
            }
        } else if (event.name == "Line") {
            if (scope_stack.empty() || current_block || current_line) {
                throw std::invalid_argument("malformed or nested SLX Line");
            }
            auto& scope_lines = scopes[scope_stack.back()].lines;
            scope_lines.push_back({{}, {}});
            current_line = std::pair{scope_stack.back(), scope_lines.size() - 1};
        } else if (event.name == "P") {
            if (current_block.has_value() == current_line.has_value() ||
                !parameter_name.empty()) {
                throw std::invalid_argument("detached or nested SLX P element");
            }
            parameter_name = slx_attribute(event.attributes, "Name");
            if (parameter_name.empty()) throw std::invalid_argument("SLX P requires Name");
            parameter_text.clear();
        }
        if (event.self_closing) {
            if (event.name == "Block") current_block.reset();
            if (event.name == "Line") current_line.reset();
            if (event.name == "Branch") branch_stack.pop_back();
            if (event.name == "System") {
                scope_stack.pop_back();
                --system_depth;
            }
        }
    }
    if (!saw_system || system_depth != 0 || current_block || current_line ||
        !scope_stack.empty() || !parent_blocks.empty() || !branch_stack.empty() ||
        !parameter_name.empty()) {
        throw std::invalid_argument("incomplete SLX block diagram XML");
    }
    if (graph.model_id.empty()) graph.model_id = path.stem().string();
    const auto line_targets = [](const ParsedLine& line) {
        std::vector<std::string> targets;
        if (const auto target = line.parameters.find("Dst");
            target != line.parameters.end()) {
            targets.push_back(target->second);
        }
        for (const auto& branch : line.branch_parameters) {
            if (branch.contains("Src")) {
                throw std::invalid_argument("SLX Branch cannot override its Line Src");
            }
            if (const auto target = branch.find("Dst"); target != branch.end()) {
                targets.push_back(target->second);
            }
        }
        if (targets.empty()) throw std::invalid_argument("SLX Line has no Dst or Branch Dst");
        return targets;
    };
    struct FlattenedScope {
        std::vector<ParsedBlock> blocks;
        std::vector<ParsedLine> lines;
        std::map<std::size_t, std::vector<std::string>> inputs;
        std::map<std::size_t, std::string> outputs;
    };
    std::function<FlattenedScope(std::size_t, const std::string&,
                                 const std::optional<std::pair<double, double>>&)> flatten_scope;
    flatten_scope = [&](std::size_t scope_index, const std::string& prefix,
                        const std::optional<std::pair<double, double>>& expected_rate) {
        FlattenedScope result;
        std::unordered_map<std::string, std::pair<std::string, std::size_t>> connectors;
        struct TagBlock {
            std::string type;
            std::string tag;
        };
        std::unordered_map<std::string, TagBlock> tag_blocks;
        std::unordered_map<std::string, std::string> goto_by_tag;
        std::unordered_map<std::string, FlattenedScope> child_interfaces;
        for (const auto& original : scopes[scope_index].blocks) {
            if (original.block_type == "Inport" || original.block_type == "Outport") {
                if (scope_index == 0) {
                    throw std::invalid_argument("top-level SLX Inport/Outport is unsupported");
                }
                const auto port = original.parameters.find("Port");
                if (port == original.parameters.end()) {
                    throw std::invalid_argument(original.name + ": connector requires Port");
                }
                std::size_t consumed{};
                const auto parsed_number = std::stoull(port->second, &consumed);
                if (consumed != port->second.size() || parsed_number == 0 ||
                    parsed_number > std::numeric_limits<std::size_t>::max()) {
                    throw std::invalid_argument(original.name + ": invalid connector Port");
                }
                const auto number = static_cast<std::size_t>(parsed_number);
                for (const auto& [_, connector] : connectors) {
                    if (connector == std::pair{original.block_type, number}) {
                        throw std::invalid_argument(
                            original.name + ": duplicate SubSystem connector Port");
                    }
                }
                connectors.emplace(original.sid, std::pair{original.block_type, number});
                continue;
            }
            if (original.block_type == "Goto" || original.block_type == "From") {
                const auto tag = original.parameters.find("GotoTag");
                if (tag == original.parameters.end() || trim(tag->second).empty()) {
                    throw std::invalid_argument(
                        prefix + original.name + ": Goto/From requires non-empty GotoTag");
                }
                if (original.block_type == "Goto") {
                    const auto visibility = original.parameters.find("TagVisibility");
                    if (visibility != original.parameters.end() &&
                        visibility->second != "local") {
                        throw std::invalid_argument(
                            prefix + original.name +
                            ": only local Goto TagVisibility is supported");
                    }
                    if (!goto_by_tag.emplace(tag->second, original.sid).second) {
                        throw std::invalid_argument(
                            prefix + original.name + ": duplicate local Goto tag");
                    }
                }
                tag_blocks.emplace(
                    original.sid, TagBlock{original.block_type, tag->second});
                continue;
            }
            const auto sample = original.parameters.find("SampleTime");
            if (sample == original.parameters.end()) {
                throw std::invalid_argument(original.name + ": explicit SampleTime is required");
            }
            const auto rate = slx_sample_time(sample->second);
            if (expected_rate && rate != *expected_rate) {
                throw std::invalid_argument(
                    prefix + original.name + ": flattened child SampleTime must match SubSystem");
            }
            if (original.block_type == "SubSystem") {
                std::vector<std::size_t> children;
                for (std::size_t index = 1; index < scopes.size(); ++index) {
                    if (scopes[index].parent_scope == scope_index &&
                        scopes[index].parent_sid == original.sid) children.push_back(index);
                }
                if (children.size() != 1) {
                    throw std::invalid_argument(
                        prefix + original.name + ": SubSystem must contain exactly one System");
                }
                auto child = flatten_scope(
                    children.front(), prefix + original.name + "/", rate);
                result.blocks.insert(
                    result.blocks.end(), child.blocks.begin(), child.blocks.end());
                result.lines.insert(result.lines.end(), child.lines.begin(), child.lines.end());
                child_interfaces.emplace(original.sid, std::move(child));
            } else {
                auto block = original;
                block.name = prefix + block.name;
                result.blocks.push_back(std::move(block));
            }
        }
        const auto resolve_child_source = [&](const std::string& endpoint) {
            const auto [sid, port] = slx_endpoint(endpoint);
            if (const auto child = child_interfaces.find(sid);
                child != child_interfaces.end()) {
                const auto output = child->second.outputs.find(port);
                if (output == child->second.outputs.end()) {
                    throw std::invalid_argument("unconnected SubSystem output port");
                }
                return output->second;
            }
            return endpoint;
        };
        std::unordered_map<std::string, std::string> goto_inputs;
        std::unordered_map<std::string, std::size_t> from_uses;
        std::unordered_map<std::string, std::size_t> goto_uses;
        for (const auto& line : scopes[scope_index].lines) {
            const auto source = line.parameters.find("Src");
            if (source == line.parameters.end()) throw std::invalid_argument("SLX Line requires Src");
            const auto [source_sid, source_port] = slx_endpoint(source->second);
            const auto source_tag = tag_blocks.find(source_sid);
            if (source_tag != tag_blocks.end()) {
                if (source_tag->second.type == "Goto") {
                    throw std::invalid_argument(prefix + "Goto block has a physical output line");
                }
                if (source_port != 1) {
                    throw std::invalid_argument(prefix + "From block requires output port 1");
                }
                ++from_uses[source_sid];
            }
            for (const auto& target : line_targets(line)) {
                const auto [target_sid, target_port] = slx_endpoint(target);
                const auto target_tag = tag_blocks.find(target_sid);
                if (target_tag == tag_blocks.end()) continue;
                if (target_tag->second.type == "From") {
                    throw std::invalid_argument(prefix + "From block has a physical input line");
                }
                if (target_port != 1 || source_tag != tag_blocks.end() ||
                    connectors.contains(source_sid) || child_interfaces.contains(source_sid)) {
                    throw std::invalid_argument(
                        prefix + "unsupported connector/Goto/From chain");
                }
                if (!goto_inputs.emplace(
                        target_sid, resolve_child_source(source->second)).second) {
                    throw std::invalid_argument(prefix + "Goto input has multiple drivers");
                }
            }
        }
        for (const auto& [sid, tag_block] : tag_blocks) {
            if (tag_block.type == "Goto" && !goto_inputs.contains(sid)) {
                throw std::invalid_argument(prefix + "Goto input is unconnected");
            }
            if (tag_block.type == "From" && !from_uses.contains(sid)) {
                throw std::invalid_argument(prefix + "From output is unconsumed");
            }
            if (tag_block.type == "From" && !goto_by_tag.contains(tag_block.tag)) {
                throw std::invalid_argument(prefix + "unresolved local From tag");
            }
        }
        for (const auto& line : scopes[scope_index].lines) {
            const auto source = line.parameters.find("Src");
            if (source == line.parameters.end()) throw std::invalid_argument("SLX Line requires Src");
            const auto [source_sid, source_port] = slx_endpoint(source->second);
            auto resolved_source = resolve_child_source(source->second);
            const auto source_connector = connectors.find(source_sid);
            if (const auto source_tag = tag_blocks.find(source_sid);
                source_tag != tag_blocks.end()) {
                if (source_tag->second.type == "Goto") continue;
                const auto goto_sid = goto_by_tag.at(source_tag->second.tag);
                resolved_source = goto_inputs.at(goto_sid);
                ++goto_uses[goto_sid];
            }
            for (const auto& target : line_targets(line)) {
                const auto [target_sid, target_port] = slx_endpoint(target);
                if (tag_blocks.contains(target_sid)) continue;
                const auto target_connector = connectors.find(target_sid);
                if (tag_blocks.contains(source_sid) &&
                    (target_connector != connectors.end() ||
                     child_interfaces.contains(target_sid))) {
                    throw std::invalid_argument(
                        prefix + "unsupported connector/Goto/From chain");
                }
                std::vector<std::string> resolved_targets{target};
                if (const auto child = child_interfaces.find(target_sid);
                    child != child_interfaces.end()) {
                    const auto input = child->second.inputs.find(target_port);
                    if (input == child->second.inputs.end() || input->second.empty()) {
                        throw std::invalid_argument("unconnected SubSystem input port");
                    }
                    resolved_targets = input->second;
                }
                if (source_connector != connectors.end()) {
                    if (source_connector->second.first != "Inport" || source_port != 1 ||
                        target_connector != connectors.end()) {
                        throw std::invalid_argument("unsupported SubSystem connector chain");
                    }
                    result.inputs[source_connector->second.second].insert(
                        result.inputs[source_connector->second.second].end(),
                        resolved_targets.begin(), resolved_targets.end());
                    continue;
                }
                if (target_connector != connectors.end()) {
                    if (target_connector->second.first != "Outport" || target_port != 1 ||
                        resolved_targets.size() != 1) {
                        throw std::invalid_argument("invalid SubSystem output connector");
                    }
                    if (!result.outputs.emplace(
                            target_connector->second.second, resolved_source).second) {
                        throw std::invalid_argument(
                            "multiple drivers for SubSystem output port");
                    }
                    continue;
                }
                for (const auto& resolved_target : resolved_targets) {
                    ParsedLine flattened;
                    flattened.parameters.emplace("Src", resolved_source);
                    flattened.parameters.emplace("Dst", resolved_target);
                    result.lines.push_back(std::move(flattened));
                }
            }
        }
        for (const auto& [tag, sid] : goto_by_tag) {
            if (!goto_uses.contains(sid)) {
                throw std::invalid_argument(prefix + "local Goto tag has no From consumer: " + tag);
            }
        }
        for (const auto& [_, connector] : connectors) {
            const auto& [type, port] = connector;
            if ((type == "Inport" && !result.inputs.contains(port)) ||
                (type == "Outport" && !result.outputs.contains(port))) {
                throw std::invalid_argument("unconnected SubSystem connector port");
            }
        }
        for (const auto& [sid, child] : child_interfaces) {
            std::unordered_set<std::size_t> consumed_inputs;
            std::unordered_set<std::size_t> consumed_outputs;
            for (const auto& line : scopes[scope_index].lines) {
                const auto source = line.parameters.find("Src");
                if (source == line.parameters.end()) continue;
                const auto [source_sid, source_port] = slx_endpoint(source->second);
                if (source_sid == sid) consumed_outputs.insert(source_port);
                for (const auto& target : line_targets(line)) {
                    const auto [target_sid, target_port] = slx_endpoint(target);
                    if (target_sid == sid) consumed_inputs.insert(target_port);
                }
            }
            for (const auto& [port, _] : child.inputs) {
                if (!consumed_inputs.contains(port)) {
                    throw std::invalid_argument("unconnected SubSystem input port");
                }
            }
            for (const auto& [port, _] : child.outputs) {
                if (!consumed_outputs.contains(port)) {
                    throw std::invalid_argument("unconnected SubSystem output port");
                }
            }
        }
        return result;
    };
    auto flattened = flatten_scope(0, {}, std::nullopt);
    auto blocks = std::move(flattened.blocks);
    auto lines = std::move(flattened.lines);
    if (blocks.empty()) throw std::invalid_argument("SLX block diagram has no supported top-level blocks");

    std::unordered_map<std::string, std::size_t> sid_to_node;
    std::unordered_set<std::string> names;
    for (const auto& block : blocks) {
        BlockGraphNode node;
        node.id = block.name;
        if (node.id.find('.') != std::string::npos) {
            throw std::invalid_argument(
                node.id + ": SLX block names containing '.' are unsupported");
        }
        if (!names.insert(node.id).second ||
            !sid_to_node.emplace(block.sid, graph.nodes.size()).second) {
            throw std::invalid_argument("duplicate SLX block Name or SID");
        }
        const auto sample = block.parameters.find("SampleTime");
        if (sample == block.parameters.end()) {
            throw std::invalid_argument(node.id + ": explicit SampleTime is required");
        }
        std::tie(node.sample_time, node.sample_offset) = slx_sample_time(sample->second);
        if (block.block_type == "Constant") {
            node.type = "constant";
            const auto value = block.parameters.find("Value");
            if (value == block.parameters.end()) {
                throw std::invalid_argument(node.id + ": Constant requires Value");
            }
            node.attributes["value"] = std::to_string(slx_number(value->second, "Constant Value"));
        } else if (block.block_type == "Gain") {
            node.type = "gain";
            const auto gain = block.parameters.find("Gain");
            if (gain == block.parameters.end()) {
                throw std::invalid_argument(node.id + ": Gain requires Gain parameter");
            }
            node.attributes["gain"] = std::to_string(slx_number(gain->second, "Gain"));
        } else if (block.block_type == "Sum") {
            node.type = "sum";
            const auto inputs = block.parameters.find("Inputs");
            if (inputs == block.parameters.end() || inputs->second.empty() ||
                !std::all_of(inputs->second.begin(), inputs->second.end(), [](char value) {
                    return value == '+' || value == '-' || value == '|';
                })) {
                throw std::invalid_argument(node.id + ": invalid SLX Sum Inputs contract");
            }
            std::string signs;
            std::copy_if(
                inputs->second.begin(), inputs->second.end(), std::back_inserter(signs),
                [](char value) { return value == '+' || value == '-'; });
            if (signs.empty()) {
                throw std::invalid_argument(node.id + ": SLX Sum has no input signs");
            }
            node.attributes["signs"] = std::move(signs);
        } else if (block.block_type == "UnitDelay") {
            node.type = "unit_delay";
            if (!(node.sample_time > 0.0)) {
                throw std::invalid_argument(node.id + ": UnitDelay requires positive SampleTime");
            }
            const auto initial = block.parameters.find("InitialCondition");
            if (initial == block.parameters.end()) {
                throw std::invalid_argument(node.id + ": UnitDelay requires InitialCondition");
            }
            node.attributes["initial"] = std::to_string(
                slx_number(initial->second, "UnitDelay InitialCondition"));
        } else if (block.block_type == "Switch") {
            node.type = "switch";
            const auto criteria = block.parameters.find("Criteria");
            const auto threshold = block.parameters.find("Threshold");
            const std::string criterion = criteria == block.parameters.end()
                ? "u2 >= Threshold" : criteria->second;
            if (criterion == "u2 > Threshold") {
                node.attributes["criterion"] = "gt";
            } else if (criterion == "u2 >= Threshold") {
                node.attributes["criterion"] = "ge";
            } else if (criterion == "u2 ~= 0") {
                node.attributes["criterion"] = "ne_zero";
            } else {
                throw std::invalid_argument(node.id + ": unsupported SLX Switch Criteria");
            }
            const double threshold_value = threshold == block.parameters.end()
                ? 0.0 : slx_number(threshold->second, "Switch Threshold");
            node.attributes["threshold"] = std::to_string(threshold_value);
        } else {
            throw std::invalid_argument(
                "unsupported native SLX block type: " + block.block_type);
        }
        graph.nodes.push_back(std::move(node));
    }
    const auto input_port = [&](const BlockGraphNode& node, std::size_t port) {
        if (node.type == "gain" || node.type == "unit_delay") {
            if (port != 1) throw std::invalid_argument(node.id + ": invalid SLX input port");
            return std::string("in");
        }
        if (node.type == "sum") return "in" + std::to_string(port);
        if (node.type == "switch") {
            if (port == 1) return std::string("true");
            if (port == 2) return std::string("control");
            if (port == 3) return std::string("false");
        }
        throw std::invalid_argument(node.id + ": invalid SLX input port");
    };
    for (const auto& line : lines) {
        const auto source = line.parameters.find("Src");
        if (source == line.parameters.end()) {
            throw std::invalid_argument("SLX Line requires Src");
        }
        const auto [source_sid, source_port] = slx_endpoint(source->second);
        if (!sid_to_node.contains(source_sid) || source_port != 1) {
            throw std::invalid_argument("SLX Line references unsupported source or output port");
        }
        const auto& source_node = graph.nodes[sid_to_node.at(source_sid)];
        std::vector<std::string> targets;
        if (const auto target = line.parameters.find("Dst"); target != line.parameters.end()) {
            targets.push_back(target->second);
        }
        for (const auto& branch : line.branch_parameters) {
            if (branch.contains("Src")) {
                throw std::invalid_argument("SLX Branch cannot override its Line Src");
            }
            const auto target = branch.find("Dst");
            if (target != branch.end()) targets.push_back(target->second);
        }
        if (targets.empty()) throw std::invalid_argument("SLX Line has no Dst or Branch Dst");
        for (const auto& target : targets) {
            const auto [target_sid, target_port] = slx_endpoint(target);
            if (!sid_to_node.contains(target_sid)) {
                throw std::invalid_argument("SLX Line references unsupported target block");
            }
            const auto& target_node = graph.nodes[sid_to_node.at(target_sid)];
            graph.connections.push_back({
                source_node.id, "out", target_node.id,
                input_port(target_node, target_port)});
        }
    }
    graph.commit_order = schedule(graph);
    graph.validate(path.parent_path());
    return graph;
}

}  // namespace

void BlockGraphIR::validate(const std::filesystem::path& base_directory) const {
    if (schema_version != kBlockGraphSchemaVersion) {
        throw std::invalid_argument("unsupported block graph schema: " + schema_version);
    }
    if (model_id.empty()) throw std::invalid_argument("block graph model id is empty");
    std::unordered_set<std::string> ids;
    for (const auto& node : nodes) {
        if (!ids.insert(node.id).second) {
            throw std::invalid_argument("duplicate block graph node: " + node.id);
        }
        if (!supported_type(node.type)) {
            throw std::invalid_argument("unsupported Simulink bridge block type: " + node.type);
        }
        if (!std::isfinite(node.sample_time) || node.sample_time < 0.0) {
            throw std::invalid_argument(node.id + ": sample time cannot be negative");
        }
        if (!std::isfinite(node.sample_offset) || node.sample_offset < 0.0) {
            throw std::invalid_argument(node.id + ": sample offset cannot be negative");
        }
        if ((node.sample_time == 0.0 && node.sample_offset != 0.0) ||
            (node.sample_time > 0.0 && node.sample_offset >= node.sample_time)) {
            throw std::invalid_argument(
                node.id + ": sample offset must satisfy 0 <= offset < sample time");
        }
        if (node.sample_offset > 0.0) {
            if (node.type == "algebraic_model") {
                throw std::invalid_argument(
                    node.id + ": offset algebraic_model needs an explicit output contract");
            }
            if (node.type != "unit_delay" &&
                !node.attributes.contains("initial_output")) {
                throw std::invalid_argument(
                    node.id + ": nonzero sample offset requires initial_output");
            }
        }
        if (node.type == "algebraic_model") {
            const auto iterator = node.attributes.find("ir");
            if (iterator == node.attributes.end()) {
                throw std::invalid_argument(node.id + ": algebraic_model requires ir attribute");
            }
            const auto ir_path = base_directory.empty()
                ? std::filesystem::path(iterator->second)
                : base_directory / iterator->second;
            if (!std::filesystem::exists(ir_path)) {
                throw std::invalid_argument(node.id + ": model IR does not exist: " + ir_path.string());
            }
            (void)ModelIR::read(ir_path);
        }
        if (node.type == "switch") {
            const auto criterion = node.attributes.find("criterion");
            const auto threshold = node.attributes.find("threshold");
            if ((criterion == node.attributes.end()) !=
                (threshold == node.attributes.end())) {
                throw std::invalid_argument(
                    node.id + ": switch criterion and threshold must be declared together");
            }
            if (criterion != node.attributes.end()) {
                if (criterion->second != "gt" && criterion->second != "ge" &&
                    criterion->second != "ne_zero") {
                    throw std::invalid_argument(node.id + ": invalid switch criterion");
                }
                std::size_t consumed{};
                const double value = std::stod(threshold->second, &consumed);
                if (consumed != threshold->second.size() || !std::isfinite(value)) {
                    throw std::invalid_argument(node.id + ": invalid switch threshold");
                }
            }
        }
    }
    std::unordered_set<std::string> target_ports;
    for (const auto& connection : connections) {
        if (!ids.contains(connection.source_node) || !ids.contains(connection.target_node)) {
            throw std::invalid_argument("connection references an unknown node");
        }
        const auto target_key = connection.target_node + "." + connection.target_port;
        if (!target_ports.insert(target_key).second) {
            throw std::invalid_argument("multiple drivers for port: " + target_key);
        }
        const auto& target = *std::find_if(
            nodes.begin(), nodes.end(), [&](const BlockGraphNode& node) {
                return node.id == connection.target_node;
            });
        const auto signs = target.attributes.find("signs");
        if (target.type == "sum" && signs != target.attributes.end()) {
            if (!connection.target_port.starts_with("in")) {
                throw std::invalid_argument(target.id + ": signed sum requires inN ports");
            }
            std::size_t consumed{};
            const auto port = std::stoull(connection.target_port.substr(2), &consumed);
            if (consumed != connection.target_port.size() - 2 || port == 0 ||
                port > signs->second.size()) {
                throw std::invalid_argument(target.id + ": signed sum input port is out of range");
            }
        }
    }
    for (const auto& node : nodes) {
        const auto signs = node.attributes.find("signs");
        if (node.type != "sum" || signs == node.attributes.end()) continue;
        if (signs->second.empty() ||
            !std::all_of(signs->second.begin(), signs->second.end(), [](char sign) {
                return sign == '+' || sign == '-';
            })) {
            throw std::invalid_argument(node.id + ": invalid signed sum contract");
        }
        for (std::size_t port = 1; port <= signs->second.size(); ++port) {
            if (!target_ports.contains(node.id + ".in" + std::to_string(port))) {
                throw std::invalid_argument(node.id + ": signed sum input is unconnected");
            }
        }
    }
    for (const auto& node : nodes) {
        if (node.type != "switch") continue;
        for (const auto* port : {"true", "control", "false"}) {
            if (!target_ports.contains(node.id + "." + port)) {
                throw std::invalid_argument(node.id + ": switch input is unconnected: " + port);
            }
        }
    }
    if (schedule(*this) != commit_order) {
        throw std::invalid_argument("block graph commit order is not its deterministic schedule");
    }
}

void BlockGraphIR::write(const std::filesystem::path& path) const {
    validate(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write block graph IR: " + path.string());
    output << "SMAVE_BLOCK_GRAPH 2\nMODEL " << std::quoted(model_id) << '\n'
           << "SOURCE_HASH " << std::quoted(source_hash) << '\n'
           << "NODES " << nodes.size() << '\n';
    for (const auto& node : nodes) {
        output << "NODE " << std::quoted(node.id) << ' ' << std::quoted(node.type) << ' '
               << std::setprecision(17) << node.sample_time << ' '
               << node.sample_offset << ' ' << node.attributes.size();
        for (const auto& [key, value] : node.attributes) {
            output << ' ' << std::quoted(key) << ' ' << std::quoted(value);
        }
        output << '\n';
    }
    output << "CONNECTIONS " << connections.size() << '\n';
    for (const auto& connection : connections) {
        output << "CONNECTION " << std::quoted(connection.source_node) << ' '
               << std::quoted(connection.source_port) << ' '
               << std::quoted(connection.target_node) << ' '
               << std::quoted(connection.target_port) << '\n';
    }
    output << "COMMIT_ORDER " << commit_order.size();
    for (const auto& id : commit_order) output << ' ' << std::quoted(id);
    output << "\nEND\n";
}

BlockGraphIR BlockGraphIR::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read block graph IR: " + path.string());
    require_tag(input, "SMAVE_BLOCK_GRAPH");
    int version{};
    input >> version;
    if (version != 1 && version != 2) {
        throw std::invalid_argument("unsupported block graph version");
    }
    BlockGraphIR graph;
    require_tag(input, "MODEL");
    input >> std::quoted(graph.model_id);
    require_tag(input, "SOURCE_HASH");
    input >> std::quoted(graph.source_hash);
    require_tag(input, "NODES");
    std::size_t count{};
    input >> count;
    graph.nodes.resize(count);
    for (auto& node : graph.nodes) {
        require_tag(input, "NODE");
        std::size_t attributes{};
        input >> std::quoted(node.id) >> std::quoted(node.type) >> node.sample_time;
        if (version == 2) input >> node.sample_offset;
        input >> attributes;
        for (std::size_t index = 0; index < attributes; ++index) {
            std::string key;
            std::string value;
            input >> std::quoted(key) >> std::quoted(value);
            node.attributes.emplace(std::move(key), std::move(value));
        }
    }
    require_tag(input, "CONNECTIONS");
    input >> count;
    graph.connections.resize(count);
    for (auto& connection : graph.connections) {
        require_tag(input, "CONNECTION");
        input >> std::quoted(connection.source_node) >> std::quoted(connection.source_port)
              >> std::quoted(connection.target_node) >> std::quoted(connection.target_port);
    }
    require_tag(input, "COMMIT_ORDER");
    input >> count;
    graph.commit_order.resize(count);
    for (auto& id : graph.commit_order) input >> std::quoted(id);
    require_tag(input, "END");
    graph.validate(path.parent_path());
    return graph;
}

BlockGraphIR import_block_graph(const std::filesystem::path& path) {
    if (path.extension() == ".slx") return import_slx_block_graph(path);
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read Simulink bridge export: " + path.string());
    std::ostringstream raw;
    raw << input.rdbuf();
    std::istringstream parser(raw.str());
    require_tag(parser, "SMAVE_SIMULINK_EXPORT");
    int version{};
    parser >> version;
    if (version != 1 && version != 2) {
        throw std::invalid_argument("unsupported Simulink bridge export version");
    }
    BlockGraphIR graph;
    graph.source_hash = digest(raw.str());
    require_tag(parser, "MODEL");
    parser >> std::quoted(graph.model_id);
    std::string tag;
    while (parser >> tag) {
        if (tag == "NODE") {
            BlockGraphNode node;
            std::size_t attributes{};
            parser >> std::quoted(node.id) >> std::quoted(node.type) >> node.sample_time;
            if (version == 2) parser >> node.sample_offset;
            parser >> attributes;
            for (std::size_t index = 0; index < attributes; ++index) {
                std::string key;
                std::string value;
                parser >> std::quoted(key) >> std::quoted(value);
                node.attributes.emplace(std::move(key), std::move(value));
            }
            graph.nodes.push_back(std::move(node));
        } else if (tag == "CONNECTION") {
            BlockGraphConnection connection;
            parser >> std::quoted(connection.source_node) >> std::quoted(connection.source_port)
                   >> std::quoted(connection.target_node) >> std::quoted(connection.target_port);
            graph.connections.push_back(std::move(connection));
        } else if (tag == "END") {
            break;
        } else {
            throw std::invalid_argument("unknown Simulink bridge export tag: " + tag);
        }
        if (!parser) throw std::runtime_error("truncated Simulink bridge export");
    }
    graph.commit_order = schedule(graph);
    graph.validate(path.parent_path());
    for (auto& node : graph.nodes) {
        if (node.type != "algebraic_model") continue;
        auto ir_path = std::filesystem::path(node.attributes.at("ir"));
        if (ir_path.is_relative()) ir_path = path.parent_path() / ir_path;
        node.attributes["ir"] = std::filesystem::absolute(ir_path).lexically_normal().string();
    }
    return graph;
}

}  // namespace smave
