#include "smave/benchmark/sparse_suite.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

namespace smave::benchmark {
namespace {

struct Header {
    bool coordinate{};
    bool array{};
    bool pattern{};
    bool integer{};
    bool symmetric{};
    std::string value_kind;
    std::string symmetry;
};

Header parse_header(const std::string& line) {
    std::istringstream input(line);
    std::string banner;
    std::string object;
    std::string format;
    std::string field;
    std::string symmetry;
    input >> banner >> object >> format >> field >> symmetry;
    if (!input || banner != "%%MatrixMarket" || object != "matrix") {
        throw std::runtime_error("unsupported Matrix Market banner");
    }
    Header header;
    header.coordinate = format == "coordinate";
    header.array = format == "array";
    header.pattern = field == "pattern";
    header.integer = field == "integer";
    header.symmetric = symmetry == "symmetric";
    header.value_kind = field;
    header.symmetry = symmetry;
    if ((!header.coordinate && !header.array) ||
        (!header.pattern && !header.integer && field != "real") ||
        (symmetry != "general" && symmetry != "symmetric")) {
        throw std::runtime_error("unsupported Matrix Market type: " + line);
    }
    return header;
}

std::string next_data_line(std::istream& input) {
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.front() != '%') return line;
    }
    throw std::runtime_error("unexpected end of Matrix Market file");
}

double infinity_norm(const std::vector<double>& values) {
    double result{};
    for (const auto value : values) result = std::max(result, std::abs(value));
    return result;
}

void write_observation(std::ostream& output, const SolverObservation& observation) {
    output << "OBSERVATION " << std::quoted(observation.backend) << ' '
           << std::quoted(observation.status) << ' '
           << std::quoted(observation.reason) << ' '
           << observation.iterations << ' '
           << observation.setup_seconds << ' '
           << observation.solve_seconds << ' '
           << observation.relative_residual << ' '
           << observation.relative_solution_error << ' '
           << observation.peak_resident_bytes << '\n';
}

SolverObservation read_observation(std::istream& input) {
    SolverObservation observation;
    std::string setup_seconds;
    std::string solve_seconds;
    std::string relative_residual;
    std::string relative_solution_error;
    input >> std::quoted(observation.backend)
          >> std::quoted(observation.status)
          >> std::quoted(observation.reason)
          >> observation.iterations
          >> setup_seconds
          >> solve_seconds
          >> relative_residual
          >> relative_solution_error
          >> observation.peak_resident_bytes;
    if (!input) throw std::runtime_error("invalid sparse observation");
    observation.setup_seconds = std::stod(setup_seconds);
    observation.solve_seconds = std::stod(solve_seconds);
    observation.relative_residual = std::stod(relative_residual);
    observation.relative_solution_error = std::stod(relative_solution_error);
    return observation;
}

}  // namespace

std::size_t SparseMatrix::nonzeros() const { return values.size(); }

std::vector<double> SparseMatrix::multiply(const std::vector<double>& input) const {
    if (input.size() != columns) throw std::invalid_argument("sparse multiply shape mismatch");
    std::vector<double> output(rows);
    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t offset = row_offsets[row]; offset < row_offsets[row + 1]; ++offset) {
            output[row] += values[offset] * input[column_indices[offset]];
        }
    }
    return output;
}

SparseMatrix read_matrix_market(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open Matrix Market file: " + path.string());
    std::string banner;
    std::getline(input, banner);
    const auto header = parse_header(banner);
    if (!header.coordinate) throw std::runtime_error("matrix must use coordinate storage");
    std::istringstream dimensions(next_data_line(input));
    std::size_t rows{};
    std::size_t columns{};
    std::size_t entries{};
    dimensions >> rows >> columns >> entries;
    if (!dimensions || rows == 0 || columns == 0) {
        throw std::runtime_error("invalid Matrix Market dimensions");
    }
    const auto data_position = input.tellg();
    std::vector<std::size_t> row_counts(rows);
    for (std::size_t index = 0; index < entries; ++index) {
        std::size_t row{};
        std::size_t column{};
        double value{1.0};
        input >> row >> column;
        if (!header.pattern) input >> value;
        if (!input || row == 0 || column == 0 || row > rows || column > columns ||
            !std::isfinite(value)) {
            throw std::runtime_error("invalid Matrix Market coordinate in " + path.string());
        }
        --row;
        --column;
        if (value == 0.0) continue;
        ++row_counts[row];
        if (header.symmetric && row != column) {
            ++row_counts[column];
        }
    }
    SparseMatrix matrix;
    matrix.rows = rows;
    matrix.columns = columns;
    matrix.declared_symmetric = header.symmetric;
    matrix.pattern = header.pattern;
    matrix.row_offsets.resize(rows + 1);
    for (std::size_t row = 0; row < rows; ++row) {
        matrix.row_offsets[row + 1] = matrix.row_offsets[row] + row_counts[row];
    }
    matrix.column_indices.resize(matrix.row_offsets.back());
    matrix.values.resize(matrix.row_offsets.back());
    auto positions = matrix.row_offsets;
    input.clear();
    input.seekg(data_position);
    for (std::size_t index = 0; index < entries; ++index) {
        std::size_t row{};
        std::size_t column{};
        double value{1.0};
        input >> row >> column;
        if (!header.pattern) input >> value;
        if (!input) throw std::runtime_error("invalid Matrix Market second pass");
        --row;
        --column;
        if (value == 0.0) continue;
        auto offset = positions[row]++;
        matrix.column_indices[offset] = column;
        matrix.values[offset] = value;
        if (header.symmetric && row != column) {
            offset = positions[column]++;
            matrix.column_indices[offset] = row;
            matrix.values[offset] = value;
        }
    }
    for (std::size_t row = 0; row < rows; ++row) {
        const auto begin = matrix.row_offsets[row];
        const auto end = matrix.row_offsets[row + 1];
        std::vector<std::pair<std::size_t, double>> sorted(end - begin);
        for (std::size_t offset = begin; offset < end; ++offset) {
            sorted[offset - begin] = {
                matrix.column_indices[offset], matrix.values[offset]};
        }
        std::sort(sorted.begin(), sorted.end());
        for (std::size_t offset = begin; offset < end; ++offset) {
            matrix.column_indices[offset] = sorted[offset - begin].first;
            matrix.values[offset] = sorted[offset - begin].second;
        }
    }
    std::size_t write_offset{};
    for (std::size_t row = 0; row < rows; ++row) {
        const auto begin = matrix.row_offsets[row];
        const auto end = matrix.row_offsets[row + 1];
        matrix.row_offsets[row] = write_offset;
        for (std::size_t offset = begin; offset < end;) {
            const auto column = matrix.column_indices[offset];
            double value{};
            do {
                value += matrix.values[offset++];
            } while (offset < end && matrix.column_indices[offset] == column);
            if (value == 0.0) continue;
            matrix.column_indices[write_offset] = column;
            matrix.values[write_offset] = value;
            ++write_offset;
        }
    }
    matrix.row_offsets[rows] = write_offset;
    matrix.column_indices.resize(write_offset);
    matrix.values.resize(write_offset);
    return matrix;
}

std::vector<double> read_matrix_market_vector(
    const std::filesystem::path& path, std::size_t expected_size) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open Matrix Market vector: " + path.string());
    std::string banner;
    std::getline(input, banner);
    const auto header = parse_header(banner);
    if (!header.array || header.pattern) {
        throw std::runtime_error("right-hand side must use real/integer array storage");
    }
    std::istringstream dimensions(next_data_line(input));
    std::size_t rows{};
    std::size_t columns{};
    dimensions >> rows >> columns;
    if (!dimensions || rows != expected_size || columns != 1) {
        throw std::runtime_error("right-hand side dimensions do not match matrix");
    }
    std::vector<double> values(rows);
    for (auto& value : values) input >> value;
    if (!input || !std::all_of(values.begin(), values.end(), [](double value) {
            return std::isfinite(value);
        })) {
        throw std::runtime_error("invalid Matrix Market right-hand side");
    }
    return values;
}

std::vector<SparseCase> discover_suite_sparse_cases(
    const std::filesystem::path& root) {
    std::vector<SparseCase> cases;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        const auto relative = entry.path().lexically_relative(root);
        const bool hidden_cache = std::any_of(
            relative.begin(), relative.end(), [](const auto& component) {
                const auto text = component.string();
                return !text.empty() && text.front() == '.';
            });
        if (hidden_cache) continue;
        if (!entry.is_regular_file() || entry.path().extension() != ".mtx" ||
            entry.path().stem().string().ends_with("_b")) continue;
        SparseCase test_case;
        test_case.name = entry.path().stem().string();
        test_case.matrix_path = entry.path();
        const auto right_hand_side = entry.path().parent_path() /
            (entry.path().stem().string() + "_b.mtx");
        if (std::filesystem::exists(right_hand_side)) {
            test_case.right_hand_side_path = right_hand_side;
        }
        cases.push_back(std::move(test_case));
    }
    std::sort(cases.begin(), cases.end(), [](const auto& left, const auto& right) {
        return left.name < right.name;
    });
    return cases;
}

std::vector<double> deterministic_reference_solution(std::size_t size) {
    std::vector<double> result(size);
    for (std::size_t index = 0; index < size; ++index) {
        result[index] = 1.0 + static_cast<double>((index * 17U + 3U) % 13U) / 13.0;
    }
    return result;
}

double relative_residual(
    const SparseMatrix& matrix,
    const std::vector<double>& solution,
    const std::vector<double>& right_hand_side) {
    const auto product = matrix.multiply(solution);
    if (product.size() != right_hand_side.size()) {
        throw std::invalid_argument("residual shape mismatch");
    }
    std::vector<double> residual(product.size());
    for (std::size_t index = 0; index < product.size(); ++index) {
        residual[index] = product[index] - right_hand_side[index];
    }
    return infinity_norm(residual) / std::max(1.0, infinity_norm(right_hand_side));
}

double relative_error(
    const std::vector<double>& actual,
    const std::vector<double>& expected) {
    if (actual.size() != expected.size()) throw std::invalid_argument("error shape mismatch");
    std::vector<double> error(actual.size());
    for (std::size_t index = 0; index < actual.size(); ++index) {
        error[index] = actual[index] - expected[index];
    }
    return infinity_norm(error) / std::max(1.0, infinity_norm(expected));
}

void write_sparse_case_result(
    const SparseCaseResult& result, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write sparse benchmark checkpoint");
    output << std::setprecision(17)
           << "SMAVE_SPARSE_CASE 2\n"
           << "NAME " << std::quoted(result.test_case.name) << '\n'
           << "MATRIX " << std::quoted(result.test_case.matrix_path.string()) << '\n'
           << "RHS " << std::quoted(result.test_case.right_hand_side_path.string()) << '\n'
           << "SHAPE " << result.rows << ' ' << result.columns << ' '
           << result.nonzeros << '\n'
           << "TYPE " << std::quoted(result.value_kind) << ' '
           << std::quoted(result.symmetry) << ' '
           << std::quoted(result.right_hand_side_kind) << '\n'
           << "ROUTING " << std::quoted(result.equation_family) << ' '
           << std::quoted(result.solve_plan_id) << ' '
           << result.backend_chain.size();
    for (const auto& backend : result.backend_chain) {
        output << ' ' << std::quoted(backend);
    }
    output << '\n';
    write_observation(output, result.smave);
    output << "REFERENCES " << result.references.size() << '\n';
    for (const auto& reference : result.references) write_observation(output, reference);
    output << "CORRECTNESS_AGREEMENT " << result.correctness_agreement << "\nEND\n";
}

SparseCaseResult read_sparse_case_result(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read sparse benchmark checkpoint");
    auto tag = [&](const std::string& expected) {
        std::string actual;
        input >> actual;
        if (!input || actual != expected) {
            throw std::runtime_error("invalid sparse checkpoint: expected " + expected);
        }
    };
    SparseCaseResult result;
    tag("SMAVE_SPARSE_CASE");
    int schema{};
    input >> schema;
    if (schema != 1 && schema != 2) {
        throw std::runtime_error("unsupported sparse checkpoint schema");
    }
    tag("NAME"); input >> std::quoted(result.test_case.name);
    std::string path_text;
    tag("MATRIX"); input >> std::quoted(path_text); result.test_case.matrix_path = path_text;
    tag("RHS"); input >> std::quoted(path_text); result.test_case.right_hand_side_path = path_text;
    tag("SHAPE"); input >> result.rows >> result.columns >> result.nonzeros;
    tag("TYPE"); input >> std::quoted(result.value_kind) >> std::quoted(result.symmetry)
                       >> std::quoted(result.right_hand_side_kind);
    if (schema >= 2) {
        tag("ROUTING");
        input >> std::quoted(result.equation_family)
              >> std::quoted(result.solve_plan_id);
        std::size_t backend_count{};
        input >> backend_count;
        result.backend_chain.resize(backend_count);
        for (auto& backend : result.backend_chain) input >> std::quoted(backend);
    }
    tag("OBSERVATION"); result.smave = read_observation(input);
    tag("REFERENCES");
    std::size_t count{};
    input >> count;
    result.references.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        tag("OBSERVATION");
        result.references.push_back(read_observation(input));
    }
    tag("CORRECTNESS_AGREEMENT"); input >> result.correctness_agreement;
    tag("END");
    return result;
}

}  // namespace smave::benchmark
