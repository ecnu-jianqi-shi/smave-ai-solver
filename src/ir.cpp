#include "smave/ir.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
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

template <typename Container>
void write_strings(std::ostream& output, const Container& values) {
    output << values.size();
    for (const auto& value : values) {
        output << ' ' << std::quoted(value);
    }
    output << '\n';
}

std::vector<std::string> read_strings(std::istream& input) {
    std::size_t count{};
    input >> count;
    std::vector<std::string> result(count);
    for (auto& value : result) {
        input >> std::quoted(value);
    }
    return result;
}

void require_tag(std::istream& input, std::string_view expected) {
    std::string actual;
    input >> actual;
    if (!input || actual != expected) {
        throw std::runtime_error(
            "invalid IR: expected " + std::string(expected) + ", got " + actual);
    }
}

const VariableIR& find_variable(const ModelIR& model, const std::string& name) {
    const auto iterator = std::find_if(
        model.variables.begin(), model.variables.end(),
        [&](const VariableIR& variable) { return variable.name == name; });
    if (iterator == model.variables.end()) {
        throw std::invalid_argument("unknown variable: " + name);
    }
    return *iterator;
}

const EquationIR& find_equation(const ModelIR& model, const std::string& id) {
    const auto iterator = std::find_if(
        model.equations.begin(), model.equations.end(),
        [&](const EquationIR& equation) { return equation.id == id; });
    if (iterator == model.equations.end()) {
        throw std::invalid_argument("unknown equation: " + id);
    }
    return *iterator;
}

std::string block_fingerprint_for_schema(
    const BlockIR& block,
    const ModelIR& model,
    std::string_view schema_version) {
    std::ostringstream contract;
    contract << schema_version << '|' << block.mode << '|' << block.original_solver
             << '|' << block.linear << '|';
    for (const auto& name : block.unknowns) {
        const auto& variable = find_variable(model, name);
        contract << variable.name << ':' << variable.nominal << ':' << variable.unit << ';';
    }
    contract << '|';
    for (const auto& name : block.contexts) {
        const auto& variable = find_variable(model, name);
        contract << variable.name << ':' << variable.nominal << ':' << variable.unit << ';';
    }
    contract << '|';
    for (const auto& id : block.equation_ids) {
        contract << find_equation(model, id).residual << ';';
    }
    contract << '|';
    if (schema_version == kLegacyIrSchemaVersion) {
        for (std::size_t row = 0; row < block.jacobian_sparsity.row_count; ++row) {
            for (std::size_t column = 0;
                 column < block.jacobian_sparsity.column_count; ++column) {
                contract << block.jacobian_sparsity.contains(row, column);
            }
            contract << ';';
        }
    } else {
        contract << block.jacobian_sparsity.row_count << ':'
                 << block.jacobian_sparsity.column_count << ':';
        for (const auto offset : block.jacobian_sparsity.row_offsets) {
            contract << offset << ',';
        }
        contract << ':';
        for (const auto column : block.jacobian_sparsity.column_indices) {
            contract << column << ',';
        }
    }
    return digest(contract.str());
}

}  // namespace

SparsityPattern SparsityPattern::from_rows(
    std::size_t columns,
    const std::vector<std::vector<std::size_t>>& rows) {
    SparsityPattern pattern;
    pattern.row_count = rows.size();
    pattern.column_count = columns;
    pattern.row_offsets.reserve(rows.size() + 1);
    pattern.row_offsets.push_back(0);
    for (auto row : rows) {
        std::sort(row.begin(), row.end());
        row.erase(std::unique(row.begin(), row.end()), row.end());
        if (!row.empty() && row.back() >= columns) {
            throw std::invalid_argument("sparsity column index is out of range");
        }
        pattern.column_indices.insert(
            pattern.column_indices.end(), row.begin(), row.end());
        pattern.row_offsets.push_back(pattern.column_indices.size());
    }
    pattern.validate();
    return pattern;
}

SparsityPattern SparsityPattern::from_dense(
    const std::vector<std::vector<int>>& dense) {
    const std::size_t columns = dense.empty() ? 0 : dense.front().size();
    std::vector<std::vector<std::size_t>> rows(dense.size());
    for (std::size_t row = 0; row < dense.size(); ++row) {
        if (dense[row].size() != columns) {
            throw std::invalid_argument("dense sparsity rows have inconsistent widths");
        }
        for (std::size_t column = 0; column < columns; ++column) {
            if (dense[row][column] != 0) rows[row].push_back(column);
        }
    }
    return from_rows(columns, rows);
}

void SparsityPattern::validate() const {
    if (row_offsets.size() != row_count + 1 || row_offsets.empty() ||
        row_offsets.front() != 0 || row_offsets.back() != column_indices.size()) {
        throw std::invalid_argument("invalid CSR row offsets");
    }
    for (std::size_t row_index = 0; row_index < row_count; ++row_index) {
        if (row_offsets[row_index] > row_offsets[row_index + 1]) {
            throw std::invalid_argument("CSR row offsets are not monotonic");
        }
        std::size_t previous{};
        bool first = true;
        for (std::size_t offset = row_offsets[row_index];
             offset < row_offsets[row_index + 1]; ++offset) {
            const auto column = column_indices[offset];
            if (column >= column_count || (!first && column <= previous)) {
                throw std::invalid_argument("CSR columns are invalid or unsorted");
            }
            previous = column;
            first = false;
        }
    }
}

bool SparsityPattern::empty() const { return row_count == 0; }

std::size_t SparsityPattern::nonzeros() const { return column_indices.size(); }

bool SparsityPattern::contains(std::size_t row_index, std::size_t column) const {
    if (row_index >= row_count || column >= column_count) return false;
    const auto values = row(row_index);
    return std::binary_search(values.begin(), values.end(), column);
}

std::span<const std::size_t> SparsityPattern::row(std::size_t index) const {
    if (index >= row_count) throw std::out_of_range("sparsity row is out of range");
    if (row_offsets[index] == row_offsets[index + 1]) return {};
    return std::span<const std::size_t>(
        column_indices.data() + row_offsets[index],
        row_offsets[index + 1] - row_offsets[index]);
}

std::vector<std::size_t> SparsityPattern::greedy_column_coloring() const {
    validate();
    std::vector<std::vector<std::size_t>> conflicts(column_count);
    for (std::size_t row_index = 0; row_index < row_count; ++row_index) {
        const auto columns = row(row_index);
        for (std::size_t left = 0; left < columns.size(); ++left) {
            for (std::size_t right = left + 1; right < columns.size(); ++right) {
                conflicts[columns[left]].push_back(columns[right]);
                conflicts[columns[right]].push_back(columns[left]);
            }
        }
    }
    for (auto& neighbors : conflicts) {
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
    }
    const auto uncolored = column_count;
    std::vector<std::size_t> colors(column_count, uncolored);
    std::vector<bool> forbidden;
    for (std::size_t column = 0; column < column_count; ++column) {
        forbidden.assign(column_count, false);
        for (const auto neighbor : conflicts[column]) {
            if (colors[neighbor] != uncolored) forbidden[colors[neighbor]] = true;
        }
        std::size_t color{};
        while (color < forbidden.size() && forbidden[color]) ++color;
        colors[column] = color;
    }
    return colors;
}

void ModelIR::validate() const {
    if (schema_version != kIrSchemaVersion) {
        throw std::invalid_argument("unsupported IR schema: " + schema_version);
    }
    std::unordered_set<std::string> variable_names;
    for (const auto& variable : variables) {
        if (!variable_names.insert(variable.name).second) {
            throw std::invalid_argument("duplicate variable: " + variable.name);
        }
        if (!(variable.nominal > 0.0)) {
            throw std::invalid_argument("variable nominal must be positive: " + variable.name);
        }
    }
    std::unordered_set<std::string> equation_ids;
    for (const auto& equation : equations) {
        if (!equation_ids.insert(equation.id).second) {
            throw std::invalid_argument("duplicate equation: " + equation.id);
        }
    }
    for (const auto& block : blocks) {
        if (block.original_solver.empty()) {
            throw std::invalid_argument(block.id + ": missing original solver fallback");
        }
        if (block.equation_ids.empty()) {
            throw std::invalid_argument(block.id + ": missing runtime residual");
        }
        for (const auto& name : block.unknowns) find_variable(*this, name);
        for (const auto& name : block.contexts) find_variable(*this, name);
        for (const auto& id : block.equation_ids) find_equation(*this, id);
        block.jacobian_sparsity.validate();
        if (block.jacobian_sparsity.row_count != block.equation_ids.size() ||
            block.jacobian_sparsity.column_count != block.unknowns.size()) {
            throw std::invalid_argument(block.id + ": sparsity dimensions do not match block");
        }
        if (block.fingerprint != block_fingerprint(block, *this)) {
            throw std::invalid_argument(block.id + ": incompatible fingerprint");
        }
    }
}

std::string block_fingerprint(const BlockIR& block, const ModelIR& model) {
    return block_fingerprint_for_schema(block, model, kIrSchemaVersion);
}

void ModelIR::write(const std::filesystem::path& path) const {
    validate();
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write IR: " + path.string());
    output << std::setprecision(17);
    output << "SMAVE_IR " << std::quoted(schema_version) << '\n';
    output << "MODEL " << std::quoted(model_id) << ' ' << std::quoted(source_hash)
           << ' ' << std::quoted(frontend_version) << '\n';
    output << "CAPABILITIES ";
    write_strings(output, capabilities);
    output << "VARIABLES " << variables.size() << '\n';
    for (const auto& variable : variables) {
        output << "VARIABLE " << std::quoted(variable.name) << ' '
               << std::quoted(variable.kind) << ' ' << variable.nominal << ' '
               << variable.start << ' ' << variable.minimum.has_value() << ' '
               << variable.minimum.value_or(0.0) << ' ' << variable.maximum.has_value()
               << ' ' << variable.maximum.value_or(0.0) << ' '
               << std::quoted(variable.unit) << '\n';
    }
    output << "EQUATIONS " << equations.size() << '\n';
    for (const auto& equation : equations) {
        output << "EQUATION " << std::quoted(equation.id) << ' '
               << std::quoted(equation.residual) << ' ' << equation.source_line << ' ';
        write_strings(output, equation.variables);
    }
    output << "BLOCKS " << blocks.size() << '\n';
    for (const auto& block : blocks) {
        output << "BLOCK " << std::quoted(block.id) << ' ' << block.linear << ' '
               << block.smooth << ' ' << block.event_related << ' ' << block.dae_index
               << ' ' << std::quoted(block.mode) << ' '
               << std::quoted(block.original_solver) << ' '
               << std::quoted(block.fingerprint) << '\n';
        output << "UNKNOWNS "; write_strings(output, block.unknowns);
        output << "CONTEXTS "; write_strings(output, block.contexts);
        output << "BLOCK_EQUATIONS "; write_strings(output, block.equation_ids);
        output << "SPARSITY_CSR " << block.jacobian_sparsity.row_count << ' '
               << block.jacobian_sparsity.column_count << ' '
               << block.jacobian_sparsity.nonzeros() << '\n';
        output << "ROW_OFFSETS " << block.jacobian_sparsity.row_offsets.size();
        for (const auto offset : block.jacobian_sparsity.row_offsets) {
            output << ' ' << offset;
        }
        output << '\n';
        output << "COLUMN_INDICES " << block.jacobian_sparsity.column_indices.size();
        for (const auto column : block.jacobian_sparsity.column_indices) {
            output << ' ' << column;
        }
        output << '\n';
    }
    output << "END\n";
}

ModelIR ModelIR::read(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read IR: " + path.string());
    ModelIR model;
    require_tag(input, "SMAVE_IR"); input >> std::quoted(model.schema_version);
    const bool legacy = model.schema_version == kLegacyIrSchemaVersion;
    if (!legacy && model.schema_version != kIrSchemaVersion) {
        throw std::runtime_error("unsupported IR schema: " + model.schema_version);
    }
    require_tag(input, "MODEL");
    input >> std::quoted(model.model_id) >> std::quoted(model.source_hash)
          >> std::quoted(model.frontend_version);
    require_tag(input, "CAPABILITIES"); model.capabilities = read_strings(input);
    require_tag(input, "VARIABLES");
    std::size_t variable_count{}; input >> variable_count;
    for (std::size_t index = 0; index < variable_count; ++index) {
        require_tag(input, "VARIABLE");
        VariableIR variable;
        bool has_minimum{}, has_maximum{};
        double minimum{}, maximum{};
        input >> std::quoted(variable.name) >> std::quoted(variable.kind)
              >> variable.nominal >> variable.start >> has_minimum >> minimum
              >> has_maximum >> maximum >> std::quoted(variable.unit);
        if (has_minimum) variable.minimum = minimum;
        if (has_maximum) variable.maximum = maximum;
        model.variables.push_back(std::move(variable));
    }
    require_tag(input, "EQUATIONS");
    std::size_t equation_count{}; input >> equation_count;
    for (std::size_t index = 0; index < equation_count; ++index) {
        require_tag(input, "EQUATION");
        EquationIR equation;
        input >> std::quoted(equation.id) >> std::quoted(equation.residual)
              >> equation.source_line;
        equation.variables = read_strings(input);
        model.equations.push_back(std::move(equation));
    }
    require_tag(input, "BLOCKS");
    std::size_t block_count{}; input >> block_count;
    for (std::size_t index = 0; index < block_count; ++index) {
        require_tag(input, "BLOCK");
        BlockIR block;
        input >> std::quoted(block.id) >> block.linear >> block.smooth
              >> block.event_related >> block.dae_index >> std::quoted(block.mode)
              >> std::quoted(block.original_solver) >> std::quoted(block.fingerprint);
        require_tag(input, "UNKNOWNS"); block.unknowns = read_strings(input);
        require_tag(input, "CONTEXTS"); block.contexts = read_strings(input);
        require_tag(input, "BLOCK_EQUATIONS"); block.equation_ids = read_strings(input);
        if (legacy) {
            require_tag(input, "SPARSITY");
            std::size_t rows{}, columns{}; input >> rows >> columns;
            std::vector<std::vector<int>> dense(rows, std::vector<int>(columns));
            for (auto& row : dense) {
                for (auto& item : row) input >> item;
            }
            block.jacobian_sparsity = SparsityPattern::from_dense(dense);
        } else {
            require_tag(input, "SPARSITY_CSR");
            std::size_t expected_nonzeros{};
            input >> block.jacobian_sparsity.row_count
                  >> block.jacobian_sparsity.column_count >> expected_nonzeros;
            require_tag(input, "ROW_OFFSETS");
            std::size_t offset_count{}; input >> offset_count;
            block.jacobian_sparsity.row_offsets.resize(offset_count);
            for (auto& offset : block.jacobian_sparsity.row_offsets) input >> offset;
            require_tag(input, "COLUMN_INDICES");
            std::size_t column_count{}; input >> column_count;
            block.jacobian_sparsity.column_indices.resize(column_count);
            for (auto& column : block.jacobian_sparsity.column_indices) input >> column;
            if (column_count != expected_nonzeros) {
                throw std::runtime_error("invalid IR: CSR nonzero count mismatch");
            }
            block.jacobian_sparsity.validate();
        }
        model.blocks.push_back(std::move(block));
    }
    require_tag(input, "END");
    if (!input) throw std::runtime_error("truncated or malformed IR");
    if (legacy) {
        for (auto& block : model.blocks) {
            block.jacobian_sparsity.validate();
            if (block.fingerprint !=
                block_fingerprint_for_schema(block, model, kLegacyIrSchemaVersion)) {
                throw std::invalid_argument(block.id + ": incompatible legacy fingerprint");
            }
        }
        model.schema_version = kIrSchemaVersion;
        for (auto& block : model.blocks) {
            block.fingerprint = block_fingerprint(block, model);
        }
    }
    model.validate();
    return model;
}

}  // namespace smave
