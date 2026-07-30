#include "smave/compiler.hpp"
#include "smave/block_graph.hpp"
#include "smave/competition.hpp"
#include "smave/continuous.hpp"
#include "smave/cosimulation.hpp"
#include "smave/dae.hpp"
#include "smave/dae_learning.hpp"
#include "smave/data_registry.hpp"
#include "smave/device.hpp"
#include "smave/config.hpp"
#include "smave/embedding.hpp"
#include "smave/expression.hpp"
#include "smave/family_routing.hpp"
#include "smave/fmi.hpp"
#include "smave/expert.hpp"
#include "smave/ir.hpp"
#include "smave/learning.hpp"
#include "smave/model_group.hpp"
#include "smave/hybrid.hpp"
#include "smave/operator.hpp"
#include "smave/linear.hpp"
#include "smave/routing.hpp"
#include "smave/release.hpp"
#include "smave/runtime.hpp"
#include "smave/tensor.hpp"
#include "smave/validation.hpp"
#include "smave/verification.hpp"

#include <zlib.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string legacy_block_fingerprint(
    const smave::BlockIR& block, const smave::ModelIR& model) {
    const auto find_variable = [&](const std::string& name) -> const smave::VariableIR& {
        return *std::find_if(
            model.variables.begin(), model.variables.end(),
            [&](const auto& variable) { return variable.name == name; });
    };
    const auto find_equation = [&](const std::string& id) -> const smave::EquationIR& {
        return *std::find_if(
            model.equations.begin(), model.equations.end(),
            [&](const auto& equation) { return equation.id == id; });
    };
    std::ostringstream contract;
    contract << smave::kLegacyIrSchemaVersion << '|' << block.mode << '|'
             << block.original_solver << '|' << block.linear << '|';
    for (const auto& name : block.unknowns) {
        const auto& variable = find_variable(name);
        contract << variable.name << ':' << variable.nominal << ':' << variable.unit << ';';
    }
    contract << '|';
    for (const auto& name : block.contexts) {
        const auto& variable = find_variable(name);
        contract << variable.name << ':' << variable.nominal << ':' << variable.unit << ';';
    }
    contract << '|';
    for (const auto& id : block.equation_ids) contract << find_equation(id).residual << ';';
    contract << '|';
    for (std::size_t row = 0; row < block.jacobian_sparsity.row_count; ++row) {
        for (std::size_t column = 0;
             column < block.jacobian_sparsity.column_count; ++column) {
            contract << block.jacobian_sparsity.contains(row, column);
        }
        contract << ';';
    }
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char character : contract.str()) {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

void append_u16(std::string& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<char>(value & 0xffU));
    bytes.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

void append_u32(std::string& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32U; shift += 8U) {
        bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

void write_stored_zip(
    const std::filesystem::path& path,
    const std::vector<std::pair<std::string, std::string>>& entries,
    bool corrupt_descriptor_crc = false) {
    struct CentralRecord {
        std::string name;
        std::uint32_t crc{};
        std::uint32_t size{};
        std::uint32_t offset{};
    };
    std::string bytes;
    std::vector<CentralRecord> central;
    for (const auto& [name, contents] : entries) {
        CentralRecord record;
        record.name = name;
        record.crc = crc32(
            0U, reinterpret_cast<const Bytef*>(contents.data()),
            static_cast<uInt>(contents.size()));
        if (corrupt_descriptor_crc && name == "modelDescription.xml") record.crc ^= 1U;
        record.size = static_cast<std::uint32_t>(contents.size());
        record.offset = static_cast<std::uint32_t>(bytes.size());
        append_u32(bytes, 0x04034b50U);
        append_u16(bytes, 20U); append_u16(bytes, 0U); append_u16(bytes, 0U);
        append_u16(bytes, 0U); append_u16(bytes, 0U);
        append_u32(bytes, record.crc); append_u32(bytes, record.size);
        append_u32(bytes, record.size); append_u16(bytes, static_cast<std::uint16_t>(name.size()));
        append_u16(bytes, 0U); bytes += name; bytes += contents;
        central.push_back(std::move(record));
    }
    const auto central_offset = static_cast<std::uint32_t>(bytes.size());
    for (const auto& record : central) {
        append_u32(bytes, 0x02014b50U);
        append_u16(bytes, 20U); append_u16(bytes, 20U); append_u16(bytes, 0U);
        append_u16(bytes, 0U); append_u16(bytes, 0U); append_u16(bytes, 0U);
        append_u32(bytes, record.crc); append_u32(bytes, record.size);
        append_u32(bytes, record.size);
        append_u16(bytes, static_cast<std::uint16_t>(record.name.size()));
        append_u16(bytes, 0U); append_u16(bytes, 0U); append_u16(bytes, 0U);
        append_u16(bytes, 0U); append_u32(bytes, 0U); append_u32(bytes, record.offset);
        bytes += record.name;
    }
    const auto central_size = static_cast<std::uint32_t>(bytes.size()) - central_offset;
    append_u32(bytes, 0x06054b50U); append_u16(bytes, 0U); append_u16(bytes, 0U);
    append_u16(bytes, static_cast<std::uint16_t>(central.size()));
    append_u16(bytes, static_cast<std::uint16_t>(central.size()));
    append_u32(bytes, central_size); append_u32(bytes, central_offset); append_u16(bytes, 0U);
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::filesystem::path temporary_directory() {
    const auto process_id = [] {
#if defined(_WIN32)
        return static_cast<unsigned long>(GetCurrentProcessId());
#else
        return static_cast<unsigned long>(::getpid());
#endif
    };
    const auto path = std::filesystem::path(SMAVE_TEST_WORK_DIR) /
        ("process-" + std::to_string(process_id()));
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

void test_expression() {
    const smave::Expression expression("sqrt(x^2) + sin(pi / 2)");
    require(std::abs(expression.evaluate({{"x", -3.0}}) - 4.0) < 1.0e-12,
            "expression evaluation failed");
    bool rejected = false;
    try { smave::Expression unsafe("system(x)"); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "unsafe function was accepted");
    const auto coefficients = smave::Expression("4*x - y + p + sin(p)")
        .constant_linear_coefficients({"x", "y"});
    require(coefficients.has_value() && coefficients->size() == 2 &&
                std::abs((*coefficients)[0] - 4.0) < 1.0e-12 &&
                std::abs((*coefficients)[1] + 1.0) < 1.0e-12,
            "constant linear coefficient proof rejected a safe expression");
    require(!smave::Expression("p*x + y").constant_linear_coefficients({"x", "y"}),
            "parameter-dependent unknown coefficient was marked cacheable");
    require(!smave::Expression("x*x + y").constant_linear_coefficients({"x", "y"}),
            "nonlinear unknown product was marked cacheable");
    require(!smave::Expression("sin(x) + y").constant_linear_coefficients({"x", "y"}),
            "nonlinear unknown function was marked cacheable");
    const smave::Expression differentiated("sin(x)*exp(y) + x^3/y");
    const auto derivative = differentiated.directional_derivative(
        {{"x", 2.0}, {"y", 4.0}}, {{"x", 1.5}, {"y", -0.25}});
    const double expected =
        (std::cos(2.0) * std::exp(4.0) + 3.0) * 1.5 +
        (std::sin(2.0) * std::exp(4.0) - 0.5) * -0.25;
    require(derivative.has_value() && std::abs(*derivative - expected) < 1.0e-10,
            "expression forward directional AD is incorrect");
    require(!smave::Expression("abs(x)").directional_derivative(
                {{"x", 0.0}}, {{"x", 1.0}}),
            "nondifferentiable abs point did not request fallback");
}

void test_fmi_blackbox_import(const std::filesystem::path& root) {
    const auto fmi3 = root / "fmi3";
    std::filesystem::create_directories(fmi3);
    std::ofstream(fmi3 / "modelDescription.xml")
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"ThermalUnit\" "
        << "instantiationToken=\"token-3\" generationTool=\"SMAVE fixture\" "
        << "variableNamingConvention=\"structured\" numberOfEventIndicators=\"2\">\n"
        << "<ModelExchange modelIdentifier=\"thermal_me\" "
        << "providesDirectionalDerivatives=\"true\"/>\n"
        << "<CoSimulation modelIdentifier=\"thermal_cs\" "
        << "canGetAndSetFMUState=\"true\"/>\n"
        << "<DefaultExperiment startTime=\"0\" stopTime=\"10\" tolerance=\"1e-6\"/>\n"
        << "<ModelVariables>\n"
        << "<Float64 name=\"u\" valueReference=\"1\" causality=\"input\" "
        << "variability=\"continuous\" unit=\"K\" start=\"293.15\"/>\n"
        << "<Float64 name=\"y\" valueReference=\"2\" causality=\"output\" "
        << "variability=\"continuous\"><Dimension start=\"3\"/></Float64>\n"
        << "<Clock name=\"tick\" valueReference=\"3\" causality=\"input\" "
        << "variability=\"discrete\" intervalVariability=\"triggered\"/>\n"
        << "</ModelVariables></fmiModelDescription>\n";
    const auto first = smave::import_fmu(fmi3);
    require(first.fmi_version == "3.0" && first.model_name == "ThermalUnit" &&
                first.interfaces.size() == 2 && first.variables.size() == 3 &&
                first.variables[1].dimensions == 1 &&
                first.variables[1].dimension_descriptors.size() == 1 &&
                first.variables[1].dimension_descriptors[0].fixed_extent == 3 &&
                first.number_of_event_indicators == 2 &&
                !first.host_binary_candidate_available && first.trajectory_proxy_allowed &&
                first.differential_test_allowed &&
                !first.equation_level_validation_allowed && !first.direct_expert_allowed,
            "FMI 3 blackbox metadata or permission boundary was parsed incorrectly");
    require(std::any_of(
                first.warnings.begin(), first.warnings.end(), [](const std::string& warning) {
                    return warning.find("directional derivatives are unavailable") !=
                        std::string::npos;
                }),
            "missing FMI capability did not produce an explicit degradation warning");
    std::string extension;
    if (first.host_platform.ends_with("darwin")) extension = ".dylib";
    if (first.host_platform.ends_with("linux")) extension = ".so";
    if (first.host_platform.ends_with("windows")) extension = ".dll";
    if (!extension.empty()) {
        const auto binaries = fmi3 / "binaries" / first.host_platform;
        std::filesystem::create_directories(binaries);
        std::ofstream(binaries / ("thermal_me" + extension)) << "not executed";
        std::ofstream(binaries / ("thermal_cs" + extension)) << "not executed";
        const auto with_binaries = smave::import_fmu(fmi3);
        require(with_binaries.host_binary_candidate_available,
                "matching FMI interface binaries were not inventoried");
    }
    std::ifstream descriptor_input(fmi3 / "modelDescription.xml");
    std::ostringstream descriptor_stream;
    descriptor_stream << descriptor_input.rdbuf();
    const auto archive_path = root / "thermal.fmu";
    std::vector<std::pair<std::string, std::string>> archive_entries{{
        "modelDescription.xml", descriptor_stream.str()}};
    if (!extension.empty()) {
        archive_entries.push_back({
            "binaries/" + first.host_platform + "/thermal_me" + extension,
            "not executed"});
        archive_entries.push_back({
            "binaries/" + first.host_platform + "/thermal_cs" + extension,
            "not executed"});
    }
    write_stored_zip(archive_path, archive_entries);
    const auto archive_model = smave::import_fmu(archive_path);
    require(archive_model.model_name == "ThermalUnit" &&
                (extension.empty() || archive_model.host_binary_candidate_available) &&
                std::none_of(
                    archive_model.warnings.begin(), archive_model.warnings.end(),
                    [](const std::string& warning) {
                        return warning.find("directory import hashes") != std::string::npos;
                    }),
            "valid FMU ZIP archive was not imported with package-level identity");

    const auto clock_array = root / "fmi3-clock-array";
    std::filesystem::create_directories(clock_array);
    std::ofstream(clock_array / "modelDescription.xml")
        << "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"InvalidClockArray\" "
        << "instantiationToken=\"clock-array-token\">"
        << "<ModelExchange modelIdentifier=\"invalid_clock_array\"/>"
        << "<ModelVariables><Clock name=\"ticks\" valueReference=\"1\" "
        << "causality=\"input\" variability=\"discrete\" "
        << "intervalVariability=\"triggered\"><Dimension start=\"3\"/>"
        << "</Clock></ModelVariables></fmiModelDescription>";
    bool rejected_clock_array{};
    try {
        (void)smave::import_fmu(clock_array);
    } catch (const std::invalid_argument&) {
        rejected_clock_array = true;
    }
    require(rejected_clock_array, "FMI 3 Clock array was accepted");

    const auto invalid_clock_metadata = root / "fmi3-invalid-clock-metadata";
    std::filesystem::create_directories(invalid_clock_metadata);
    std::ofstream(invalid_clock_metadata / "modelDescription.xml")
        << "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"InvalidClock\" "
        << "instantiationToken=\"invalid-clock-token\">"
        << "<ModelExchange modelIdentifier=\"invalid_clock\"/>"
        << "<ModelVariables><Clock name=\"tick\" valueReference=\"1\" "
        << "causality=\"input\" variability=\"discrete\"/>"
        << "</ModelVariables></fmiModelDescription>";
    bool rejected_invalid_clock_metadata{};
    try {
        (void)smave::import_fmu(invalid_clock_metadata);
    } catch (const std::invalid_argument&) {
        rejected_invalid_clock_metadata = true;
    }
    require(rejected_invalid_clock_metadata,
            "FMI 3 Clock without intervalVariability was accepted");

    const auto unsafe_archive = root / "unsafe.fmu";
    write_stored_zip(unsafe_archive, {
        {"modelDescription.xml", descriptor_stream.str()}, {"../escape", "payload"}});
    bool rejected_unsafe{};
    try {
        (void)smave::import_fmu(unsafe_archive);
    } catch (const std::invalid_argument&) {
        rejected_unsafe = true;
    }
    require(rejected_unsafe, "FMU ZIP path traversal was accepted");

    const auto corrupt_archive = root / "corrupt.fmu";
    write_stored_zip(
        corrupt_archive, {{"modelDescription.xml", descriptor_stream.str()}}, true);
    bool rejected_crc{};
    try {
        (void)smave::import_fmu(corrupt_archive);
    } catch (const std::invalid_argument&) {
        rejected_crc = true;
    }
    require(rejected_crc, "FMU descriptor CRC corruption was accepted");
    const auto ir_path = root / "thermal.fmi.ir";
    first.write(ir_path);
    const auto restored = smave::FmiBlackboxIR::read(ir_path);
    require(restored.source_hash == first.source_hash &&
                restored.interfaces.front().model_identifier == "thermal_me" &&
                !restored.equation_level_validation_allowed,
            "FMI blackbox IR roundtrip widened permissions or lost identity");

    const auto fmi2 = root / "fmi2";
    std::filesystem::create_directories(fmi2);
    std::ofstream(fmi2 / "modelDescription.xml")
        << "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"LegacyUnit\" "
        << "guid=\"legacy-guid\" numberOfEventIndicators=\"1\">"
        << "<CoSimulation modelIdentifier=\"legacy_cs\" canGetAndSetFMUstate=\"true\"/>"
        << "<ModelVariables>"
        << "<ScalarVariable name=\"gain\" valueReference=\"7\" causality=\"parameter\" "
        << "variability=\"tunable\" initial=\"exact\"><Real unit=\"1\" start=\"2\"/>"
        << "</ScalarVariable></ModelVariables></fmiModelDescription>";
    const auto legacy = smave::import_fmu(fmi2);
    require(legacy.fmi_version == "2.0" && legacy.instantiation_token == "legacy-guid" &&
                legacy.interfaces.size() == 1 && legacy.variables.size() == 1 &&
                legacy.variables.front().type == "Real" &&
                legacy.variables.front().start == "2",
            "FMI 2 ScalarVariable metadata was parsed incorrectly");

    const std::string native_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeSmoke\" "
        "instantiationToken=\"native-token\" numberOfEventIndicators=\"0\">"
        "<CoSimulation modelIdentifier=\"NativeSmoke\" "
        "canGetAndSetFMUState=\"true\" canSerializeFMUState=\"true\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\" "
        "variability=\"tunable\" start=\"2\"/>"
        "<Float64 name=\"u\" valueReference=\"2\" causality=\"input\"/>"
        "<Float64 name=\"y\" valueReference=\"3\" causality=\"output\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto native_metadata_directory = root / "native-metadata";
    std::filesystem::create_directories(native_metadata_directory);
    std::ofstream(native_metadata_directory / "modelDescription.xml") << native_descriptor;
    const auto native_metadata = smave::import_fmu(native_metadata_directory);
    std::string native_extension;
    if (native_metadata.host_platform.ends_with("darwin")) native_extension = ".dylib";
    if (native_metadata.host_platform.ends_with("linux")) native_extension = ".so";
    if (native_metadata.host_platform.ends_with("windows")) native_extension = ".dll";
    require(!native_extension.empty(), "test platform lacks a native FMI library extension");
    std::ifstream native_binary_input(SMAVE_FMI_FIXTURE_PATH, std::ios::binary);
    std::ostringstream native_binary_bytes;
    native_binary_bytes << native_binary_input.rdbuf();
    const auto native = root / "native.fmu";
    write_stored_zip(native, {
        {"modelDescription.xml", native_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeSmoke" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_without_opt_in{};
    try {
        (void)smave::smoke_fmi3_co_simulation(
            native, 0.2, 0.1, {{"gain", 3.0}, {"u", 2.0}}, false);
    } catch (const std::invalid_argument&) {
        rejected_without_opt_in = true;
    }
    require(rejected_without_opt_in, "native FMU execution proceeded without explicit opt-in");
    const auto native_smoke = smave::smoke_fmi3_co_simulation(
        native, 0.2, 0.1, {{"gain", 3.0}, {"u", 2.0}}, true);
    require(native_smoke.success && native_smoke.samples.size() == 3 &&
                native_smoke.state_roundtrip_attempted &&
                native_smoke.state_roundtrip_passed &&
                native_smoke.state_serialization_attempted &&
                native_smoke.state_serialization_passed &&
                native_smoke.serialized_state_bytes == 32 &&
                native_smoke.maximum_state_replay_error == 0.0 &&
                native_smoke.samples.front().outputs.at("y") == 6.0 &&
                std::abs(native_smoke.samples.back().outputs.at("y") - 6.2) < 1.0e-12,
            "opt-in FMI 3 Co-Simulation lifecycle or state replay failed");

    auto corrupt_cs_descriptor = native_descriptor;
    corrupt_cs_descriptor.replace(
        corrupt_cs_descriptor.find("native-token"),
        std::string("native-token").size(),
        "invalid-serialized-state-token");
    const auto corrupt_cs = root / "native-corrupt-serialized-state.fmu";
    write_stored_zip(corrupt_cs, {
        {"modelDescription.xml", corrupt_cs_descriptor},
        {"binaries/" + native_metadata.host_platform +
             "/NativeSmoke" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_corrupt_cs_serialization{};
    try {
        (void)smave::smoke_fmi3_co_simulation(
            corrupt_cs, 0.2, 0.1, {{"gain", 3.0}, {"u", 2.0}}, true);
    } catch (const std::runtime_error&) {
        rejected_corrupt_cs_serialization = true;
    }
    require(rejected_corrupt_cs_serialization,
            "corrupt FMI 3 Co-Simulation serialized state was accepted");

    auto cs_serialization_without_state = native_descriptor;
    cs_serialization_without_state.erase(
        cs_serialization_without_state.find("canGetAndSetFMUState=\"true\" "),
        std::string("canGetAndSetFMUState=\"true\" ").size());
    const auto invalid_cs_capability = root / "native-cs-serialization-without-state.fmu";
    write_stored_zip(invalid_cs_capability, {
        {"modelDescription.xml", cs_serialization_without_state},
        {"binaries/" + native_metadata.host_platform +
             "/NativeSmoke" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_cs_serialization_without_state{};
    try {
        (void)smave::smoke_fmi3_co_simulation(
            invalid_cs_capability, 0.2, 0.1,
            {{"gain", 3.0}, {"u", 2.0}}, true);
    } catch (const std::invalid_argument&) {
        rejected_cs_serialization_without_state = true;
    }
    require(rejected_cs_serialization_without_state,
            "FMI 3 CS serialization capability without state capability was accepted");

    const std::string scheduled_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeScheduled\" "
        "instantiationToken=\"native-scheduled-token\">"
        "<ScheduledExecution modelIdentifier=\"NativeSmoke\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\" "
        "variability=\"tunable\" start=\"2\"/>"
        "<Float64 name=\"u\" valueReference=\"2\" causality=\"input\"/>"
        "<Float64 name=\"y\" valueReference=\"3\" causality=\"output\" "
        "variability=\"discrete\"/>"
        "<Clock name=\"tick\" valueReference=\"46\" causality=\"input\" "
        "variability=\"discrete\" intervalVariability=\"tunable\" priority=\"10\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto scheduled = root / "native-scheduled.fmu";
    write_stored_zip(scheduled, {
        {"modelDescription.xml", scheduled_descriptor},
        {"binaries/" + native_metadata.host_platform +
             "/NativeSmoke" + native_extension,
         native_binary_bytes.str()}});
    const auto scheduled_smoke = smave::smoke_fmi3_scheduled_execution(
        scheduled, 0.5, 0.25, {{"gain", 3.0}, {"u", 2.0}}, true);
    require(scheduled_smoke.success && scheduled_smoke.samples.size() == 3 &&
                scheduled_smoke.model_partition_activations == 3 &&
                scheduled_smoke.clock_update_callbacks == 3 &&
                scheduled_smoke.lock_preemption_callbacks == 3 &&
                scheduled_smoke.unlock_preemption_callbacks == 3 &&
                scheduled_smoke.partition_activation_order.size() == 3 &&
                scheduled_smoke.partition_activation_order.front().clock_name == "tick" &&
                scheduled_smoke.partition_activation_order.front().clock_value_reference == 46 &&
                scheduled_smoke.clock_intervals.at("tick") == 0.25 &&
                scheduled_smoke.clock_shifts.at("tick") == 0.0 &&
                scheduled_smoke.clock_priorities.at("tick") == 10 &&
                scheduled_smoke.samples.front().outputs.at("y") == 6.0 &&
                scheduled_smoke.samples.back().outputs.at("y") == 6.5,
            "FMI 3 Scheduled Execution lifecycle or callback contract failed");
    bool rejected_scheduled_interval{};
    try {
        (void)smave::smoke_fmi3_scheduled_execution(
            scheduled, 0.2, 0.1, {}, true);
    } catch (const std::invalid_argument&) {
        rejected_scheduled_interval = true;
    }
    require(rejected_scheduled_interval,
            "Scheduled Execution accepted a CLI interval that disagrees with the Clock");

    auto no_clock_descriptor = scheduled_descriptor;
    const auto clock_begin = no_clock_descriptor.find("<Clock ");
    const auto clock_end = no_clock_descriptor.find("/>", clock_begin);
    no_clock_descriptor.erase(clock_begin, clock_end + 2 - clock_begin);
    const auto scheduled_no_clock = root / "native-scheduled-no-clock.fmu";
    write_stored_zip(scheduled_no_clock, {
        {"modelDescription.xml", no_clock_descriptor},
        {"binaries/" + native_metadata.host_platform +
             "/NativeSmoke" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_scheduled_no_clock{};
    try {
        (void)smave::smoke_fmi3_scheduled_execution(
            scheduled_no_clock, 0.5, 0.25, {}, true);
    } catch (const std::invalid_argument&) {
        rejected_scheduled_no_clock = true;
    }
    require(rejected_scheduled_no_clock,
            "Scheduled Execution without an input Clock was accepted");

    auto invalid_preemption_descriptor = scheduled_descriptor;
    invalid_preemption_descriptor.replace(
        invalid_preemption_descriptor.find("native-scheduled-token"),
        std::string("native-scheduled-token").size(),
        "native-scheduled-invalid-preemption-token");
    const auto scheduled_invalid_preemption =
        root / "native-scheduled-invalid-preemption.fmu";
    write_stored_zip(scheduled_invalid_preemption, {
        {"modelDescription.xml", invalid_preemption_descriptor},
        {"binaries/" + native_metadata.host_platform +
             "/NativeSmoke" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_invalid_preemption{};
    try {
        (void)smave::smoke_fmi3_scheduled_execution(
            scheduled_invalid_preemption, 0.5, 0.25, {}, true);
    } catch (const std::runtime_error&) {
        rejected_invalid_preemption = true;
    }
    require(rejected_invalid_preemption,
            "Scheduled Execution accepted unbalanced preemption callbacks");

    const std::string cs_event_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeEvent\" "
        "instantiationToken=\"native-event-token\" numberOfEventIndicators=\"0\">"
        "<CoSimulation modelIdentifier=\"NativeEvent\" hasEventMode=\"true\" "
        "canGetAndSetFMUState=\"true\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\" "
        "variability=\"tunable\" start=\"2\"/>"
        "<Float64 name=\"u\" valueReference=\"2\" causality=\"input\"/>"
        "<Float64 name=\"y\" valueReference=\"3\" causality=\"output\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto native_event = root / "native-event.fmu";
    write_stored_zip(native_event, {
        {"modelDescription.xml", cs_event_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeEvent" + native_extension,
         native_binary_bytes.str()}});
    const auto event_smoke = smave::smoke_fmi3_co_simulation(
        native_event, 0.3, 0.1, {{"gain", 3.0}, {"u", 2.0}}, true);
    require(event_smoke.success && event_smoke.samples.size() == 4 &&
                event_smoke.event_mode_entries == 1 &&
                event_smoke.discrete_update_iterations == 2 &&
                event_smoke.state_roundtrip_passed &&
                event_smoke.maximum_state_replay_error == 0.0 &&
                std::abs(event_smoke.samples[2].outputs.at("y") - 36.2) < 1.0e-12 &&
                std::abs(event_smoke.samples[3].outputs.at("y") - 36.3) < 1.0e-12,
            "FMI 3 Co-Simulation event-mode fixed point or replay failed");

    auto undeclared_event_descriptor = cs_event_descriptor;
    const auto event_capability = undeclared_event_descriptor.find(" hasEventMode=\"true\"");
    undeclared_event_descriptor.erase(
        event_capability, std::string(" hasEventMode=\"true\"").size());
    const auto undeclared_event = root / "undeclared-event.fmu";
    write_stored_zip(undeclared_event, {
        {"modelDescription.xml", undeclared_event_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeEvent" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_undeclared_event{};
    try {
        (void)smave::smoke_fmi3_co_simulation(
            undeclared_event, 0.3, 0.1, {{"gain", 3.0}, {"u", 2.0}}, true);
    } catch (const std::runtime_error&) {
        rejected_undeclared_event = true;
    }
    require(rejected_undeclared_event,
            "Co-Simulation smoke accepted an event request without hasEventMode");

    const std::string early_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeEarly\" "
        "instantiationToken=\"native-early-event-token\" numberOfEventIndicators=\"0\">"
        "<CoSimulation modelIdentifier=\"NativeEarly\" hasEventMode=\"true\" "
        "canReturnEarlyAfterIntermediateUpdate=\"true\" "
        "canGetAndSetFMUState=\"true\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\" "
        "variability=\"tunable\" start=\"2\"/>"
        "<Float64 name=\"u\" valueReference=\"2\" causality=\"input\"/>"
        "<Float64 name=\"y\" valueReference=\"3\" causality=\"output\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto native_early = root / "native-early.fmu";
    write_stored_zip(native_early, {
        {"modelDescription.xml", early_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeEarly" + native_extension,
         native_binary_bytes.str()}});
    const auto early_smoke = smave::smoke_fmi3_co_simulation(
        native_early, 0.3, 0.1, {{"gain", 3.0}, {"u", 2.0}}, true);
    require(early_smoke.success && early_smoke.samples.size() == 4 &&
                early_smoke.do_step_calls == 6 && early_smoke.early_returns == 2 &&
                early_smoke.event_mode_entries == 2 &&
                early_smoke.discrete_update_iterations == 4 &&
                early_smoke.state_roundtrip_passed &&
                early_smoke.maximum_state_replay_error == 0.0 &&
                std::abs(early_smoke.samples[1].outputs.at("y") - 36.1) < 1.0e-12 &&
                std::abs(early_smoke.samples.back().outputs.at("y") - 36.3) < 1.0e-12,
            "FMI early-return event continuation or first-step replay failed");

    auto undeclared_early_descriptor = early_descriptor;
    const auto early_capability = undeclared_early_descriptor.find(
        " canReturnEarlyAfterIntermediateUpdate=\"true\"");
    undeclared_early_descriptor.erase(
        early_capability,
        std::string(" canReturnEarlyAfterIntermediateUpdate=\"true\"").size());
    const auto token_position = undeclared_early_descriptor.find("native-early-event-token");
    undeclared_early_descriptor.replace(
        token_position, std::string("native-early-event-token").size(),
        "native-undeclared-early-event-token");
    const auto undeclared_early = root / "undeclared-early.fmu";
    write_stored_zip(undeclared_early, {
        {"modelDescription.xml", undeclared_early_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeEarly" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_undeclared_early{};
    try {
        (void)smave::smoke_fmi3_co_simulation(
            undeclared_early, 0.3, 0.1, {{"gain", 3.0}, {"u", 2.0}}, true);
    } catch (const std::runtime_error&) {
        rejected_undeclared_early = true;
    }
    require(rejected_undeclared_early,
            "Co-Simulation smoke accepted undeclared early return");

    const std::string time_event_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeTimeEvent\" "
        "instantiationToken=\"native-time-event-token\" numberOfEventIndicators=\"0\">"
        "<CoSimulation modelIdentifier=\"NativeTimeEvent\" hasEventMode=\"true\" "
        "canGetAndSetFMUState=\"true\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\" "
        "variability=\"tunable\" start=\"2\"/>"
        "<Float64 name=\"u\" valueReference=\"2\" causality=\"input\"/>"
        "<Float64 name=\"y\" valueReference=\"3\" causality=\"output\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto native_time_event = root / "native-time-event.fmu";
    write_stored_zip(native_time_event, {
        {"modelDescription.xml", time_event_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeTimeEvent" + native_extension,
         native_binary_bytes.str()}});
    const auto time_event_smoke = smave::smoke_fmi3_co_simulation(
        native_time_event, 0.3, 0.1, {{"gain", 3.0}, {"u", 2.0}}, true);
    require(time_event_smoke.success && time_event_smoke.samples.size() == 4 &&
                time_event_smoke.do_step_calls == 5 &&
                time_event_smoke.time_event_splits == 1 &&
                time_event_smoke.time_events == 1 &&
                time_event_smoke.event_mode_entries == 3 &&
                time_event_smoke.discrete_update_iterations == 3 &&
                time_event_smoke.state_roundtrip_passed &&
                std::abs(time_event_smoke.samples[2].outputs.at("y") - 36.2) < 1.0e-12,
            "FMI nextEventTime scheduling or state replay failed");

    auto invalid_time_event_descriptor = time_event_descriptor;
    const auto time_token = invalid_time_event_descriptor.find("native-time-event-token");
    invalid_time_event_descriptor.replace(
        time_token, std::string("native-time-event-token").size(),
        "native-invalid-time-event-token");
    const auto invalid_time_event = root / "invalid-time-event.fmu";
    write_stored_zip(invalid_time_event, {
        {"modelDescription.xml", invalid_time_event_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeTimeEvent" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_past_time_event{};
    try {
        (void)smave::smoke_fmi3_co_simulation(
            invalid_time_event, 0.3, 0.1, {{"gain", 3.0}, {"u", 2.0}}, true);
    } catch (const std::runtime_error&) {
        rejected_past_time_event = true;
    }
    require(rejected_past_time_event,
            "Co-Simulation smoke accepted nextEventTime in the past");

    const std::string me_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeME\" "
        "instantiationToken=\"native-me-token\" numberOfEventIndicators=\"0\">"
        "<ModelExchange modelIdentifier=\"NativeME\" canGetAndSetFMUState=\"true\" "
        "canSerializeFMUState=\"true\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\"/>"
        "<Float64 name=\"x\" valueReference=\"2\" causality=\"output\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto native_me = root / "native-me.fmu";
    write_stored_zip(native_me, {
        {"modelDescription.xml", me_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeME" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_me_without_opt_in{};
    try {
        (void)smave::smoke_fmi3_model_exchange(
            native_me, 0.2, 0.1, {{"gain", 1.0}}, false);
    } catch (const std::invalid_argument&) {
        rejected_me_without_opt_in = true;
    }
    require(rejected_me_without_opt_in,
            "native ModelExchange execution proceeded without explicit opt-in");
    const auto me_smoke = smave::smoke_fmi3_model_exchange(
        native_me, 0.2, 0.1, {{"gain", 1.0}}, true);
    require(me_smoke.success && me_smoke.interface_kind == "ModelExchange" &&
                me_smoke.samples.size() == 3 && me_smoke.state_roundtrip_passed &&
                me_smoke.state_serialization_attempted &&
                me_smoke.state_serialization_passed &&
                me_smoke.serialized_state_bytes == 32 &&
                me_smoke.maximum_state_replay_error == 0.0 &&
                std::abs(me_smoke.samples.back().outputs.at("x") - std::exp(0.2)) < 2.0e-7,
            "FMI 3 ModelExchange RK4 lifecycle or state replay failed");

    const std::string invalid_serialization_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"InvalidSerialization\" "
        "instantiationToken=\"invalid-serialization-token\" numberOfEventIndicators=\"0\">"
        "<ModelExchange modelIdentifier=\"InvalidSerialization\" "
        "canSerializeFMUState=\"true\"/>"
        "<ModelVariables><Float64 name=\"x\" valueReference=\"2\" "
        "causality=\"output\"/></ModelVariables></fmiModelDescription>";
    const auto invalid_serialization = root / "invalid-serialization.fmu";
    write_stored_zip(invalid_serialization, {
        {"modelDescription.xml", invalid_serialization_descriptor},
        {"binaries/" + native_metadata.host_platform +
             "/InvalidSerialization" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_serialization_without_state{};
    try {
        (void)smave::smoke_fmi3_model_exchange(
            invalid_serialization, 0.2, 0.1, {}, true);
    } catch (const std::invalid_argument&) {
        rejected_serialization_without_state = true;
    }
    require(rejected_serialization_without_state,
            "FMI serialization capability without state capability was accepted");

    auto corrupt_serialization_descriptor = me_descriptor;
    const auto serialization_token =
        corrupt_serialization_descriptor.find("native-me-token");
    corrupt_serialization_descriptor.replace(
        serialization_token, std::string("native-me-token").size(),
        "invalid-serialized-state-token");
    const auto corrupt_serialization = root / "corrupt-serialization.fmu";
    write_stored_zip(corrupt_serialization, {
        {"modelDescription.xml", corrupt_serialization_descriptor},
        {"binaries/" + native_metadata.host_platform +
             "/NativeME" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_corrupt_serialization{};
    try {
        (void)smave::smoke_fmi3_model_exchange(
            corrupt_serialization, 0.2, 0.1, {{"gain", 1.0}}, true);
    } catch (const std::runtime_error&) {
        rejected_corrupt_serialization = true;
    }
    require(rejected_corrupt_serialization,
            "corrupt serialized FMI state was accepted");

    const std::string me_event_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeMEEvent\" "
        "instantiationToken=\"native-me-event-token\" numberOfEventIndicators=\"1\">"
        "<ModelExchange modelIdentifier=\"NativeMEEvent\" "
        "canGetAndSetFMUState=\"true\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\"/>"
        "<Float64 name=\"x\" valueReference=\"2\" causality=\"output\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto native_me_event = root / "native-me-event.fmu";
    write_stored_zip(native_me_event, {
        {"modelDescription.xml", me_event_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeMEEvent" + native_extension,
         native_binary_bytes.str()}});
    const auto me_event_smoke = smave::smoke_fmi3_model_exchange(
        native_me_event, 0.3, 0.1, {{"gain", 1.0}}, true);
    require(me_event_smoke.success && me_event_smoke.samples.size() == 4 &&
                me_event_smoke.model_exchange_roots == 2 &&
                me_event_smoke.event_mode_entries == 3 &&
                me_event_smoke.discrete_update_iterations == 3 &&
                me_event_smoke.state_roundtrip_passed &&
                me_event_smoke.maximum_state_replay_error == 0.0 &&
                me_event_smoke.samples[1].outputs.at("x") > 0.52 &&
                me_event_smoke.samples[1].outputs.at("x") < 0.53 &&
                me_event_smoke.samples.back().outputs.at("x") > 0.64 &&
                me_event_smoke.samples.back().outputs.at("x") < 0.65,
            "FMI 3 ModelExchange root localization, reset, or replay failed");

    const std::string me_grazing_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeMEGrazing\" "
        "instantiationToken=\"native-me-grazing-token\" numberOfEventIndicators=\"1\">"
        "<ModelExchange modelIdentifier=\"NativeMEGrazing\" "
        "canGetAndSetFMUState=\"true\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\"/>"
        "<Float64 name=\"x\" valueReference=\"2\" causality=\"output\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto native_me_grazing = root / "native-me-grazing.fmu";
    write_stored_zip(native_me_grazing, {
        {"modelDescription.xml", me_grazing_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeMEGrazing" +
             native_extension,
         native_binary_bytes.str()}});
    const auto me_grazing_smoke = smave::smoke_fmi3_model_exchange(
        native_me_grazing, 0.2, 0.1, {{"gain", 1.0}}, true);
    require(me_grazing_smoke.success && me_grazing_smoke.samples.size() == 3 &&
                me_grazing_smoke.model_exchange_roots == 2 &&
                me_grazing_smoke.model_exchange_grazing_roots == 2 &&
                me_grazing_smoke.state_roundtrip_passed &&
                me_grazing_smoke.maximum_state_replay_error == 0.0 &&
                me_grazing_smoke.samples[1].outputs.at("x") > 0.52 &&
                me_grazing_smoke.samples[1].outputs.at("x") < 0.53,
            "FMI 3 ModelExchange grazing root or replay failed");

    const std::string me_near_grazing_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeMENearGrazing\" "
        "instantiationToken=\"native-me-near-grazing-token\" "
        "numberOfEventIndicators=\"1\">"
        "<ModelExchange modelIdentifier=\"NativeMENearGrazing\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\"/>"
        "<Float64 name=\"x\" valueReference=\"2\" causality=\"output\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto native_me_near_grazing = root / "native-me-near-grazing.fmu";
    write_stored_zip(native_me_near_grazing, {
        {"modelDescription.xml", me_near_grazing_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeMENearGrazing" +
             native_extension,
         native_binary_bytes.str()}});
    const auto me_near_grazing_smoke = smave::smoke_fmi3_model_exchange(
        native_me_near_grazing, 0.2, 0.1, {{"gain", 1.0}}, true);
    require(me_near_grazing_smoke.success &&
                me_near_grazing_smoke.model_exchange_roots == 0 &&
                me_near_grazing_smoke.model_exchange_grazing_roots == 0 &&
                std::abs(me_near_grazing_smoke.samples.back().outputs.at("x") -
                    std::exp(0.2)) < 2.0e-7,
            "FMI 3 ModelExchange accepted a near-grazing indicator that did not reach zero");

    const std::string me_nominal_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeMENominal\" "
        "instantiationToken=\"native-me-nominal-token\" numberOfEventIndicators=\"1\">"
        "<ModelExchange modelIdentifier=\"NativeMENominal\" "
        "canGetAndSetFMUState=\"true\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\"/>"
        "<Float64 name=\"x\" valueReference=\"2\" causality=\"output\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto native_me_nominal = root / "native-me-nominal.fmu";
    write_stored_zip(native_me_nominal, {
        {"modelDescription.xml", me_nominal_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeMENominal" +
             native_extension,
         native_binary_bytes.str()}});
    const auto me_nominal_smoke = smave::smoke_fmi3_model_exchange(
        native_me_nominal, 0.2, 0.1, {{"gain", 1.0}}, true);
    require(me_nominal_smoke.success &&
                me_nominal_smoke.continuous_state_nominal_updates == 2 &&
                me_nominal_smoke.minimum_continuous_state_nominal == 2.0 &&
                me_nominal_smoke.maximum_continuous_state_nominal == 2.0 &&
                me_nominal_smoke.state_roundtrip_passed,
            "FMI 3 ModelExchange nominal change was not queried and replayed");

    const std::string me_invalid_nominal_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeMEInvalidNominal\" "
        "instantiationToken=\"native-me-invalid-nominal-token\" "
        "numberOfEventIndicators=\"1\">"
        "<ModelExchange modelIdentifier=\"NativeMEInvalidNominal\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\"/>"
        "<Float64 name=\"x\" valueReference=\"2\" causality=\"output\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto native_me_invalid_nominal = root / "native-me-invalid-nominal.fmu";
    write_stored_zip(native_me_invalid_nominal, {
        {"modelDescription.xml", me_invalid_nominal_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeMEInvalidNominal" +
             native_extension,
         native_binary_bytes.str()}});
    bool rejected_invalid_fmi3_nominal{};
    try {
        (void)smave::smoke_fmi3_model_exchange(
            native_me_invalid_nominal, 0.2, 0.1, {{"gain", 1.0}}, true);
    } catch (const std::runtime_error& error) {
        rejected_invalid_fmi3_nominal = std::string(error.what()).find(
            "finite and positive") != std::string::npos;
    }
    require(rejected_invalid_fmi3_nominal,
            "FMI 3 ModelExchange accepted a zero continuous-state nominal");

    const std::string me_multi_event_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeMEMultiEvent\" "
        "instantiationToken=\"native-me-multi-event-token\" "
        "numberOfEventIndicators=\"2\">"
        "<ModelExchange modelIdentifier=\"NativeMEMultiEvent\" "
        "canGetAndSetFMUState=\"true\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\"/>"
        "<Float64 name=\"x\" valueReference=\"2\" causality=\"output\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto native_me_multi_event = root / "native-me-multi-event.fmu";
    write_stored_zip(native_me_multi_event, {
        {"modelDescription.xml", me_multi_event_descriptor},
        {"binaries/" + native_metadata.host_platform +
             "/NativeMEMultiEvent" + native_extension,
         native_binary_bytes.str()}});
    const auto me_multi_event_smoke = smave::smoke_fmi3_model_exchange(
        native_me_multi_event, 0.3, 0.1, {{"gain", 1.0}}, true);
    require(me_multi_event_smoke.success && me_multi_event_smoke.samples.size() == 4 &&
                me_multi_event_smoke.model_exchange_roots == 4 &&
                me_multi_event_smoke.event_mode_entries == 5 &&
                me_multi_event_smoke.discrete_update_iterations == 5 &&
                me_multi_event_smoke.state_roundtrip_passed &&
                me_multi_event_smoke.maximum_state_replay_error == 0.0 &&
                std::abs(me_multi_event_smoke.samples[1].outputs.at("x") -
                         1.1051709168394022) < 1.0e-12,
            "FMI ModelExchange did not select and replay multiple roots in time order");

    const std::string me_time_event_descriptor =
        "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"NativeMETimeEvent\" "
        "instantiationToken=\"native-me-time-event-token\" "
        "numberOfEventIndicators=\"0\">"
        "<ModelExchange modelIdentifier=\"NativeMETimeEvent\" "
        "canGetAndSetFMUState=\"true\"/>"
        "<ModelVariables>"
        "<Float64 name=\"gain\" valueReference=\"1\" causality=\"parameter\"/>"
        "<Float64 name=\"x\" valueReference=\"2\" causality=\"output\"/>"
        "</ModelVariables></fmiModelDescription>";
    const auto native_me_time_event = root / "native-me-time-event.fmu";
    write_stored_zip(native_me_time_event, {
        {"modelDescription.xml", me_time_event_descriptor},
        {"binaries/" + native_metadata.host_platform +
             "/NativeMETimeEvent" + native_extension,
         native_binary_bytes.str()}});
    const auto me_time_event_smoke = smave::smoke_fmi3_model_exchange(
        native_me_time_event, 0.3, 0.1, {{"gain", 0.0}}, true);
    require(me_time_event_smoke.success && me_time_event_smoke.samples.size() == 4 &&
                me_time_event_smoke.time_event_splits == 2 &&
                me_time_event_smoke.time_events == 2 &&
                me_time_event_smoke.model_exchange_roots == 0 &&
                me_time_event_smoke.event_mode_entries == 3 &&
                me_time_event_smoke.discrete_update_iterations == 3 &&
                me_time_event_smoke.state_roundtrip_passed &&
                me_time_event_smoke.maximum_state_replay_error == 0.0 &&
                std::abs(me_time_event_smoke.samples[1].outputs.at("x") - 11.0) < 1.0e-12,
            "FMI ModelExchange nextEventTime scheduling or replay failed");

    auto invalid_me_time_event_descriptor = me_time_event_descriptor;
    const auto me_time_token = invalid_me_time_event_descriptor.find(
        "native-me-time-event-token");
    invalid_me_time_event_descriptor.replace(
        me_time_token, std::string("native-me-time-event-token").size(),
        "native-me-time-event-invalid-token");
    const auto invalid_me_time_event = root / "invalid-me-time-event.fmu";
    write_stored_zip(invalid_me_time_event, {
        {"modelDescription.xml", invalid_me_time_event_descriptor},
        {"binaries/" + native_metadata.host_platform +
             "/NativeMETimeEvent" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_invalid_me_time_event{};
    try {
        (void)smave::smoke_fmi3_model_exchange(
            invalid_me_time_event, 0.3, 0.1, {{"gain", 0.0}}, true);
    } catch (const std::runtime_error&) {
        rejected_invalid_me_time_event = true;
    }
    require(rejected_invalid_me_time_event,
            "FMI ModelExchange accepted an invalid nextEventTime");

    const auto event_me = root / "event-me.fmu";
    auto event_descriptor = me_descriptor;
    const auto indicator = event_descriptor.find("numberOfEventIndicators=\"0\"");
    event_descriptor.replace(indicator, std::string("numberOfEventIndicators=\"0\"").size(),
                             "numberOfEventIndicators=\"1\"");
    write_stored_zip(event_me, {
        {"modelDescription.xml", event_descriptor},
        {"binaries/" + native_metadata.host_platform + "/NativeME" + native_extension,
         native_binary_bytes.str()}});
    bool rejected_me_events{};
    try {
        (void)smave::smoke_fmi3_model_exchange(
            event_me, 0.2, 0.1, {{"gain", 1.0}}, true);
    } catch (const std::runtime_error&) {
        rejected_me_events = true;
    }
    require(rejected_me_events,
            "ModelExchange smoke accepted mismatched metadata/runtime event indicators");

    auto widened = restored;
    widened.direct_expert_allowed = true;
    bool rejected{};
    try {
        widened.validate();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "FMI blackbox IR accepted Direct expert permission");
}

void test_fmi2_native_co_simulation(const std::filesystem::path& root) {
    const std::string descriptor =
        "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"NativeFmi2\" "
        "guid=\"smave-fmi2-token\" numberOfEventIndicators=\"0\">"
        "<CoSimulation modelIdentifier=\"SMAVEFmi2\" "
        "canHandleVariableCommunicationStepSize=\"false\" "
        "canGetAndSetFMUstate=\"true\" canSerializeFMUstate=\"true\"/>"
        "<ModelVariables>"
        "<ScalarVariable name=\"gain\" valueReference=\"1\" causality=\"parameter\" "
        "variability=\"tunable\" initial=\"exact\"><Real start=\"2\"/></ScalarVariable>"
        "<ScalarVariable name=\"u\" valueReference=\"2\" causality=\"input\">"
        "<Real start=\"1\"/></ScalarVariable>"
        "<ScalarVariable name=\"y\" valueReference=\"3\" causality=\"output\">"
        "<Real/></ScalarVariable>"
        "</ModelVariables></fmiModelDescription>";
    const auto metadata_directory = root / "fmi2-native-metadata";
    std::filesystem::create_directories(metadata_directory);
    std::ofstream(metadata_directory / "modelDescription.xml") << descriptor;
    const auto metadata = smave::import_fmu(metadata_directory);
    const std::string legacy_platform = metadata.host_platform.ends_with("darwin")
        ? "darwin64"
        : metadata.host_platform.ends_with("linux") ? "linux64"
        : metadata.host_platform.ends_with("windows") ? "win64" : "";
    std::string extension;
    if (metadata.host_platform.ends_with("darwin")) extension = ".dylib";
    if (metadata.host_platform.ends_with("linux")) extension = ".so";
    if (metadata.host_platform.ends_with("windows")) extension = ".dll";
    require(!legacy_platform.empty() && !extension.empty(),
            "test platform lacks FMI 2 legacy binary naming");
    std::ifstream binary_input(SMAVE_FMI2_FIXTURE_PATH, std::ios::binary);
    std::ostringstream binary_bytes;
    binary_bytes << binary_input.rdbuf();
    const auto archive = root / "native-fmi2.fmu";
    write_stored_zip(archive, {
        {"modelDescription.xml", descriptor},
        {"binaries/" + legacy_platform + "/SMAVEFmi2" + extension,
         binary_bytes.str()}});
    const auto imported = smave::import_fmu(archive);
    require(imported.fmi_version == "2.0" &&
                imported.host_binary_candidate_available,
            "FMI 2 legacy host binary was not recognized");
    bool rejected_without_opt_in{};
    try {
        (void)smave::smoke_fmi2_co_simulation(
            archive, 0.2, 0.1, {{"gain", 3.0}, {"u", 2.0}}, false);
    } catch (const std::invalid_argument&) {
        rejected_without_opt_in = true;
    }
    require(rejected_without_opt_in,
            "FMI 2 native execution proceeded without explicit opt-in");
    const auto result = smave::smoke_fmi2_co_simulation(
        archive, 0.2, 0.1, {{"gain", 3.0}, {"u", 2.0}}, true);
    require(result.success && result.interface_kind == "CoSimulation" &&
                result.samples.size() == 3 && result.do_step_calls == 3 &&
                result.state_roundtrip_attempted &&
                result.state_roundtrip_passed &&
                result.state_serialization_attempted &&
                result.state_serialization_passed &&
                result.serialized_state_bytes == 32 &&
                result.maximum_state_replay_error == 0.0 &&
                result.samples.front().outputs.at("y") == 6.0 &&
                std::abs(result.samples.back().outputs.at("y") - 6.2) < 1.0e-12,
            "FMI 2 native lifecycle or state replay failed");

    auto invalid_serialized_descriptor = descriptor;
    invalid_serialized_descriptor.replace(
        invalid_serialized_descriptor.find("smave-fmi2-token"),
        std::string("smave-fmi2-token").size(),
        "smave-fmi2-invalid-serialized-token");
    const auto invalid_serialized_archive = root / "native-fmi2-invalid-serialized.fmu";
    write_stored_zip(invalid_serialized_archive, {
        {"modelDescription.xml", invalid_serialized_descriptor},
        {"binaries/" + legacy_platform + "/SMAVEFmi2" + extension,
         binary_bytes.str()}});
    bool rejected_invalid_serialized{};
    try {
        (void)smave::smoke_fmi2_co_simulation(
            invalid_serialized_archive, 0.2, 0.1,
            {{"gain", 3.0}, {"u", 2.0}}, true);
    } catch (const std::runtime_error&) {
        rejected_invalid_serialized = true;
    }
    require(rejected_invalid_serialized,
            "corrupt FMI 2 Co-Simulation serialized state was accepted");

    auto serialization_without_state_descriptor = descriptor;
    serialization_without_state_descriptor.erase(
        serialization_without_state_descriptor.find(
            "canGetAndSetFMUstate=\"true\" "),
        std::string("canGetAndSetFMUstate=\"true\" ").size());
    const auto serialization_without_state_archive =
        root / "native-fmi2-cs-serialization-without-state.fmu";
    write_stored_zip(serialization_without_state_archive, {
        {"modelDescription.xml", serialization_without_state_descriptor},
        {"binaries/" + legacy_platform + "/SMAVEFmi2" + extension,
         binary_bytes.str()}});
    bool rejected_serialization_without_state{};
    try {
        (void)smave::smoke_fmi2_co_simulation(
            serialization_without_state_archive, 0.2, 0.1,
            {{"gain", 3.0}, {"u", 2.0}}, true);
    } catch (const std::invalid_argument&) {
        rejected_serialization_without_state = true;
    }
    require(rejected_serialization_without_state,
            "FMI 2 CS serialization capability without state capability was accepted");
    bool rejected_by_fmi3_executor{};
    try {
        (void)smave::smoke_fmi3_co_simulation(
            archive, 0.2, 0.1, {{"gain", 3.0}, {"u", 2.0}}, true);
    } catch (const std::invalid_argument&) {
        rejected_by_fmi3_executor = true;
    }
    require(rejected_by_fmi3_executor,
            "FMI 3 executor accepted FMI 2 metadata");
}

void test_fmi2_co_simulation_discard_event(const std::filesystem::path& root) {
    const std::string descriptor =
        "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"NativeFmi2Event\" "
        "guid=\"smave-fmi2-event-token\" numberOfEventIndicators=\"0\">"
        "<CoSimulation modelIdentifier=\"SMAVEFmi2Event\" "
        "canHandleVariableCommunicationStepSize=\"true\" "
        "canGetAndSetFMUstate=\"true\"/>"
        "<ModelVariables>"
        "<ScalarVariable name=\"gain\" valueReference=\"1\" causality=\"parameter\">"
        "<Real start=\"3\"/></ScalarVariable>"
        "<ScalarVariable name=\"u\" valueReference=\"2\" causality=\"input\">"
        "<Real start=\"2\"/></ScalarVariable>"
        "<ScalarVariable name=\"y\" valueReference=\"3\" causality=\"output\">"
        "<Real/></ScalarVariable>"
        "</ModelVariables></fmiModelDescription>";
    const auto metadata_directory = root / "fmi2-event-metadata";
    std::filesystem::create_directories(metadata_directory);
    std::ofstream(metadata_directory / "modelDescription.xml") << descriptor;
    const auto metadata = smave::import_fmu(metadata_directory);
    const std::string legacy_platform = metadata.host_platform.ends_with("darwin")
        ? "darwin64"
        : metadata.host_platform.ends_with("linux") ? "linux64"
        : metadata.host_platform.ends_with("windows") ? "win64" : "";
    std::string extension;
    if (metadata.host_platform.ends_with("darwin")) extension = ".dylib";
    if (metadata.host_platform.ends_with("linux")) extension = ".so";
    if (metadata.host_platform.ends_with("windows")) extension = ".dll";
    std::ifstream binary_input(SMAVE_FMI2_EVENT_FIXTURE_PATH, std::ios::binary);
    std::ostringstream binary_bytes;
    binary_bytes << binary_input.rdbuf();
    const auto archive = root / "native-fmi2-event.fmu";
    write_stored_zip(archive, {
        {"modelDescription.xml", descriptor},
        {"binaries/" + legacy_platform + "/SMAVEFmi2Event" + extension,
         binary_bytes.str()}});
    const auto result = smave::smoke_fmi2_co_simulation(
        archive, 0.3, 0.1, {{"gain", 3.0}, {"u", 2.0}}, true, 25);
    require(result.success && result.samples.size() == 4 &&
                result.discard_recoveries == 1 &&
                result.do_step_calls == 5 && result.state_roundtrip_passed &&
                std::abs(result.samples[1].outputs.at("y") - 6.1) < 1.0e-12 &&
                std::abs(result.samples[2].outputs.at("y") - 16.2) < 1.0e-12 &&
                std::abs(result.samples[3].outputs.at("y") - 16.3) < 1.0e-12,
            "FMI 2 Co-Simulation Discard event did not continue to the communication point");

    const auto no_progress_descriptor = std::string(descriptor).replace(
        descriptor.find("smave-fmi2-event-token"),
        std::string("smave-fmi2-event-token").size(),
        "smave-fmi2-event-token-no-progress");
    const auto no_progress_archive = root / "native-fmi2-event-no-progress.fmu";
    write_stored_zip(no_progress_archive, {
        {"modelDescription.xml", no_progress_descriptor},
        {"binaries/" + legacy_platform + "/SMAVEFmi2Event" + extension,
         binary_bytes.str()}});
    bool no_progress_rejected{};
    try {
        (void)smave::smoke_fmi2_co_simulation(
            no_progress_archive, 0.1, 0.1, {}, true);
    } catch (const std::runtime_error& error) {
        no_progress_rejected = std::string(error.what()).find(
            "bounded interior progress") != std::string::npos;
    }
    require(no_progress_rejected,
            "FMI 2 Co-Simulation accepted Discard without forward progress");

    auto undeclared_descriptor = descriptor;
    const auto variable_step_capability = undeclared_descriptor.find(
        "canHandleVariableCommunicationStepSize=\"true\"");
    undeclared_descriptor.replace(
        variable_step_capability,
        std::string("canHandleVariableCommunicationStepSize=\"true\"").size(),
        "canHandleVariableCommunicationStepSize=\"false\"");
    const auto undeclared_archive = root / "native-fmi2-event-undeclared.fmu";
    write_stored_zip(undeclared_archive, {
        {"modelDescription.xml", undeclared_descriptor},
        {"binaries/" + legacy_platform + "/SMAVEFmi2Event" + extension,
         binary_bytes.str()}});
    bool undeclared_rejected{};
    try {
        (void)smave::smoke_fmi2_co_simulation(
            undeclared_archive, 0.2, 0.1, {}, true);
    } catch (const std::runtime_error& error) {
        undeclared_rejected = std::string(error.what()).find(
            "canHandleVariableCommunicationStepSize") != std::string::npos;
    }
    require(undeclared_rejected,
            "FMI 2 Co-Simulation accepted Discard without variable-step capability");
}

void test_fmi2_co_simulation_pending(const std::filesystem::path& root) {
    const std::string descriptor =
        "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"NativeFmi2Async\" "
        "guid=\"smave-fmi2-async-token\" numberOfEventIndicators=\"0\">"
        "<CoSimulation modelIdentifier=\"SMAVEFmi2Async\" "
        "canRunAsynchronuously=\"true\" canGetAndSetFMUstate=\"true\"/>"
        "<ModelVariables>"
        "<ScalarVariable name=\"gain\" valueReference=\"1\" causality=\"parameter\">"
        "<Real start=\"3\"/></ScalarVariable>"
        "<ScalarVariable name=\"u\" valueReference=\"2\" causality=\"input\">"
        "<Real start=\"2\"/></ScalarVariable>"
        "<ScalarVariable name=\"y\" valueReference=\"3\" causality=\"output\">"
        "<Real/></ScalarVariable>"
        "</ModelVariables></fmiModelDescription>";
    const auto metadata_directory = root / "fmi2-async-metadata";
    std::filesystem::create_directories(metadata_directory);
    std::ofstream(metadata_directory / "modelDescription.xml") << descriptor;
    const auto metadata = smave::import_fmu(metadata_directory);
    const std::string legacy_platform = metadata.host_platform.ends_with("darwin")
        ? "darwin64"
        : metadata.host_platform.ends_with("linux") ? "linux64"
        : metadata.host_platform.ends_with("windows") ? "win64" : "";
    std::string extension;
    if (metadata.host_platform.ends_with("darwin")) extension = ".dylib";
    if (metadata.host_platform.ends_with("linux")) extension = ".so";
    if (metadata.host_platform.ends_with("windows")) extension = ".dll";
    std::ifstream binary_input(SMAVE_FMI2_ASYNC_FIXTURE_PATH, std::ios::binary);
    std::ostringstream binary_bytes;
    binary_bytes << binary_input.rdbuf();
    const auto binary_entry =
        "binaries/" + legacy_platform + "/SMAVEFmi2Async" + extension;
    const auto write_archive = [&](const std::filesystem::path& archive,
                                   const std::string& xml) {
        write_stored_zip(archive, {
            {"modelDescription.xml", xml},
            {binary_entry, binary_bytes.str()}});
    };

    const auto archive = root / "native-fmi2-async.fmu";
    write_archive(archive, descriptor);
    const auto result = smave::smoke_fmi2_co_simulation(
        archive, 0.3, 0.1, {{"gain", 3.0}, {"u", 2.0}}, true, 25);
    require(result.success && result.samples.size() == 4 &&
                result.pending_steps == 1 &&
                result.step_finished_callbacks == 1 &&
                result.cross_thread_callbacks == 1 &&
                result.cancelled_steps == 0 &&
                result.asynchronous_timeout_ms == 25 &&
                result.do_step_calls == 4 &&
                result.state_roundtrip_passed &&
                std::abs(result.samples.back().outputs.at("y") - 6.3) < 1.0e-12,
            "FMI 2 Co-Simulation Pending step did not complete consistently");

    auto undeclared_descriptor = descriptor;
    const auto asynchronous_capability = undeclared_descriptor.find(
        "canRunAsynchronuously=\"true\"");
    undeclared_descriptor.replace(
        asynchronous_capability,
        std::string("canRunAsynchronuously=\"true\"").size(),
        "canRunAsynchronuously=\"false\"");
    const auto undeclared_archive = root / "native-fmi2-async-undeclared.fmu";
    write_archive(undeclared_archive, undeclared_descriptor);
    bool undeclared_rejected{};
    try {
        (void)smave::smoke_fmi2_co_simulation(
            undeclared_archive, 0.2, 0.1, {}, true);
    } catch (const std::runtime_error& error) {
        undeclared_rejected = std::string(error.what()).find(
            "declared asynchronous execution") != std::string::npos;
    }
    require(undeclared_rejected,
            "FMI 2 Co-Simulation accepted undeclared Pending execution");

    auto no_callback_descriptor = descriptor;
    no_callback_descriptor.replace(
        no_callback_descriptor.find("smave-fmi2-async-token"),
        std::string("smave-fmi2-async-token").size(),
        "smave-fmi2-async-token-no-callback");
    const auto no_callback_archive = root / "native-fmi2-async-no-callback.fmu";
    write_archive(no_callback_archive, no_callback_descriptor);
    bool callback_rejected{};
    try {
        (void)smave::smoke_fmi2_co_simulation(
            no_callback_archive, 0.2, 0.1, {}, true);
    } catch (const std::runtime_error& error) {
        callback_rejected = std::string(error.what()).find(
            "callback is missing or inconsistent") != std::string::npos;
    }
    require(callback_rejected,
            "FMI 2 Co-Simulation accepted completion without stepFinished callback");

    auto stuck_descriptor = descriptor;
    stuck_descriptor.replace(
        stuck_descriptor.find("smave-fmi2-async-token"),
        std::string("smave-fmi2-async-token").size(),
        "smave-fmi2-async-token-stuck");
    const auto stuck_archive = root / "native-fmi2-async-stuck.fmu";
    write_archive(stuck_archive, stuck_descriptor);
    bool timeout_rejected{};
    try {
        (void)smave::smoke_fmi2_co_simulation(
            stuck_archive, 0.2, 0.1, {}, true, 2);
    } catch (const std::runtime_error& error) {
        timeout_rejected = std::string(error.what()).find(
            "timed out and was cancelled") != std::string::npos;
    }
    require(timeout_rejected,
            "FMI 2 Co-Simulation accepted a permanently Pending step");

    bool zero_timeout_rejected{};
    try {
        (void)smave::smoke_fmi2_co_simulation(
            archive, 0.2, 0.1, {}, true, 0);
    } catch (const std::invalid_argument& error) {
        zero_timeout_rejected = std::string(error.what()).find(
            "between 1 and 60000") != std::string::npos;
    }
    require(zero_timeout_rejected,
            "FMI 2 Co-Simulation accepted a zero asynchronous timeout");
}

void test_fmi2_native_model_exchange(const std::filesystem::path& root) {
    const auto ordered_metadata_directory = root / "fmi2-me-order-metadata";
    std::filesystem::create_directories(ordered_metadata_directory);
    std::ofstream(ordered_metadata_directory / "modelDescription.xml")
        << "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"OrderedME\" "
           "guid=\"ordered-token\" numberOfEventIndicators=\"0\">"
           "<ModelExchange modelIdentifier=\"OrderedME\"/>"
           "<ModelVariables>"
           "<ScalarVariable name=\"x1\" valueReference=\"1\"><Real/></ScalarVariable>"
           "<ScalarVariable name=\"dx1\" valueReference=\"2\"><Real derivative=\"1\"/></ScalarVariable>"
           "<ScalarVariable name=\"x2\" valueReference=\"3\"><Real/></ScalarVariable>"
           "<ScalarVariable name=\"dx2\" valueReference=\"4\"><Real derivative=\"3\"/></ScalarVariable>"
           "</ModelVariables><ModelStructure><Derivatives>"
           "<Unknown index=\"4\"/><Unknown index=\"2\"/>"
           "</Derivatives></ModelStructure></fmiModelDescription>";
    const auto ordered_metadata = smave::import_fmu(ordered_metadata_directory);
    require(ordered_metadata.derivative_variable_order ==
                std::vector<std::size_t>({4, 2}),
            "FMI 2 ModelStructure derivative order was not retained");

    const std::string descriptor =
        "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"NativeFmi2ME\" "
        "guid=\"smave-fmi2-me-token\" numberOfEventIndicators=\"1\">"
        "<ModelExchange modelIdentifier=\"SMAVEFmi2ME\" "
        "canGetAndSetFMUstate=\"true\" canSerializeFMUstate=\"true\"/>"
        "<ModelVariables>"
        "<ScalarVariable name=\"x\" valueReference=\"1\" causality=\"local\">"
        "<Real start=\"0\"/></ScalarVariable>"
        "<ScalarVariable name=\"der_x\" valueReference=\"2\" causality=\"local\">"
        "<Real derivative=\"1\"/></ScalarVariable>"
        "<ScalarVariable name=\"rate\" valueReference=\"3\" causality=\"parameter\">"
        "<Real start=\"1\"/></ScalarVariable>"
        "<ScalarVariable name=\"y\" valueReference=\"4\" causality=\"output\">"
        "<Real/></ScalarVariable>"
        "</ModelVariables>"
        "<ModelStructure><Outputs><Unknown index=\"4\"/></Outputs>"
        "<Derivatives><Unknown index=\"2\"/></Derivatives></ModelStructure>"
        "</fmiModelDescription>";
    const auto metadata_directory = root / "fmi2-me-native-metadata";
    std::filesystem::create_directories(metadata_directory);
    std::ofstream(metadata_directory / "modelDescription.xml") << descriptor;
    const auto metadata = smave::import_fmu(metadata_directory);
    std::string extension;
    if (metadata.host_platform.ends_with("darwin")) extension = ".dylib";
    if (metadata.host_platform.ends_with("linux")) extension = ".so";
    if (metadata.host_platform.ends_with("windows")) extension = ".dll";
    require(!extension.empty(), "test platform lacks FMI 2 ME binary naming");
    std::ifstream binary_input(SMAVE_FMI2_ME_FIXTURE_PATH, std::ios::binary);
    std::ostringstream binary_bytes;
    binary_bytes << binary_input.rdbuf();
    const auto archive = root / "native-fmi2-me.fmu";
    write_stored_zip(archive, {
        {"modelDescription.xml", descriptor},
        {"binaries/" + metadata.host_platform + "/SMAVEFmi2ME" + extension,
         binary_bytes.str()}});
    const auto imported = smave::import_fmu(archive);
    require(imported.schema_version == smave::kFmiBlackboxSchemaVersion &&
                imported.variables[1].derivative_of == 1,
            "FMI 2 derivative metadata was not retained");
    const auto current_ir = root / "fmi2-me-v4.ir";
    imported.write(current_ir);
    std::ifstream current_input(current_ir);
    const auto previous_ir = root / "fmi2-me-v3.ir";
    const auto older_ir = root / "fmi2-me-v2.ir";
    const auto legacy_ir = root / "fmi2-me-v1.ir";
    std::ofstream previous_output(previous_ir);
    std::ofstream older_output(older_ir);
    std::ofstream legacy_output(legacy_ir);
    std::string line;
    bool first_line = true;
    while (std::getline(current_input, line)) {
        if (first_line) {
            previous_output << smave::kPreviousFmiBlackboxSchemaVersion << '\n';
            older_output << smave::kOlderFmiBlackboxSchemaVersion << '\n';
            legacy_output << smave::kLegacyFmiBlackboxSchemaVersion << '\n';
            first_line = false;
            continue;
        }
        if (line.starts_with("VARIABLE ")) {
            const auto dimension_count = line.find_last_of(' ');
            const auto has_priority = line.find_last_of(' ', dimension_count - 1);
            line.erase(has_priority, dimension_count - has_priority);
            line.erase(line.find_last_of(' '));
            previous_output << line << " 0\n";
            older_output << line << '\n';
            line.erase(line.find_last_of(' '));
        } else {
            previous_output << line << '\n';
            older_output << line << '\n';
        }
        if (line.starts_with("DERIVATIVE_ORDER ")) continue;
        legacy_output << line << '\n';
    }
    previous_output.close();
    older_output.close();
    legacy_output.close();
    const auto previous = smave::FmiBlackboxIR::read(previous_ir);
    require(previous.schema_version == smave::kFmiBlackboxSchemaVersion &&
                previous.variables[1].derivative_of == 1 &&
                previous.variables[1].dimension_descriptors.empty() &&
                !previous.variables[1].clock_priority,
            "previous FMI IR was not safely upgraded");
    const auto older = smave::FmiBlackboxIR::read(older_ir);
    require(older.schema_version == smave::kFmiBlackboxSchemaVersion &&
                older.variables[1].derivative_of == 1 &&
                older.variables[1].dimension_descriptors.empty(),
            "older FMI IR was not safely upgraded");
    const auto legacy = smave::FmiBlackboxIR::read(legacy_ir);
    require(legacy.schema_version == smave::kFmiBlackboxSchemaVersion &&
                legacy.variables[1].derivative_of == 0,
            "legacy FMI IR was not safely upgraded");

    bool rejected_without_opt_in{};
    try {
        (void)smave::smoke_fmi2_model_exchange(
            archive, 0.2, 0.1, {{"rate", 1.0}}, false);
    } catch (const std::invalid_argument&) {
        rejected_without_opt_in = true;
    }
    require(rejected_without_opt_in,
            "FMI 2 ME execution proceeded without explicit opt-in");
    const auto result = smave::smoke_fmi2_model_exchange(
        archive, 0.2, 0.1, {{"rate", 1.0}}, true);
    require(result.success && result.samples.size() == 3 &&
                result.state_roundtrip_passed &&
                result.state_serialization_attempted &&
                result.state_serialization_passed &&
                result.serialized_state_bytes == 35 &&
                result.maximum_state_replay_error == 0.0 &&
                result.model_exchange_roots == 2 && result.time_events == 2 &&
                result.event_mode_entries == 4 &&
                std::abs(result.samples[1].outputs.at("y") - 1.55) < 1.0e-9 &&
                std::abs(result.samples[2].outputs.at("y") - 1.65) < 1.0e-9,
            "FMI 2 ME root, time event, or replay lifecycle failed");

    auto invalid_serialized_descriptor = descriptor;
    invalid_serialized_descriptor.replace(
        invalid_serialized_descriptor.find("smave-fmi2-me-token"),
        std::string("smave-fmi2-me-token").size(),
        "smave-fmi2-me-invalid-serialized-token");
    const auto invalid_serialized_archive =
        root / "native-fmi2-me-invalid-serialized.fmu";
    write_stored_zip(invalid_serialized_archive, {
        {"modelDescription.xml", invalid_serialized_descriptor},
        {"binaries/" + metadata.host_platform + "/SMAVEFmi2ME" + extension,
         binary_bytes.str()}});
    bool rejected_invalid_serialized{};
    try {
        (void)smave::smoke_fmi2_model_exchange(
            invalid_serialized_archive, 0.2, 0.1, {{"rate", 1.0}}, true);
    } catch (const std::runtime_error&) {
        rejected_invalid_serialized = true;
    }
    require(rejected_invalid_serialized,
            "corrupt FMI 2 serialized state was accepted");

    auto serialization_without_state_descriptor = descriptor;
    serialization_without_state_descriptor.erase(
        serialization_without_state_descriptor.find(
            "canGetAndSetFMUstate=\"true\" "),
        std::string("canGetAndSetFMUstate=\"true\" ").size());
    const auto serialization_without_state_archive =
        root / "native-fmi2-me-serialization-without-state.fmu";
    write_stored_zip(serialization_without_state_archive, {
        {"modelDescription.xml", serialization_without_state_descriptor},
        {"binaries/" + metadata.host_platform + "/SMAVEFmi2ME" + extension,
         binary_bytes.str()}});
    bool rejected_serialization_without_state{};
    try {
        (void)smave::smoke_fmi2_model_exchange(
            serialization_without_state_archive, 0.2, 0.1, {{"rate", 1.0}}, true);
    } catch (const std::invalid_argument&) {
        rejected_serialization_without_state = true;
    }
    require(rejected_serialization_without_state,
            "FMI 2 serialization capability without state capability was accepted");

    const auto grazing_descriptor = std::string(descriptor)
        .replace(descriptor.find("smave-fmi2-me-token"),
                 std::string("smave-fmi2-me-token").size(),
                 "smave-fmi2-me-grazing-token");
    const auto grazing_archive = root / "native-fmi2-me-grazing.fmu";
    write_stored_zip(grazing_archive, {
        {"modelDescription.xml", grazing_descriptor},
        {"binaries/" + metadata.host_platform + "/SMAVEFmi2ME" + extension,
         binary_bytes.str()}});
    const auto grazing_result = smave::smoke_fmi2_model_exchange(
        grazing_archive, 0.2, 0.1, {{"rate", 1.0}}, true);
    require(grazing_result.success && grazing_result.samples.size() == 3 &&
                grazing_result.model_exchange_roots == 2 &&
                grazing_result.model_exchange_grazing_roots == 2 &&
                grazing_result.state_roundtrip_passed &&
                grazing_result.maximum_state_replay_error == 0.0 &&
                std::abs(grazing_result.samples.back().outputs.at("y") - 0.65) <
                    1.0e-8,
            "FMI 2 ModelExchange grazing root or replay failed");

    const auto near_grazing_descriptor = std::string(descriptor)
        .replace(descriptor.find("smave-fmi2-me-token"),
                 std::string("smave-fmi2-me-token").size(),
                 "smave-fmi2-me-near-grazing-token");
    const auto near_grazing_archive = root / "native-fmi2-me-near-grazing.fmu";
    write_stored_zip(near_grazing_archive, {
        {"modelDescription.xml", near_grazing_descriptor},
        {"binaries/" + metadata.host_platform + "/SMAVEFmi2ME" + extension,
         binary_bytes.str()}});
    const auto near_grazing_result = smave::smoke_fmi2_model_exchange(
        near_grazing_archive, 0.2, 0.1, {{"rate", 1.0}}, true);
    require(near_grazing_result.success &&
                near_grazing_result.model_exchange_roots == 0 &&
                near_grazing_result.model_exchange_grazing_roots == 0 &&
                std::abs(near_grazing_result.samples.back().outputs.at("y") - 0.2) <
                    1.0e-9,
            "FMI 2 ModelExchange accepted a near-grazing indicator that did not reach zero");

    const auto nominal_descriptor = std::string(descriptor)
        .replace(descriptor.find("smave-fmi2-me-token"),
                 std::string("smave-fmi2-me-token").size(),
                 "smave-fmi2-me-nominal-token");
    const auto nominal_archive = root / "native-fmi2-me-nominal.fmu";
    write_stored_zip(nominal_archive, {
        {"modelDescription.xml", nominal_descriptor},
        {"binaries/" + metadata.host_platform + "/SMAVEFmi2ME" + extension,
         binary_bytes.str()}});
    const auto nominal_result = smave::smoke_fmi2_model_exchange(
        nominal_archive, 0.2, 0.1, {{"rate", 1.0}}, true);
    require(nominal_result.success &&
                nominal_result.continuous_state_nominal_updates == 2 &&
                nominal_result.minimum_continuous_state_nominal == 2.0 &&
                nominal_result.maximum_continuous_state_nominal == 2.0 &&
                nominal_result.state_roundtrip_passed,
            "FMI 2 ModelExchange nominal change was not queried and replayed");

    const auto invalid_nominal_descriptor = std::string(descriptor)
        .replace(descriptor.find("smave-fmi2-me-token"),
                 std::string("smave-fmi2-me-token").size(),
                 "smave-fmi2-me-invalid-nominal-token");
    const auto invalid_nominal_archive = root / "native-fmi2-me-invalid-nominal.fmu";
    write_stored_zip(invalid_nominal_archive, {
        {"modelDescription.xml", invalid_nominal_descriptor},
        {"binaries/" + metadata.host_platform + "/SMAVEFmi2ME" + extension,
         binary_bytes.str()}});
    bool rejected_invalid_fmi2_nominal{};
    try {
        (void)smave::smoke_fmi2_model_exchange(
            invalid_nominal_archive, 0.2, 0.1, {{"rate", 1.0}}, true);
    } catch (const std::runtime_error& error) {
        rejected_invalid_fmi2_nominal = std::string(error.what()).find(
            "finite and positive") != std::string::npos;
    }
    require(rejected_invalid_fmi2_nominal,
            "FMI 2 ModelExchange accepted a zero continuous-state nominal");
    bool rejected_by_fmi3_executor{};
    try {
        (void)smave::smoke_fmi3_model_exchange(
            archive, 0.2, 0.1, {{"rate", 1.0}}, true);
    } catch (const std::invalid_argument&) {
        rejected_by_fmi3_executor = true;
    }
    require(rejected_by_fmi3_executor,
            "FMI 3 ME executor accepted FMI 2 metadata");
}

void test_compile_roundtrip_and_runtime(const std::filesystem::path& root) {
    const auto source = root / "Coupled.mo";
    std::ofstream(source)
        << "model Coupled\n"
        << " parameter Real p = 2.0;\n"
        << " Real x(start=1.0, nominal=2.0, min=0.0);\n"
        << " Real y(start=1.0, nominal=2.0, min=0.0);\n"
        << "equation\n"
        << " x*x + y = p + 4.0;\n"
        << " x + y*y = p + 4.0;\n"
        << "end Coupled;\n";
    const auto model = smave::compile_model(source, "Coupled");
    require(model.blocks.size() == 1, "coupled equations must form one SCC");
    require(!model.blocks.front().linear, "nonlinear block misclassified");
    const auto alias_source = root / "CubicLinearAlias.mo";
    std::ofstream(alias_source)
        << "model CubicLinearAlias\n"
        << "Real x(start=0.0);\n"
        << "equation\n"
        << "(x-1.0)+0.001*(x-1.0)*(x-1.0)*(x-1.0)=0.0;\n"
        << "end CubicLinearAlias;\n";
    const auto alias_model = smave::compile_model(alias_source);
    require(alias_model.blocks.size() == 1 && !alias_model.blocks.front().linear,
            "cubic expression passing through x=0,1,2 was misclassified as linear");
    require(!model.blocks.front().fingerprint.empty(), "fingerprint missing");
    const auto ir_path = root / "coupled.ir";
    model.write(ir_path);
    std::ifstream written_input(ir_path);
    const std::string written{
        std::istreambuf_iterator<char>(written_input),
        std::istreambuf_iterator<char>()};
    require(written.find("SMAVE_IR \"smave.ir.v2\"") != std::string::npos &&
                written.find("SPARSITY_CSR 2 2 4") != std::string::npos &&
                written.find("\nSPARSITY ") == std::string::npos,
            "IR v2 did not use canonical CSR serialization");
    const auto restored = smave::ModelIR::read(ir_path);
    require(restored.source_hash == model.source_hash, "IR roundtrip lost source hash");

    auto legacy_text = written;
    legacy_text.replace(
        legacy_text.find("smave.ir.v2"), std::string("smave.ir.v2").size(),
        "smave.ir.v1");
    const auto legacy_fingerprint = legacy_block_fingerprint(model.blocks.front(), model);
    legacy_text.replace(
        legacy_text.find(model.blocks.front().fingerprint),
        model.blocks.front().fingerprint.size(), legacy_fingerprint);
    const auto csr_begin = legacy_text.find("SPARSITY_CSR");
    const auto csr_end = legacy_text.find("END\n", csr_begin);
    legacy_text.replace(
        csr_begin, csr_end - csr_begin,
        "SPARSITY 2 2\n1 1\n1 1\n");
    const auto legacy_path = root / "coupled-v1.ir";
    std::ofstream(legacy_path) << legacy_text;
    const auto upgraded = smave::ModelIR::read(legacy_path);
    require(upgraded.schema_version == smave::kIrSchemaVersion &&
                upgraded.blocks.front().jacobian_sparsity.nonzeros() == 4 &&
                upgraded.blocks.front().fingerprint ==
                    smave::block_fingerprint(upgraded.blocks.front(), upgraded),
            "legacy IR was not verified and upgraded to canonical CSR v2");
    const auto upgraded_path = root / "coupled-upgraded.ir";
    upgraded.write(upgraded_path);
    std::ifstream upgraded_input(upgraded_path);
    const std::string upgraded_text{
        std::istreambuf_iterator<char>(upgraded_input),
        std::istreambuf_iterator<char>()};
    require(upgraded_text.find("SPARSITY_CSR") != std::string::npos &&
                upgraded_text.find("smave.ir.v1") == std::string::npos,
            "upgraded legacy IR was not written exclusively as v2 CSR");

    auto damaged_legacy = legacy_text;
    damaged_legacy.replace(
        damaged_legacy.find(legacy_fingerprint), legacy_fingerprint.size(),
        "0000000000000000");
    const auto damaged_legacy_path = root / "coupled-v1-damaged.ir";
    std::ofstream(damaged_legacy_path) << damaged_legacy;
    bool rejected_legacy_fingerprint{};
    try {
        (void)smave::ModelIR::read(damaged_legacy_path);
    } catch (const std::invalid_argument&) {
        rejected_legacy_fingerprint = true;
    }
    require(rejected_legacy_fingerprint,
            "legacy IR upgrade accepted an incompatible stored fingerprint");

    auto damaged_csr = written;
    const auto offsets = damaged_csr.find("ROW_OFFSETS 3 0 2 4");
    damaged_csr.replace(
        offsets, std::string("ROW_OFFSETS 3 0 2 4").size(),
        "ROW_OFFSETS 3 0 4 3");
    const auto damaged_csr_path = root / "coupled-v2-damaged.ir";
    std::ofstream(damaged_csr_path) << damaged_csr;
    bool rejected_csr{};
    try {
        (void)smave::ModelIR::read(damaged_csr_path);
    } catch (const std::invalid_argument&) {
        rejected_csr = true;
    }
    require(rejected_csr, "IR v2 accepted malformed CSR row offsets");

    const smave::Runtime runtime(restored);
    auto rejected_values = std::unordered_map<std::string, double>{{"p", 2.0}, {"x", -1.0}, {"y", 5.0}};
    const auto rejected = runtime.evaluate_gate(restored.blocks.front(), rejected_values, true);
    require(rejected.decision == smave::GateDecision::reject, "constraint gate accepted invalid root");
    const auto outcome = runtime.solve({{"p", 2.0}}, root / "traces");
    require(outcome.success, "runtime failed solvable system: " + outcome.message);
    require(std::abs(outcome.values.at("x") - 2.0) < 1.0e-7, "wrong x root");
    require(std::abs(outcome.values.at("y") - 2.0) < 1.0e-7, "wrong y root");
    require(outcome.blocks.front().gate.decision == smave::GateDecision::direct_accept,
            "final result did not pass independent gate");
    require(outcome.timing.total_us > 0.0 && outcome.blocks.front().timing.gate_us > 0.0,
            "runtime timing telemetry was not recorded");
    require(outcome.warm_start_count == 1, "warm-start path count was not recorded");
    require(std::filesystem::exists(root / "traces" / (outcome.trace_id + ".trace")),
            "trace not persisted");
}

void test_expert_residency_and_runtime_fallback(const std::filesystem::path& root) {
    smave::ExpertResidencyManager manager(smave::ResidencyConfig{
        .device = "cpu",
        .capacity_bytes = 8,
        .minimum_invocations = 1,
    });
    const auto first = manager.request("expert-a", 8);
    const auto blocked = manager.request("expert-b", 8);
    const auto admitted = manager.request("expert-b", 8);
    require(first.admitted && !first.cache_hit && first.resident_bytes == 8,
            "first CPU expert was not admitted into an empty residency budget");
    require(!blocked.admitted && blocked.invocation_heat == 1 &&
                blocked.reason.find("equally hot") != std::string::npos,
            "equally hot expert displaced a resident expert");
    require(admitted.admitted && !admitted.cache_hit &&
                admitted.invocation_heat == 2 &&
                admitted.evicted_experts == std::vector<std::string>{"expert-a"},
            "hot expert did not deterministically evict the colder resident");
    const auto hit = manager.request("expert-b", 8);
    require(hit.admitted && hit.cache_hit && hit.resident_bytes == 8,
            "resident CPU expert was not served as a cache hit");
    const auto snapshot = manager.snapshot();
    require(snapshot.resident_experts == std::vector<std::string>{"expert-b"} &&
                snapshot.resident_bytes == 8,
            "CPU residency snapshot disagrees with admission decisions");

    bool rejected_device{};
    try {
        (void)smave::ExpertResidencyManager(smave::ResidencyConfig{
            .device = "gpu", .capacity_bytes = 8, .minimum_invocations = 1});
    } catch (const std::invalid_argument&) {
        rejected_device = true;
    }
    require(rejected_device, "unimplemented GPU residency was silently accepted");

    const auto source = root / "ResidencyFallback.mo";
    std::ofstream(source)
        << "model ResidencyFallback\nReal x(start=1);\n"
        << "equation\nx*x = 4;\nend ResidencyFallback;\n";
    const auto model = smave::compile_model(source);
    const smave::Runtime runtime(
        model, {}, {}, smave::ResidencyConfig{
            .device = "cpu",
            .capacity_bytes = 1,
            .minimum_invocations = 2,
        });
    const auto cold = runtime.solve({}, root / "residency-cold-traces");
    require(cold.success && cold.fallback_count == 1 &&
                cold.residency_rejection_count == 1 &&
                cold.residency_load_count == 0 &&
                cold.blocks.front().residency_records.size() == 1 &&
                cold.blocks.front().residency_records.front().outcome == "rejected",
            "cold expert rejection did not preserve the original solver fallback");
    const auto hot = runtime.solve({}, root / "residency-hot-traces");
    require(hot.success && hot.warm_start_count == 1 &&
                hot.residency_load_count == 1 &&
                hot.residency_rejection_count == 0 &&
                hot.resident_expert_bytes == 1,
            "expert was not loaded after reaching its invocation threshold");
    const auto reused = runtime.solve({}, root / "residency-hit-traces");
    require(reused.success && reused.residency_hit_count == 1 &&
                reused.residency_load_count == 0,
            "loaded expert was not reused from CPU residency");
}

void test_dataset_registry_integrity(const std::filesystem::path& root) {
    const auto source = root / "dataset-source";
    std::filesystem::create_directories(source / "nested");
    std::ofstream(source / "a.conf") << "p=1\n";
    std::ofstream(source / "nested" / "b.conf") << "p=2\n";
    const smave::DatasetStore store(root / "dataset-store");
    const auto first = store.snapshot(source, "training-domain");
    const auto repeated = store.snapshot(source, "training-domain");
    require(first.version == repeated.version && first.files.size() == 2 &&
                first.total_bytes == 8,
            "identical dataset snapshot was not content-addressed and idempotent");
    const auto verified = store.verify(first.dataset_id, first.version);
    require(verified.manifest_hash == first.manifest_hash,
            "immutable dataset snapshot did not verify");

    std::ofstream(source / "a.conf", std::ios::app) << "q=3\n";
    const auto changed = store.snapshot(source, "training-domain");
    require(changed.version != first.version,
            "changed dataset contents reused an existing version");
    require(store.verify(first.dataset_id, first.version).version == first.version,
            "new snapshot mutated the previous immutable dataset version");

    const auto copied_store = root / "dataset-store-copy";
    std::filesystem::copy(
        root / "dataset-store", copied_store,
        std::filesystem::copy_options::recursive);
    std::ofstream(
        smave::DatasetStore(copied_store).version_directory(
            first.dataset_id, first.version) / "a.conf",
        std::ios::app) << "tampered=1\n";
    bool rejected_tamper{};
    try {
        (void)smave::DatasetStore(copied_store).verify(first.dataset_id, first.version);
    } catch (const std::invalid_argument&) {
        rejected_tamper = true;
    }
    require(rejected_tamper, "tampered dataset snapshot passed integrity verification");
    require(store.verify(first.dataset_id, first.version).version == first.version,
            "copied-store tamper test modified the authoritative dataset store");

    std::ofstream(
        store.version_directory(first.dataset_id, first.version) / "unexpected.conf")
        << "p=0\n";
    bool rejected_extra{};
    try {
        (void)store.verify(first.dataset_id, first.version);
    } catch (const std::invalid_argument&) {
        rejected_extra = true;
    }
    require(rejected_extra, "dataset snapshot accepted an unmanifested extra file");
}

void test_event_rejection(const std::filesystem::path& root) {
    const auto source = root / "Dynamic.mo";
    std::ofstream(source)
        << "model Dynamic\nReal x;\nequation\nder(x) = -x;\nend Dynamic;\n";
    bool rejected = false;
    try { (void)smave::compile_model(source); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "unsupported derivative semantics were silently erased");
}

void test_linear_direct_and_fallback(const std::filesystem::path& root) {
    const auto linear_source = root / "Linear.mo";
    std::ofstream(linear_source)
        << "model Linear\nparameter Real p = 4;\nReal x(start=0);\n"
        << "equation\n2*x = p;\nend Linear;\n";
    const auto linear_model = smave::compile_model(linear_source);
    require(linear_model.blocks.front().linear, "linear block misclassified");
    std::unordered_map<std::string, smave::Expression> linear_residuals;
    for (const auto& equation : linear_model.equations) {
        linear_residuals.emplace(equation.id, smave::Expression(equation.residual));
    }
    auto cached_system = smave::assemble_linear_system(
        linear_model, linear_model.blocks.front(), linear_residuals, {{"p", 4.0}});
    smave::update_linear_right_hand_side(
        cached_system, linear_model, linear_model.blocks.front(),
        linear_residuals, {{"p", 6.0}});
    const auto reassembled_system = smave::assemble_linear_system(
        linear_model, linear_model.blocks.front(), linear_residuals, {{"p", 6.0}});
    require(cached_system.matrix == reassembled_system.matrix &&
                cached_system.right_hand_side == reassembled_system.right_hand_side,
            "constant linear matrix cache changed the assembled system");
    const auto direct = smave::Runtime(linear_model).solve({{"p", 4.0}}, root / "linear-traces");
    require(direct.success, "linear solve failed");
    require(direct.blocks.front().path == smave::SolvePath::direct_accept,
            "linear classic expert did not use direct path");

    const auto fallback_source = root / "Fallback.mo";
    std::ofstream(fallback_source)
        << "model Fallback\nReal x(start=100);\nequation\nx*x = 2;\nend Fallback;\n";
    const auto fallback_model = smave::compile_model(fallback_source);
    const auto fallback = smave::Runtime(fallback_model).solve({}, root / "fallback-traces");
    require(fallback.success, "original fallback failed");
    require(fallback.blocks.front().path == smave::SolvePath::full_fallback,
            "exhausted corrector did not reach original fallback");
    require(fallback.blocks.front().fallback_iterations > 0, "fallback was not executed");
    require(fallback.fallback_count == 1 && fallback.timing.fallback_us > 0.0,
            "fallback telemetry was not recorded");
    require(!fallback.blocks.front().attempt_records.empty() &&
                fallback.blocks.front().attempt_records.back().expert_version ==
                    "original-damped-newton" &&
                fallback.blocks.front().attempt_records.back().outcome == "fallback" &&
                fallback.blocks.front().attempt_records.back().reason.find(
                    "experts exhausted") != std::string::npos &&
                std::any_of(
                    fallback.blocks.front().attempt_records.begin(),
                    fallback.blocks.front().attempt_records.end(),
                    [](const smave::ExpertAttemptRecord& attempt) {
                        return attempt.outcome == "rejected";
                    }),
            "expert rejection and original fallback reasons were not audited");

    const auto unknown = smave::Runtime(linear_model).solve(
        {{"unsafe_unknown", 1.0}}, root / "unknown-traces");
    require(!unknown.success && unknown.message.find("unknown scenario field") != std::string::npos,
            "unknown scenario field was silently accepted");
}

void test_registry_bundle_and_routing(const std::filesystem::path& root) {
    const auto source = root / "Registry.mo";
    std::ofstream(source)
        << "model Registry\nReal x(start=1);\nequation\nx*x = 4;\nend Registry;\n";
    const auto model = smave::compile_model(source);
    auto registry = smave::make_default_registry(model);
    auto bundle = smave::make_default_bundle(model);
    registry.validate_bundle(bundle, model);
    require(registry.compatible(
                "continuation-warm-start-v1", model.blocks.front(), bundle,
                smave::Permission::warm_start),
            "valid E2 warm-start grant was rejected");

    const auto bundle_path = root / "runtime.bundle";
    bundle.write(bundle_path);
    const auto restored = smave::RuntimeBundle::read(bundle_path);
    require(restored.bundle_hash == bundle.bundle_hash, "bundle roundtrip lost integrity hash");

    const auto trailing_bundle_path = root / "trailing.bundle";
    std::filesystem::copy_file(bundle_path, trailing_bundle_path);
    std::ofstream(trailing_bundle_path, std::ios::app) << "TAMPER\n";
    bool rejected_trailing_bundle = false;
    try { (void)smave::RuntimeBundle::read(trailing_bundle_path); }
    catch (const std::runtime_error&) { rejected_trailing_bundle = true; }
    require(rejected_trailing_bundle, "bundle parser accepted trailing content");

    auto tampered = bundle;
    tampered.domain_version = "unverified-domain";
    bool rejected_tamper = false;
    try { registry.validate_bundle(tampered, model); }
    catch (const std::invalid_argument&) { rejected_tamper = true; }
    require(rejected_tamper, "tampered bundle was accepted");

    const smave::CompileRouter compile_router;
    const auto candidates = compile_router.lookup(model.blocks.front(), registry, bundle);
    require(candidates.size() == 1, "compile router failed to retain compatible expert");
    const smave::RuntimeRouter runtime_router(smave::RoutingConfig{.top_k = 1});
    const auto plan = runtime_router.route(
        model.blocks.front(), smave::BlockContext{}, candidates, registry, bundle);
    require(plan.steps.size() == 1, "runtime router violated top_k");
    require(plan.terminal_fallback == "original-damped-newton",
            "runtime router removed terminal fallback");
    require(!plan.plan_id.empty(), "runtime router omitted plan id");

    const smave::SolveStep cheap_unlikely{
        .estimated_cost_us = 2.0,
        .pass_probability = 0.2,
        .risk_score = 0.01,
    };
    const smave::SolveStep expensive_likely{
        .estimated_cost_us = 6.0,
        .pass_probability = 0.9,
        .risk_score = 0.02,
    };
    require(
        smave::cascade_ordering_index(expensive_likely) <
            smave::cascade_ordering_index(cheap_unlikely),
        "cascade ordering ignored reach-weighted acceptance probability");
    std::vector<smave::SolveStep> cascade{cheap_unlikely, expensive_likely};
    const double unsorted_cost = smave::expected_cascade_cost(cascade, 8.0);
    smave::order_cascade_steps(cascade);
    require(
        cascade.front().estimated_cost_us == expensive_likely.estimated_cost_us &&
            smave::expected_cascade_cost(cascade, 8.0) < unsorted_cost,
        "cascade ordering did not reduce expected complete cost");
    bool rejected_invalid_cascade_step = false;
    try {
        (void)smave::cascade_ordering_index(smave::SolveStep{
            .estimated_cost_us = 1.0,
            .pass_probability = 0.0,
        });
    } catch (const std::invalid_argument&) {
        rejected_invalid_cascade_step = true;
    }
    require(rejected_invalid_cascade_step,
            "cascade ordering accepted a zero-probability step");
    bool rejected_invalid_terminal_cost = false;
    try {
        (void)smave::expected_cascade_cost(cascade, -1.0);
    } catch (const std::invalid_argument&) {
        rejected_invalid_terminal_cost = true;
    }
    require(rejected_invalid_terminal_cost,
            "cascade evaluation accepted a negative terminal cost");

    const std::vector<smave::SolveStep> joint_alternatives{
        smave::SolveStep{
            .expert_version = "expert-a",
            .budget = smave::SolveBudget{.work_iterations = 0},
            .estimated_cost_us = 1.0,
            .pass_probability = 0.2,
        },
        smave::SolveStep{
            .expert_version = "expert-a",
            .budget = smave::SolveBudget{.work_iterations = 2},
            .estimated_cost_us = 5.0,
            .pass_probability = 1.0,
        },
        smave::SolveStep{
            .expert_version = "expert-b",
            .budget = smave::SolveBudget{.work_iterations = 1},
            .estimated_cost_us = 2.0,
            .pass_probability = 0.5,
        },
    };
    const auto joint = smave::optimize_joint_calibrated_cascade(
        joint_alternatives, 2, 10.0);
    require(
        joint.size() == 2 &&
            joint[0].expert_version == "expert-b" &&
            joint[0].budget.work_iterations == 1 &&
            joint[1].expert_version == "expert-a" &&
            joint[1].budget.work_iterations == 2 &&
            smave::expected_cascade_cost(joint, 10.0) == 4.5,
        "joint router did not optimize expert, budget, and continuation cost");
    require(
        std::count_if(
            joint.begin(), joint.end(), [](const smave::SolveStep& step) {
                return step.expert_version == "expert-a";
            }) == 1,
        "joint router selected multiple budgets for one expert");
    bool rejected_duplicate_joint_action = false;
    try {
        auto duplicate = joint_alternatives;
        duplicate.push_back(joint_alternatives.front());
        (void)smave::optimize_joint_calibrated_cascade(duplicate, 2, 10.0);
    } catch (const std::invalid_argument&) {
        rejected_duplicate_joint_action = true;
    }
    require(rejected_duplicate_joint_action,
            "joint router accepted duplicate expert-budget actions");
    bool rejected_joint_state_overflow = false;
    try {
        (void)smave::optimize_joint_calibrated_cascade(
            joint_alternatives, 2, 10.0, 1);
    } catch (const std::invalid_argument&) {
        rejected_joint_state_overflow = true;
    }
    require(rejected_joint_state_overflow,
            "joint router ignored its calibrated state bound");

    const std::vector<smave::SolveStep> interaction_alternatives{
        smave::SolveStep{
            .expert_version = "interaction-a",
            .budget = smave::SolveBudget{.work_iterations = 1},
            .estimated_cost_us = 2.0,
            .pass_probability = 0.5,
        },
        smave::SolveStep{
            .expert_version = "interaction-b",
            .budget = smave::SolveBudget{.work_iterations = 1},
            .estimated_cost_us = 2.0,
            .pass_probability = 0.5,
        },
    };
    const std::vector<smave::RouteConditionalCostCalibration> interactions{
        smave::RouteConditionalCostCalibration{
            .previous = smave::RouteActionReference{
                .expert_version = "interaction-a", .work_iterations = 1},
            .next = smave::RouteActionReference{
                .expert_version = "interaction-b", .work_iterations = 1},
            .independent_training_groups = 3,
            .independent_calibration_groups = 2,
            .conditional_cost_multiplier = 8.0,
            .conditional_cost_multiplier_upper = 10.0,
        },
        smave::RouteConditionalCostCalibration{
            .previous = smave::RouteActionReference{
                .expert_version = "interaction-b", .work_iterations = 1},
            .next = smave::RouteActionReference{
                .expert_version = "interaction-a", .work_iterations = 1},
            .independent_training_groups = 3,
            .independent_calibration_groups = 2,
            .conditional_cost_multiplier = 1.0,
            .conditional_cost_multiplier_upper = 1.0,
        },
    };
    const auto interaction_plan =
        smave::optimize_interaction_aware_calibrated_cascade(
            interaction_alternatives, interactions, 2, 20.0);
    require(
        interaction_plan.size() == 2 &&
            interaction_plan[0].expert_version == "interaction-b" &&
            interaction_plan[1].expert_version == "interaction-a" &&
            smave::expected_interaction_aware_cascade_cost(
                interaction_plan, 20.0, interactions) == 8.0,
        "interaction-aware router ignored conditional action cost");
    const auto interaction_degenerate =
        smave::optimize_interaction_aware_calibrated_cascade(
            joint_alternatives, {}, 2, 10.0);
    require(
        interaction_degenerate.size() == joint.size() &&
            std::equal(
                interaction_degenerate.begin(), interaction_degenerate.end(),
                joint.begin(), [](const auto& left, const auto& right) {
                    return left.expert_version == right.expert_version &&
                        left.budget.work_iterations == right.budget.work_iterations;
                }),
        "interaction-aware router did not reduce to independent calibrated routing");
    bool rejected_invalid_interaction = false;
    try {
        auto invalid_interactions = interactions;
        invalid_interactions.front().conditional_cost_multiplier_upper = 0.5;
        (void)smave::optimize_interaction_aware_calibrated_cascade(
            interaction_alternatives, invalid_interactions, 2, 20.0);
    } catch (const std::invalid_argument&) {
        rejected_invalid_interaction = true;
    }
    require(rejected_invalid_interaction,
            "interaction-aware router accepted an invalid conditional upper cost");

    bool rejected_unsafe_routing = false;
    try {
        (void)smave::RuntimeRouter(smave::RoutingConfig{
            .top_k = 1, .require_original_fallback = false});
    } catch (const std::invalid_argument&) {
        rejected_unsafe_routing = true;
    }
    require(rejected_unsafe_routing, "router allowed original fallback removal");
    bool rejected_negative_correction_budget = false;
    try {
        smave::RoutingConfig invalid_budget;
        invalid_budget.calibrations.emplace(
            "candidate", smave::RouteCalibration{.work_iterations = -1});
        (void)smave::RuntimeRouter(invalid_budget);
    } catch (const std::invalid_argument&) {
        rejected_negative_correction_budget = true;
    }
    require(rejected_negative_correction_budget,
            "router accepted a negative calibrated correction budget");
    bool rejected_invalid_joint_calibration = false;
    try {
        smave::RoutingConfig invalid_joint;
        invalid_joint.calibrated_terminal_fallback_cost_us = 10.0;
        invalid_joint.calibrations["candidate"].budget_options.push_back(
            smave::RouteBudgetCalibration{
                .work_iterations = 1,
                .attempts = 2,
                .passes = 1,
                .fallbacks = 0,
                .pass_probability = 0.5,
                .median_attempt_wall_us = 1.0,
            });
        (void)smave::RuntimeRouter(invalid_joint);
    } catch (const std::invalid_argument&) {
        rejected_invalid_joint_calibration = true;
    }
    require(rejected_invalid_joint_calibration,
            "router accepted inconsistent joint calibration counts");

    std::vector<smave::RouteActionTrainingSample> conditioned_training;
    std::vector<smave::RouteActionTrainingSample> conditioned_calibration;
    for (int index = 0; index < 12; ++index) {
        const double feature = static_cast<double>(index);
        conditioned_training.push_back(smave::RouteActionTrainingSample{
            .expert_version = "candidate",
            .work_iterations = 2,
            .independent_group = "training-" + std::to_string(index),
            .routing_family = "synthetic",
            .features = {feature},
            .attempt_wall_us = std::exp(1.0 + 0.1 * feature),
            .passed = index >= 6,
        });
    }
    for (int index = 12; index < 16; ++index) {
        const double feature = static_cast<double>(index);
        conditioned_calibration.push_back(smave::RouteActionTrainingSample{
            .expert_version = "candidate",
            .work_iterations = 2,
            .independent_group = "calibration-" + std::to_string(index),
            .routing_family = "synthetic",
            .features = {feature},
            .attempt_wall_us = std::exp(1.0 + 0.1 * feature),
            .passed = true,
        });
    }
    const auto conditioned_model = smave::train_request_conditioned_routing_model(
        {"context:p"}, conditioned_training, conditioned_calibration);
    const auto conditioned_prediction = smave::predict_request_conditioned_action(
        conditioned_model, "candidate", 2, {14.0});
    require(
        std::abs(conditioned_prediction.attempt_wall_us - std::exp(2.4)) < 1.0e-4 &&
            conditioned_prediction.pass_probability > 0.9,
        "request-conditioned route model failed a deterministic fit");

    std::vector<smave::RouteActionTrainingSample> relative_training;
    std::vector<smave::RouteActionTrainingSample> relative_calibration;
    const auto append_relative_samples = [](
        std::vector<smave::RouteActionTrainingSample>& samples,
        int begin, int end, const std::string& prefix) {
        for (int index = begin; index < end; ++index) {
            const double feature = static_cast<double>(index);
            const double terminal_cost = std::exp(3.0 + 0.1 * feature);
            samples.push_back(smave::RouteActionTrainingSample{
                .expert_version = "candidate-relative",
                .work_iterations = 4,
                .independent_group = prefix + std::to_string(index),
                .routing_family = "synthetic-relative",
                .features = {feature},
                .attempt_wall_us = terminal_cost * std::exp(-1.0 + 0.05 * feature),
                .terminal_reference_wall_us = terminal_cost,
                .cost_relative_to_terminal = true,
                .passed = true,
            });
            samples.push_back(smave::RouteActionTrainingSample{
                .expert_version = "terminal-relative",
                .work_iterations = 0,
                .independent_group = prefix + std::to_string(index),
                .routing_family = "synthetic-relative",
                .features = {feature},
                .attempt_wall_us = terminal_cost,
                .passed = true,
            });
        }
    };
    append_relative_samples(relative_training, 0, 8, "relative-training-");
    append_relative_samples(relative_calibration, 8, 12, "relative-calibration-");
    const auto relative_model = smave::train_request_conditioned_routing_model(
        {"context:p"}, relative_training, relative_calibration);
    const auto terminal_relative_prediction =
        smave::predict_request_conditioned_action(
            relative_model, "terminal-relative", 0, {10.0});
    const auto candidate_relative_prediction =
        smave::predict_request_conditioned_action(
            relative_model, "candidate-relative", 4, {10.0},
            terminal_relative_prediction.attempt_wall_us);
    require(
        std::abs(terminal_relative_prediction.attempt_wall_us - std::exp(4.0)) < 1.0e-4 &&
            std::abs(candidate_relative_prediction.attempt_wall_us - std::exp(3.5)) <
                1.0e-4 &&
            relative_model.actions.at("candidate-relative").front()
                    .independent_calibration_groups == 4,
        "relative request-conditioned model lost terminal normalization or group counts");
    auto shifted_calibration = conditioned_calibration;
    for (auto& sample : shifted_calibration) {
        sample.attempt_wall_us *= 2.0;
    }
    const auto calibrated_model = smave::train_request_conditioned_routing_model(
        {"context:p"}, conditioned_training, shifted_calibration);
    const auto calibrated_prediction = smave::predict_request_conditioned_action(
        calibrated_model, "candidate", 2, {14.0});
    require(
        std::abs(calibrated_prediction.attempt_wall_us - 2.0 * std::exp(2.4)) <
                1.0e-4 &&
            calibrated_prediction.pass_probability > 0.99,
        "calibration split did not correct request-conditioned point estimates");

    smave::SparseLinearProfile structural_profile{
        .fingerprint = "structural-features",
        .rows = 16,
        .columns = 16,
        .nonzeros = 64,
        .structurally_symmetric = true,
        .numerically_symmetric = true,
        .numerically_positive_definite = true,
        .diagonal_condition_estimate = 4.0,
        .right_hand_side_inf = 1.0,
        .right_hand_side_roughness = 0.25,
        .right_hand_side_sign_change_fraction = 0.5,
        .maximum_work_iterations = 100,
        .restart_dimension = 20,
    };
    const auto structural_features = smave::extract_sparse_routing_features(
        {"sparse:structurally_symmetric",
         "sparse:numerically_positive_definite"},
        structural_profile);
    require(
        structural_features == std::vector<double>({1.0, 1.0}),
        "sparse structure features did not expose existing numerical probes");

    smave::LinearSystem routing_feature_system;
    routing_feature_system.matrix = {{4.0, -1.0}, {0.0, 2.0}};
    routing_feature_system.right_hand_side = {1.0, 1.0};
    smave::classify_linear_system(routing_feature_system);
    require(
        std::abs(routing_feature_system.coefficient_dynamic_range - 4.0) < 1.0e-12 &&
            std::abs(
                routing_feature_system.row_nonzero_coefficient_of_variation -
                (1.0 / 3.0)) < 1.0e-12 &&
            std::abs(routing_feature_system.row_l1_condition_estimate - 2.5) < 1.0e-12 &&
            routing_feature_system.diagonal_dominance_fraction == 1.0 &&
            std::abs(
                routing_feature_system.mean_diagonal_row_l1_fraction - 0.9) < 1.0e-12 &&
            std::abs(routing_feature_system.normalized_mean_bandwidth -
                     (1.0 / 3.0)) < 1.0e-12,
        "linear classification produced incorrect sparse routing statistics");
    auto enriched_profile = structural_profile;
    enriched_profile.coefficient_dynamic_range =
        routing_feature_system.coefficient_dynamic_range;
    enriched_profile.row_nonzero_coefficient_of_variation =
        routing_feature_system.row_nonzero_coefficient_of_variation;
    enriched_profile.row_l1_condition_estimate =
        routing_feature_system.row_l1_condition_estimate;
    enriched_profile.diagonal_dominance_fraction =
        routing_feature_system.diagonal_dominance_fraction;
    enriched_profile.mean_diagonal_row_l1_fraction =
        routing_feature_system.mean_diagonal_row_l1_fraction;
    enriched_profile.normalized_mean_bandwidth =
        routing_feature_system.normalized_mean_bandwidth;
    const auto enriched_features = smave::extract_sparse_routing_features(
        {"sparse:log_coefficient_dynamic_range",
         "sparse:row_nonzero_coefficient_of_variation",
         "sparse:log_row_l1_condition",
         "sparse:diagonal_dominance_fraction",
         "sparse:mean_diagonal_row_l1_fraction",
         "sparse:normalized_mean_bandwidth"},
        enriched_profile);
    require(
        enriched_features.size() == 6 &&
            std::abs(enriched_features[0] - std::log(5.0)) < 1.0e-12 &&
            std::abs(enriched_features[1] - 1.0 / 3.0) < 1.0e-12 &&
            std::abs(enriched_features[2] - std::log(3.5)) < 1.0e-12 &&
            enriched_features[3] == 1.0 &&
            std::abs(enriched_features[4] - 0.9) < 1.0e-12 &&
            std::abs(enriched_features[5] - 1.0 / 3.0) < 1.0e-12,
        "sparse routing feature extraction lost enriched matrix statistics");

    smave::LinearSystem extreme_routing_feature_system;
    extreme_routing_feature_system.matrix = {
        {1.0e308, 0.0},
        {0.0, 1.0e-308},
    };
    extreme_routing_feature_system.right_hand_side = {1.0, 1.0};
    smave::classify_linear_system(extreme_routing_feature_system);
    require(
        std::isfinite(extreme_routing_feature_system.coefficient_dynamic_range) &&
            extreme_routing_feature_system.coefficient_dynamic_range == 1.0e32 &&
            std::isfinite(extreme_routing_feature_system.row_l1_condition_estimate) &&
            extreme_routing_feature_system.row_l1_condition_estimate == 1.0e32,
        "extreme sparse routing ratios were not bounded to finite values");
    auto extreme_profile = structural_profile;
    extreme_profile.coefficient_dynamic_range =
        std::numeric_limits<double>::infinity();
    extreme_profile.row_nonzero_coefficient_of_variation =
        std::numeric_limits<double>::infinity();
    extreme_profile.row_l1_condition_estimate =
        std::numeric_limits<double>::infinity();
    extreme_profile.diagonal_dominance_fraction =
        std::numeric_limits<double>::quiet_NaN();
    extreme_profile.mean_diagonal_row_l1_fraction =
        std::numeric_limits<double>::quiet_NaN();
    extreme_profile.normalized_mean_bandwidth =
        std::numeric_limits<double>::quiet_NaN();
    const auto bounded_features = smave::extract_sparse_routing_features(
        {"sparse:log_coefficient_dynamic_range",
         "sparse:row_nonzero_coefficient_of_variation",
         "sparse:log_row_l1_condition",
         "sparse:diagonal_dominance_fraction",
         "sparse:mean_diagonal_row_l1_fraction",
         "sparse:normalized_mean_bandwidth"},
        extreme_profile);
    require(
        std::all_of(bounded_features.begin(), bounded_features.end(), [](double value) {
            return std::isfinite(value);
        }),
        "non-finite sparse profile statistics escaped routing feature extraction");

    auto nonsymmetric_profile = structural_profile;
    nonsymmetric_profile.fingerprint = "anchor-guard";
    nonsymmetric_profile.structurally_symmetric = false;
    nonsymmetric_profile.numerically_symmetric = false;
    nonsymmetric_profile.numerically_positive_definite = false;
    smave::RequestConditionedRoutingModel guarded_model;
    guarded_model.feature_names = {"sparse:log_rows"};
    guarded_model.feature_means = {0.0};
    guarded_model.feature_scales = {1.0};
    guarded_model.actions["gmres-ilu0-cpu-v1"].push_back(
        smave::RouteActionPredictor{
            .work_iterations = 20,
            .training_samples = 8,
            .independent_training_groups = 2,
            .independent_calibration_groups = 1,
            .log_cost_coefficients = {0.0, 0.0},
            .pass_logit_coefficients = {0.0, 0.0},
            .cost_calibration_error = 1.0,
        });
    guarded_model.actions["sparse-ordered-threshold-pivot-cpu-v2"].push_back(
        smave::RouteActionPredictor{
            .work_iterations = 0,
            .training_samples = 8,
            .independent_training_groups = 2,
            .independent_calibration_groups = 1,
            .log_cost_coefficients = {std::log(3.0), 0.0},
            .pass_logit_coefficients = {std::log(99.0), 0.0},
        });
    const auto guarded_plan = smave::route_sparse_linear_system(
        nonsymmetric_profile,
        smave::RoutingConfig{
            .top_k = 2,
            .risk_weight = 0.0,
            .expert_allowlist = {
                "gmres-ilu0-cpu-v1", "sparse-ordered-threshold-pivot-cpu-v2"},
            .request_conditioned_model = guarded_model,
            .request_conditioned_family_anchors = {
                {"nonsymmetric",
                 smave::RouteActionReference{
                     .expert_version = "sparse-ordered-threshold-pivot-cpu-v2",
                     .work_iterations = 0}}},
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    require(
        guarded_plan.steps.size() == 1 &&
            guarded_plan.steps.front().expert_version ==
                "sparse-ordered-threshold-pivot-cpu-v2",
        "calibrated abstention did not retain the family anchor");

    const double normalized_log_rows = std::log1p(
        static_cast<double>(nonsymmetric_profile.rows));
    auto support_guarded_model = guarded_model;
    auto& support_candidate =
        support_guarded_model.actions["gmres-ilu0-cpu-v1"].front();
    support_candidate.pass_logit_coefficients = {std::log(99.0), 0.0};
    support_candidate.cost_calibration_error = 0.0;
    support_candidate.support_feature_minimums = {-1.0};
    support_candidate.support_feature_maximums = {1.0};
    auto& support_anchor = support_guarded_model
        .actions["sparse-ordered-threshold-pivot-cpu-v2"].front();
    support_anchor.support_feature_minimums = {normalized_log_rows - 0.1};
    support_anchor.support_feature_maximums = {normalized_log_rows + 0.1};
    const auto support_guarded_plan = smave::route_sparse_linear_system(
        nonsymmetric_profile,
        smave::RoutingConfig{
            .top_k = 2,
            .risk_weight = 0.0,
            .expert_allowlist = {
                "gmres-ilu0-cpu-v1", "sparse-ordered-threshold-pivot-cpu-v2"},
            .request_conditioned_model = support_guarded_model,
            .request_conditioned_family_anchors = {
                {"nonsymmetric",
                 smave::RouteActionReference{
                     .expert_version = "sparse-ordered-threshold-pivot-cpu-v2",
                     .work_iterations = 0}}},
            .minimum_family_anchor_gain_fraction = 0.05,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    require(
        support_guarded_plan.steps.size() == 1 &&
            support_guarded_plan.steps.front().expert_version ==
                "sparse-ordered-threshold-pivot-cpu-v2",
        "out-of-support candidate did not trigger family-anchor abstention");

    auto robust_gain_model = support_guarded_model;
    auto& robust_candidate = robust_gain_model.actions["gmres-ilu0-cpu-v1"].front();
    robust_candidate.support_feature_minimums = {normalized_log_rows - 0.1};
    robust_candidate.support_feature_maximums = {normalized_log_rows + 0.1};
    robust_candidate.cost_calibration_upper_error = 0.05;
    robust_candidate.pass_calibration_upper_error = 0.01;
    auto& robust_anchor = robust_gain_model
        .actions["sparse-ordered-threshold-pivot-cpu-v2"].front();
    robust_anchor.log_cost_coefficients = {std::log(5.0), 0.0};
    robust_anchor.cost_calibration_upper_error = 0.05;
    robust_anchor.pass_calibration_upper_error = 0.01;
    const auto robust_gain_plan = smave::route_sparse_linear_system(
        nonsymmetric_profile,
        smave::RoutingConfig{
            .top_k = 1,
            .risk_weight = 0.0,
            .expert_allowlist = {
                "gmres-ilu0-cpu-v1", "sparse-ordered-threshold-pivot-cpu-v2"},
            .request_conditioned_model = robust_gain_model,
            .request_conditioned_family_anchors = {
                {"nonsymmetric",
                 smave::RouteActionReference{
                     .expert_version = "sparse-ordered-threshold-pivot-cpu-v2",
                     .work_iterations = 0}}},
            .minimum_family_anchor_gain_fraction = 0.05,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    require(
        robust_gain_plan.steps.size() == 1 &&
            robust_gain_plan.steps.front().expert_version == "gmres-ilu0-cpu-v1",
        "in-support robust gain did not permit deviation from the family anchor");

    const auto calibration_gated_anchor_plan = smave::route_sparse_linear_system(
        nonsymmetric_profile,
        smave::RoutingConfig{
            .top_k = 1,
            .risk_weight = 0.0,
            .expert_allowlist = {
                "gmres-ilu0-cpu-v1", "sparse-ordered-threshold-pivot-cpu-v2"},
            .request_conditioned_model = robust_gain_model,
            .request_conditioned_family_anchors = {
                {"nonsymmetric",
                 smave::RouteActionReference{
                     .expert_version = "sparse-ordered-threshold-pivot-cpu-v2",
                     .work_iterations = 0}}},
            .request_conditioned_anchor_only_families = {"nonsymmetric"},
            .minimum_family_anchor_gain_fraction = 0.05,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    require(
        calibration_gated_anchor_plan.steps.size() == 1 &&
            calibration_gated_anchor_plan.steps.front().expert_version ==
                "sparse-ordered-threshold-pivot-cpu-v2",
        "calibration-level family gate did not retain the anchor");

    const auto global_fixed_fallback_plan = smave::route_sparse_linear_system(
        nonsymmetric_profile,
        smave::RoutingConfig{
            .top_k = 1,
            .risk_weight = 0.0,
            .expert_allowlist = {
                "gmres-ilu0-cpu-v1", "sparse-ordered-threshold-pivot-cpu-v2"},
            .request_conditioned_model = robust_gain_model,
            .request_conditioned_family_anchors = {
                {"nonsymmetric",
                 smave::RouteActionReference{
                     .expert_version = "sparse-ordered-threshold-pivot-cpu-v2",
                     .work_iterations = 0}}},
            .request_conditioned_global_fixed_anchor =
                smave::RouteActionReference{
                    .expert_version = "gmres-ilu0-cpu-v1",
                    .work_iterations = 20},
            .request_conditioned_anchor_only_families = {"nonsymmetric"},
            .minimum_family_anchor_gain_fraction = 0.05,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    require(
        global_fixed_fallback_plan.steps.size() == 1 &&
            global_fixed_fallback_plan.steps.front().expert_version ==
                "gmres-ilu0-cpu-v1",
        "control-aware family anchor did not fall back to global fixed");

    const auto family_anchor_gain_plan = smave::route_sparse_linear_system(
        nonsymmetric_profile,
        smave::RoutingConfig{
            .top_k = 1,
            .risk_weight = 0.0,
            .expert_allowlist = {
                "gmres-ilu0-cpu-v1", "sparse-ordered-threshold-pivot-cpu-v2"},
            .request_conditioned_model = robust_gain_model,
            .request_conditioned_family_anchors = {
                {"nonsymmetric",
                 smave::RouteActionReference{
                     .expert_version = "gmres-ilu0-cpu-v1",
                     .work_iterations = 20}}},
            .request_conditioned_global_fixed_anchor =
                smave::RouteActionReference{
                    .expert_version = "sparse-ordered-threshold-pivot-cpu-v2",
                    .work_iterations = 0},
            .request_conditioned_anchor_only_families = {"nonsymmetric"},
            .minimum_family_anchor_gain_fraction = 0.05,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    require(
        family_anchor_gain_plan.steps.size() == 1 &&
            family_anchor_gain_plan.steps.front().expert_version ==
                "gmres-ilu0-cpu-v1",
        "control-aware family anchor discarded a robust family gain");

    const auto terminal_anchor_plan = smave::route_sparse_linear_system(
        nonsymmetric_profile,
        smave::RoutingConfig{
            .top_k = 1,
            .risk_weight = 0.0,
            .expert_allowlist = {
                "gmres-ilu0-cpu-v1", "sparse-ordered-threshold-pivot-cpu-v2"},
            .request_conditioned_model = robust_gain_model,
            .request_conditioned_family_anchors = {
                {"nonsymmetric", smave::RouteActionReference{}}},
            .minimum_family_anchor_gain_fraction = 0.95,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    require(terminal_anchor_plan.steps.empty(),
            "terminal family anchor did not abstain to the terminal numerical cascade");

    auto unavailable_anchor_model = robust_gain_model;
    auto unavailable_anchor_predictor = robust_candidate;
    unavailable_anchor_predictor.work_iterations = 20;
    unavailable_anchor_model.actions["pcg-ic0-cpu-v1"] = {
        unavailable_anchor_predictor};
    const auto unavailable_anchor_plan = smave::route_sparse_linear_system(
        nonsymmetric_profile,
        smave::RoutingConfig{
            .top_k = 1,
            .risk_weight = 0.0,
            .expert_allowlist = {
                "gmres-ilu0-cpu-v1", "pcg-ic0-cpu-v1",
                "sparse-ordered-threshold-pivot-cpu-v2"},
            .request_conditioned_model = unavailable_anchor_model,
            .request_conditioned_family_anchors = {
                {"nonsymmetric",
                 smave::RouteActionReference{
                     .expert_version = "pcg-ic0-cpu-v1",
                     .work_iterations = 20}}},
            .minimum_family_anchor_gain_fraction = 0.95,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    require(unavailable_anchor_plan.steps.empty(),
            "structurally unavailable family anchor did not use the terminal cascade");

    bool rejected_unmodeled_anchor = false;
    try {
        (void)smave::RuntimeRouter(smave::RoutingConfig{
            .request_conditioned_model = robust_gain_model,
            .request_conditioned_family_anchors = {
                {"nonsymmetric",
                 smave::RouteActionReference{
                     .expert_version = "missing-anchor",
                     .work_iterations = 0}}},
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    } catch (const std::invalid_argument&) {
        rejected_unmodeled_anchor = true;
    }
    require(rejected_unmodeled_anchor,
            "request-conditioned router accepted an unmodeled family anchor");

    bool rejected_conditioned_upper_error = false;
    try {
        auto invalid_model = conditioned_model;
        invalid_model.actions["candidate"].front().cost_calibration_upper_error = -1.0;
        (void)smave::RuntimeRouter(smave::RoutingConfig{
            .request_conditioned_model = invalid_model,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    } catch (const std::invalid_argument&) {
        rejected_conditioned_upper_error = true;
    }
    require(rejected_conditioned_upper_error,
            "request-conditioned router accepted invalid upper calibration error");

    bool rejected_conditioned_support = false;
    try {
        auto invalid_model = conditioned_model;
        invalid_model.actions["candidate"].front().support_feature_minimums = {1.0};
        invalid_model.actions["candidate"].front().support_feature_maximums = {0.0};
        (void)smave::RuntimeRouter(smave::RoutingConfig{
            .request_conditioned_model = invalid_model,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    } catch (const std::invalid_argument&) {
        rejected_conditioned_support = true;
    }
    require(rejected_conditioned_support,
            "request-conditioned router accepted invalid support bounds");

    bool rejected_anchor_gain = false;
    try {
        (void)smave::RuntimeRouter(smave::RoutingConfig{
            .minimum_family_anchor_gain_fraction = 1.0,
        });
    } catch (const std::invalid_argument&) {
        rejected_anchor_gain = true;
    }
    require(rejected_anchor_gain,
            "router accepted an out-of-range family-anchor gain fraction");
    bool rejected_conditioned_dimension = false;
    try {
        (void)smave::predict_request_conditioned_action(
            conditioned_model, "candidate", 2, {});
    } catch (const std::invalid_argument&) {
        rejected_conditioned_dimension = true;
    }
    require(rejected_conditioned_dimension,
            "request-conditioned model accepted a feature dimension mismatch");
    bool rejected_conditioned_scale = false;
    try {
        auto invalid_model = conditioned_model;
        invalid_model.feature_scales.front() = 0.0;
        (void)smave::RuntimeRouter(smave::RoutingConfig{
            .request_conditioned_model = invalid_model,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    } catch (const std::invalid_argument&) {
        rejected_conditioned_scale = true;
    }
    require(rejected_conditioned_scale,
            "request-conditioned router accepted a zero feature scale");
    bool rejected_conditioned_duplicate = false;
    try {
        auto invalid_model = conditioned_model;
        invalid_model.actions["candidate"].push_back(
            invalid_model.actions["candidate"].front());
        (void)smave::RuntimeRouter(smave::RoutingConfig{
            .request_conditioned_model = invalid_model,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    } catch (const std::invalid_argument&) {
        rejected_conditioned_duplicate = true;
    }
    require(rejected_conditioned_duplicate,
            "request-conditioned router accepted a duplicate expert-budget action");
    bool rejected_conditioned_nonfinite = false;
    try {
        auto invalid_model = conditioned_model;
        invalid_model.actions["candidate"].front().log_cost_coefficients.front() =
            std::numeric_limits<double>::infinity();
        (void)smave::RuntimeRouter(smave::RoutingConfig{
            .request_conditioned_model = invalid_model,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    } catch (const std::invalid_argument&) {
        rejected_conditioned_nonfinite = true;
    }
    require(rejected_conditioned_nonfinite,
            "request-conditioned router accepted nonfinite coefficients");
    bool rejected_conditioned_offset = false;
    try {
        auto invalid_model = conditioned_model;
        invalid_model.actions["candidate"].front().log_cost_calibration_offset =
            std::numeric_limits<double>::infinity();
        (void)smave::RuntimeRouter(smave::RoutingConfig{
            .request_conditioned_model = invalid_model,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    } catch (const std::invalid_argument&) {
        rejected_conditioned_offset = true;
    }
    require(rejected_conditioned_offset,
            "request-conditioned router accepted a nonfinite calibration offset");
    bool rejected_conditioned_calibration_error = false;
    try {
        auto invalid_model = conditioned_model;
        invalid_model.actions["candidate"].front().pass_calibration_error = 2.0;
        (void)smave::RuntimeRouter(smave::RoutingConfig{
            .request_conditioned_model = invalid_model,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    } catch (const std::invalid_argument&) {
        rejected_conditioned_calibration_error = true;
    }
    require(rejected_conditioned_calibration_error,
            "request-conditioned router accepted invalid calibration error");
    bool rejected_conditioned_feature = false;
    try {
        (void)smave::train_request_conditioned_routing_model(
            {"unsupported:feature"}, conditioned_training, conditioned_calibration);
    } catch (const std::invalid_argument&) {
        rejected_conditioned_feature = true;
    }
    require(rejected_conditioned_feature,
            "request-conditioned trainer accepted an unsupported feature");
    bool rejected_conditioned_fallback = false;
    try {
        (void)smave::RuntimeRouter(smave::RoutingConfig{
            .require_original_fallback = false,
            .request_conditioned_model = conditioned_model,
            .calibrated_terminal_fallback_cost_us = 10.0,
        });
    } catch (const std::invalid_argument&) {
        rejected_conditioned_fallback = true;
    }
    require(rejected_conditioned_fallback,
            "request-conditioned router bypassed the original fallback invariant");
}

void test_equation_assessment_and_backend_routing(const std::filesystem::path& root) {
    const auto symmetric_source = root / "AssessmentSymmetric.mo";
    std::ofstream(symmetric_source)
        << "model AssessmentSymmetric\n"
        << "parameter Real b1=1; parameter Real b2=2; parameter Real b3=3; parameter Real b4=4;\n"
        << "Real x1; Real x2; Real x3; Real x4;\n"
        << "equation\n"
        << "4*x1-x2=b1;\n-x1+4*x2-x3=b2;\n-x2+4*x3-x4=b3;\n-x3+4*x4=b4;\n"
        << "end AssessmentSymmetric;\n";
    const auto symmetric_model = smave::compile_model(symmetric_source);
    const auto& symmetric_block = symmetric_model.blocks.front();
    const auto symmetric_assessment = smave::assess_equation(symmetric_block);
    require(
        symmetric_assessment.equation_family == "linear-structurally-symmetric" &&
            symmetric_assessment.structurally_square &&
            symmetric_assessment.structurally_symmetric &&
            symmetric_assessment.runtime_positive_definite_check_required,
        "equation expert failed to identify a structurally symmetric linear family");

    const auto symmetric_registry = smave::make_default_registry(symmetric_model);
    const auto symmetric_bundle = smave::make_default_bundle(symmetric_model);
    const auto symmetric_candidates = smave::CompileRouter{}.lookup(
        symmetric_block, symmetric_registry, symmetric_bundle);
    const auto has_candidate = [](const auto& candidates, const std::string& version) {
        return std::any_of(candidates.begin(), candidates.end(), [&](const auto& candidate) {
            return candidate.expert_version == version;
        });
    };
    require(
        has_candidate(symmetric_candidates, "pcg-ic0-cpu-v1") &&
            has_candidate(symmetric_candidates, "gmres-ilut-cpu-v1") &&
            has_candidate(symmetric_candidates, "dense-direct-cpu-v1"),
        "symmetric assessment did not produce a mixed Krylov/direct backend portfolio");
    const auto symmetric_plan = smave::RuntimeRouter{}.route(
        symmetric_block, {}, symmetric_candidates, symmetric_registry, symmetric_bundle);
    require(
        symmetric_plan.assessment.equation_family ==
                symmetric_assessment.equation_family &&
            std::all_of(
                symmetric_plan.steps.begin(), symmetric_plan.steps.end(),
                [](const smave::SolveStep& step) {
                    return !step.selection_reason.empty() &&
                        step.backend_role == smave::BackendRole::linear_solver;
                }),
        "SolvePlan did not retain the assessment and auditable backend reasons");

    const auto nonsymmetric_source = root / "AssessmentNonsymmetric.mo";
    std::ofstream(nonsymmetric_source)
        << "model AssessmentNonsymmetric\n"
        << "parameter Real b1=1; parameter Real b2=2; parameter Real b3=3; parameter Real b4=4;\n"
        << "Real x1; Real x2; Real x3; Real x4;\n"
        << "equation\n"
        << "2*x1+x2=b1;\n3*x2+x3=b2;\n4*x3+x4=b3;\nx1+5*x4=b4;\n"
        << "end AssessmentNonsymmetric;\n";
    const auto nonsymmetric_model = smave::compile_model(nonsymmetric_source);
    const auto& nonsymmetric_block = nonsymmetric_model.blocks.front();
    const auto nonsymmetric_assessment = smave::assess_equation(nonsymmetric_block);
    require(
        nonsymmetric_assessment.linear &&
            !nonsymmetric_assessment.structurally_symmetric &&
            nonsymmetric_assessment.equation_family.find("nonsymmetric") !=
                std::string::npos,
        "equation expert failed to identify a nonsymmetric linear family");
    const auto nonsymmetric_registry = smave::make_default_registry(nonsymmetric_model);
    const auto nonsymmetric_bundle = smave::make_default_bundle(nonsymmetric_model);
    const auto nonsymmetric_candidates = smave::CompileRouter{}.lookup(
        nonsymmetric_block, nonsymmetric_registry, nonsymmetric_bundle);
    require(
        !has_candidate(nonsymmetric_candidates, "pcg-ic0-cpu-v1") &&
            !has_candidate(nonsymmetric_candidates, "pcg-jacobi-cpu-v1") &&
            has_candidate(nonsymmetric_candidates, "gmres-ilut-cpu-v1") &&
            has_candidate(nonsymmetric_candidates, "dense-direct-cpu-v1"),
        "nonsymmetric assessment did not exclude CG while preserving safe backends");
}

void test_strict_configuration(const std::filesystem::path& root) {
    const auto valid_path = root / "valid.yaml";
    std::ofstream(valid_path)
        << "schema_version: smave.config.v1\n"
        << "tolerance:\n  residual:\n    relative: 1e-8\n    absolute: 1e-10\n"
        << "  qoi:\n    default_relative: 1e-4\n"
        << "routing:\n  top_k: 2\n  require_original_fallback: true\n"
        << "safety:\n  ood_policy: fallback\n  event_policy: original\n"
        << "  online_learning: false\n"
        << "telemetry:\n  trace: true\n  retain_context: redacted\n";
    const auto valid = smave::RuntimeConfig::read(valid_path);
    require(valid.routing.top_k == 2, "valid routing config was not loaded");

    auto expect_rejected = [&](const std::string& name, const std::string& content) {
        const auto path = root / name;
        std::ofstream(path) << content;
        bool rejected = false;
        try { (void)smave::RuntimeConfig::read(path); }
        catch (const std::exception&) { rejected = true; }
        require(rejected, name + " was silently accepted");
    };
    expect_rejected(
        "unknown.yaml",
        "schema_version: smave.config.v1\nsafety:\n  allow_unsafe: true\n");
    expect_rejected(
        "fallback-off.yaml",
        "schema_version: smave.config.v1\nrouting:\n  require_original_fallback: false\n");
    expect_rejected(
        "loose-qoi.yaml",
        "schema_version: smave.config.v1\ntolerance:\n  qoi:\n    default_relative: 0.001\n");
    expect_rejected(
        "online.yaml",
        "schema_version: smave.config.v1\nsafety:\n  online_learning: true\n");
}

void test_validation_accounting(const std::filesystem::path& root) {
    const auto source = root / "Validation.mo";
    std::ofstream(source)
        << "model Validation\nparameter Real p = 2;\n"
        << "Real x(start=1,min=0);\nReal y(start=1,min=0);\n"
        << "equation\nx*x+y=p+4;\nx+y*y=p+4;\nend Validation;\n";
    const auto model = smave::compile_model(source);
    const smave::Runtime runtime(model);
    const auto scenarios = root / "scenario-suite";
    std::filesystem::create_directories(scenarios);
    std::ofstream(scenarios / "one.conf") << "p=2\nx_previous=1.9\ny_previous=2.1\n";
    std::ofstream(scenarios / "two.conf") << "p=2\nx_previous=2.1\ny_previous=1.9\n";
    const auto report = smave::validate_scenarios(runtime, scenarios, root / "validation-traces");
    require(report.scenarios == 2 && report.admitted_invocations == 2,
            "validation invocation denominator is incorrect");
    require(report.top_k_passes == 2 && report.top_k_pass_rate == 1.0,
            "validation Top-k accounting is incorrect");
    require(report.erroneous_accepts == 0 && report.safety_evaluations == 2 &&
                report.safety_target_met && !report.confidence_target_met &&
                report.erroneous_accept_rate_upper_bound > 0.77 &&
                report.erroneous_accept_rate_upper_bound < 0.78,
            "validation safety accounting is incorrect");
    require(std::abs(smave::binomial_proportion_upper_bound(0, 64) -
                (1.0 - std::pow(0.05, 1.0 / 64.0))) < 1.0e-12 &&
                smave::binomial_proportion_upper_bound(0, 64) < 0.05 &&
                smave::binomial_proportion_upper_bound(1, 64) > 0.05,
            "exact one-sided binomial safety bound is incorrect");
    const auto report_path = root / "validation.txt";
    smave::write_validation_report(report, report_path);
    require(std::filesystem::exists(report_path), "validation report was not persisted");
}

void test_affine_learning_and_ood(const std::filesystem::path& root) {
    const auto source = root / "Learning.mo";
    std::ofstream(source)
        << "model Learning\nparameter Real p = 2;\n"
        << "Real x(start=1,min=0);\nReal y(start=1,min=0);\n"
        << "equation\nx*x+y=p+4;\nx+y*y=p+4;\nend Learning;\n";
    const auto model = smave::compile_model(source);
    const auto training = root / "learning-training";
    std::filesystem::create_directories(training);
    for (int value = 0; value <= 4; ++value) {
        std::ofstream(training / ("p-" + std::to_string(value) + ".conf"))
            << "p=" << value << '\n';
    }
    const auto artifact = smave::train_affine_warm_start(
        model, "block-1", training, root / "label-traces");
    require(artifact.training_samples == 5 && artifact.training_rmse < 0.05,
            "affine training did not produce useful evidence");
    const auto artifact_path = root / "affine.expert";
    artifact.write(artifact_path);
    const auto restored = smave::AffineWarmStartArtifact::read(artifact_path);
    require(restored.artifact_hash == artifact.artifact_hash,
            "affine artifact roundtrip lost integrity");

    auto lineage_artifact = artifact;
    lineage_artifact.schema_version = "smave.affine-warm-start.v2";
    lineage_artifact.training_dataset_id = "affine-training";
    lineage_artifact.training_dataset_version = std::string(64, 'a');
    lineage_artifact.training_dataset_manifest_hash = std::string(64, 'b');
    lineage_artifact.expert_version.clear();
    lineage_artifact.seal();
    const auto lineage_path = root / "affine-lineage.expert";
    lineage_artifact.write(lineage_path);
    const auto restored_lineage = smave::AffineWarmStartArtifact::read(lineage_path);
    require(restored_lineage.schema_version == "smave.affine-warm-start.v2" &&
                restored_lineage.training_dataset_id == "affine-training",
            "affine v2 training lineage roundtrip failed");
    const auto lineage_certificate = smave::verify_affine_warm_start(
        model, lineage_artifact, 0, root / "affine-lineage-probes");
    require(lineage_certificate.schema_version == "smave.verified-cells.v2" &&
                lineage_certificate.training_dataset_version ==
                    lineage_artifact.training_dataset_version,
            "affine verification certificate did not inherit training lineage");
    smave::AffineWarmStartExpert lineage_expert(lineage_artifact, lineage_certificate);
    auto mismatched_certificate = lineage_certificate;
    mismatched_certificate.training_dataset_version = std::string(64, 'c');
    mismatched_certificate.seal();
    bool lineage_mismatch_rejected = false;
    try {
        smave::AffineWarmStartExpert mismatched(
            lineage_artifact, mismatched_certificate);
    } catch (const std::invalid_argument&) {
        lineage_mismatch_rejected = true;
    }
    require(lineage_mismatch_rejected,
            "affine expert accepted a certificate for another training snapshot");

    auto registry = smave::make_default_registry(model);
    smave::register_affine_expert(registry, artifact);
    auto bundle = smave::make_default_bundle(model);
    bundle.add_expert(
        artifact.expert_version,
        artifact.artifact_hash,
        "original-solver-labels-5");
    const smave::Runtime learned(model, std::move(registry), bundle);
    const auto outcome = learned.solve({{"p", 2.0}}, root / "learned-traces");
    require(outcome.success && outcome.blocks.front().attempted_experts.front() == artifact.expert_version,
            "learned expert was not selected first in-domain");
    require(outcome.blocks.front().expert_iterations <= 2,
            "learned warm-start did not reduce corrector work");

    auto ood_registry = smave::make_default_registry(model);
    smave::register_affine_expert(ood_registry, artifact);
    const smave::CompileRouter compile_router;
    const auto candidates = compile_router.lookup(
        model.blocks.front(), ood_registry, bundle);
    const smave::RuntimeRouter runtime_router;
    smave::BlockContext ood_context;
    ood_context.values["p"] = 10.0;
    const auto ood_plan = runtime_router.route(
        model.blocks.front(), ood_context, candidates, ood_registry, bundle);
    require(std::none_of(
                ood_plan.steps.begin(), ood_plan.steps.end(),
                [&](const smave::SolveStep& step) {
                    return step.expert_version == artifact.expert_version;
                }),
            "OOD learned expert was not removed from the runtime plan");

    auto tampered = artifact;
    tampered.coefficients.front().front() += 1.0;
    bool rejected = false;
    try { tampered.validate(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "tampered affine artifact was accepted");

    auto mismatched_bundle = bundle;
    mismatched_bundle.expert_artifact_hashes.back() = "wrong-artifact-hash";
    mismatched_bundle.seal();
    auto mismatch_registry = smave::make_default_registry(model);
    smave::register_affine_expert(mismatch_registry, artifact);
    bool mismatch_rejected = false;
    try { mismatch_registry.validate_bundle(mismatched_bundle, model); }
    catch (const std::invalid_argument&) { mismatch_rejected = true; }
    require(mismatch_rejected, "bundle accepted a mismatched expert artifact hash");
}

void test_krylov_and_direct_cascade(const std::filesystem::path& root) {
    const auto spd_source = root / "Spd.mo";
    std::ofstream spd(spd_source);
    spd << "model Spd\n";
    for (int index = 1; index <= 8; ++index) {
        spd << "parameter Real b" << index << " = 1;\n";
    }
    for (int index = 1; index <= 8; ++index) {
        spd << "Real x" << index << "(start=0);\n";
    }
    spd << "equation\n";
    spd << "4*x1-x2=b1;\n";
    for (int index = 2; index <= 7; ++index) {
        spd << "-x" << index - 1 << "+4*x" << index << "-x" << index + 1
            << "=b" << index << ";\n";
    }
    spd << "-x7+4*x8=b8;\nend Spd;\n";
    spd.close();
    const auto spd_model = smave::compile_model(spd_source);
    require(spd_model.blocks.size() == 1 && spd_model.blocks.front().unknowns.size() == 8,
            "SPD system was not retained as one SCC");
    const auto spd_outcome = smave::Runtime(spd_model).solve({}, root / "spd-traces");
    require(spd_outcome.success, "PCG failed on SPD system");
    const auto& spd_block = spd_outcome.blocks.front();
    require(spd_block.attempted_experts.front() == "structured-tridiagonal-direct-cpu-v1",
            "tridiagonal SPD block was not routed to the structural direct backend first");
    require(spd_block.linear_spd &&
                spd_block.gate.decision == smave::GateDecision::direct_accept,
            "structured SPD direct path did not preserve gate telemetry");
    require(spd_block.attempted_experts.size() == 1,
            "successful structural direct solve unnecessarily invoked another backend");

    const auto indefinite_source = root / "Indefinite.mo";
    std::ofstream(indefinite_source)
        << "model Indefinite\n"
        << "parameter Real b1=1; parameter Real b2=2; parameter Real b3=3; parameter Real b4=4;\n"
        << "Real x1; Real x2; Real x3; Real x4;\n"
        << "equation\n"
        << "x1+2*x2+2*x4=b1;\n"
        << "2*x1+x2+2*x3=b2;\n"
        << "2*x2+x3+2*x4=b3;\n"
        << "2*x1+2*x3+x4=b4;\n"
        << "end Indefinite;\n";
    const auto indefinite_model = smave::compile_model(indefinite_source);
    const auto indefinite_outcome = smave::Runtime(indefinite_model).solve(
        {}, root / "indefinite-traces");
    require(indefinite_outcome.success, "direct solver failed after PCG rejection");
    const auto& indefinite_block = indefinite_outcome.blocks.front();
    require(!indefinite_block.linear_spd, "indefinite system was incorrectly marked SPD");
    require(
        (indefinite_block.attempted_experts.front() ==
             "structured-tridiagonal-direct-cpu-v1" ||
         indefinite_block.attempted_experts.front() ==
             "accelerate-sparse-qr-cpu-v1" ||
         indefinite_block.attempted_experts.front() ==
             "sparse-ordered-threshold-pivot-cpu-v2" ||
         indefinite_block.attempted_experts.front() == "dense-direct-cpu-v1") &&
            std::none_of(
                indefinite_block.attempted_experts.begin(),
                indefinite_block.attempted_experts.end(),
                [](const std::string& version) {
                    return version.starts_with("pcg-") || version.starts_with("gmres-");
                }),
        "numeric probe did not remove ineligible Krylov backends before execution");
    require(indefinite_block.path == smave::SolvePath::direct_accept,
            "classic direct result did not pass the independent gate");
}

void test_learned_linear_preconditioner(const std::filesystem::path& root) {
    const auto source = root / "LearnedSpd.mo";
    std::ofstream model_file(source);
    model_file << "model LearnedSpd\n";
    for (int index = 1; index <= 8; ++index) {
        model_file << "parameter Real b" << index << " = 1;\n";
    }
    for (int index = 1; index <= 8; ++index) {
        model_file << "Real x" << index << "(start=0);\n";
    }
    model_file << "equation\n4*x1-x2=b1;\n";
    for (int index = 2; index <= 7; ++index) {
        model_file << "-x" << index - 1 << "+4*x" << index << "-x" << index + 1
                   << "=b" << index << ";\n";
    }
    model_file << "-x7+4*x8=b8;\nend LearnedSpd;\n";
    model_file.close();
    const auto model = smave::compile_model(source);
    const auto training = root / "pc-training";
    std::filesystem::create_directories(training);
    for (int sample = 1; sample <= 3; ++sample) {
        std::ofstream scenario(training / ("sample-" + std::to_string(sample) + ".conf"));
        for (int index = 1; index <= 8; ++index) {
            scenario << "b" << index << '=' << sample + index * 0.1 << '\n';
        }
    }
    const auto artifact = smave::train_linear_preconditioner(
        model, "block-1", training);
    require(artifact.training_samples == 3 && artifact.maximum_matrix_drift < 1.0e-12,
            "linear preconditioner training evidence is incorrect");
    const auto artifact_path = root / "linear-pc.expert";
    artifact.write(artifact_path);
    const auto restored = smave::LinearPreconditionerArtifact::read(artifact_path);
    require(restored.artifact_hash == artifact.artifact_hash,
            "linear preconditioner artifact roundtrip failed");

    auto registry = smave::make_default_registry(model);
    smave::register_linear_preconditioner(registry, artifact);
    auto bundle = smave::make_default_bundle(model);
    bundle.add_expert(
        artifact.expert_version,
        artifact.artifact_hash,
        "matrix-residual-traces-3");
    std::unordered_map<std::string, double> scenario;
    for (int index = 1; index <= 8; ++index) {
        scenario["b" + std::to_string(index)] = 2.0 + index * 0.1;
    }
    smave::RoutingConfig learned_only;
    learned_only.expert_allowlist.insert(artifact.expert_version);
    const auto outcome = smave::Runtime(
        model, std::move(registry), bundle, {}, learned_only).solve(
            scenario, root / "learned-pc-traces");
    require(outcome.success, "learned preconditioner solve failed");
    const auto& block = outcome.blocks.front();
    require(block.attempted_experts.front() == artifact.expert_version,
            "learned preconditioner was not routed first");
    require(block.path == smave::SolvePath::corrected_accept &&
                block.preconditioner_version == artifact.expert_version,
            "learned preconditioner result did not retain corrected permission");
    require(block.krylov_iterations <= 1 && block.krylov_final_residual < 1.0e-8,
            "learned inverse action did not accelerate Krylov convergence");
    require(block.attempted_experts.size() == 1,
            "successful learned preconditioner unnecessarily cascaded");

    auto competition_registry = smave::make_default_registry(model);
    smave::register_linear_preconditioner(competition_registry, artifact);
    const auto competition = smave::compete_experts(
        model,
        competition_registry,
        bundle,
        training,
        root / "linear-competition-traces");
    const auto has_entry = [&](const std::string& version) {
        return std::any_of(
            competition.entries.begin(), competition.entries.end(),
            [&](const smave::CompetitionEntry& entry) {
                return entry.expert_version == version;
            });
    };
    require(has_entry(artifact.expert_version) && has_entry("pcg-ic0-cpu-v1") &&
                has_entry("pcg-jacobi-cpu-v1") && has_entry("dense-direct-cpu-v1") &&
                has_entry(bundle.terminal_fallback),
            "linear competition omitted learned, classic, or terminal experts");
    const auto linear_winner = std::find_if(
        competition.entries.begin(), competition.entries.end(),
        [&](const smave::CompetitionEntry& entry) {
            return entry.expert_version == competition.winner;
        });
    require(linear_winner != competition.entries.end() &&
                linear_winner->passes == linear_winner->attempts &&
                linear_winner->failures == 0,
            "linear competition selected an unsafe winner");

    auto ood_registry = smave::make_default_registry(model);
    smave::register_linear_preconditioner(ood_registry, artifact);
    const auto candidates = smave::CompileRouter{}.lookup(
        model.blocks.front(), ood_registry, bundle);
    smave::BlockContext ood_context;
    for (int index = 1; index <= 8; ++index) {
        ood_context.values["b" + std::to_string(index)] = 100.0;
    }
    const auto ood_plan = smave::RuntimeRouter{}.route(
        model.blocks.front(), ood_context, candidates, ood_registry, bundle);
    require(std::none_of(
                ood_plan.steps.begin(), ood_plan.steps.end(),
                [&](const smave::SolveStep& step) {
                    return step.expert_version == artifact.expert_version;
                }),
            "OOD learned preconditioner was not removed from the plan");

    auto unsafe = artifact;
    unsafe.inverse_operator[0][0] = -1.0;
    unsafe.seal();
    bool rejected = false;
    try { unsafe.validate(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "non-SPD learned preconditioner artifact was accepted");
}

void test_learned_multigrid(const std::filesystem::path& root) {
    const auto source = root / "LearnedMultigrid.mo";
    std::ofstream model_file(source);
    model_file << "model LearnedMultigrid\n";
    for (int index = 1; index <= 8; ++index) {
        model_file << "parameter Real b" << index << " = 1;\n"
                   << "Real x" << index << "(start=0);\n";
    }
    model_file << "equation\n4*x1-x2=b1;\n";
    for (int index = 2; index <= 7; ++index) {
        model_file << "-x" << index - 1 << "+4*x" << index << "-x" << index + 1
                   << "=b" << index << ";\n";
    }
    model_file << "-x7+4*x8=b8;\nend LearnedMultigrid;\n";
    model_file.close();
    const auto model = smave::compile_model(source);
    const auto training = root / "multigrid-training";
    std::filesystem::create_directories(training);
    for (int sample = 1; sample <= 3; ++sample) {
        std::ofstream scenario(training / ("sample-" + std::to_string(sample) + ".conf"));
        for (int index = 1; index <= 8; ++index) {
            scenario << "b" << index << '=' << sample + index * 0.1 << '\n';
        }
    }
    const auto artifact = smave::train_learned_multigrid(model, "block-1", training);
    require(artifact.training_samples == 3 &&
                artifact.maximum_probe_contraction < 1.0 &&
                artifact.level_operators.size() == 2 &&
                artifact.level_operators[0].size() == 8 &&
                artifact.level_operators[1].size() == 4 &&
                artifact.level_prolongations.size() == 1 &&
                artifact.coarse_inverse.size() == 4,
            "learned multigrid training did not produce contraction evidence");
    const auto artifact_path = root / "learned-multigrid.expert";
    artifact.write(artifact_path);
    const auto restored = smave::LearnedMultigridArtifact::read(artifact_path);
    require(restored.artifact_hash == artifact.artifact_hash &&
                restored.smoothing_weight == artifact.smoothing_weight &&
                restored.level_operators.size() == artifact.level_operators.size(),
            "learned multigrid artifact roundtrip failed");

    auto registry = smave::make_default_registry(model);
    smave::register_learned_multigrid(registry, artifact);
    auto bundle = smave::make_default_bundle(model);
    bundle.add_expert(artifact.expert_version, artifact.artifact_hash,
                      "multigrid-contraction-probes-3");
    std::unordered_map<std::string, double> scenario;
    for (int index = 1; index <= 8; ++index) {
        scenario["b" + std::to_string(index)] = 2.0 + index * 0.1;
    }
    smave::RoutingConfig learned_only;
    learned_only.expert_allowlist.insert(artifact.expert_version);
    const auto outcome = smave::Runtime(
        model, std::move(registry), bundle, {}, learned_only).solve(
            scenario, root / "learned-multigrid-traces");
    require(outcome.success && !outcome.blocks.empty() &&
                outcome.blocks.front().attempted_experts.front() == artifact.expert_version &&
                outcome.blocks.front().path == smave::SolvePath::corrected_accept &&
                outcome.blocks.front().preconditioner_version == artifact.expert_version &&
                outcome.blocks.front().krylov_final_residual < 1.0e-8,
            "learned multigrid did not pass the independent PCG residual gate");

    auto ood_registry = smave::make_default_registry(model);
    smave::register_learned_multigrid(ood_registry, artifact);
    const auto candidates = smave::CompileRouter{}.lookup(
        model.blocks.front(), ood_registry, bundle);
    smave::BlockContext ood_context;
    for (int index = 1; index <= 8; ++index) ood_context.values["b" + std::to_string(index)] = 100.0;
    const auto ood_plan = smave::RuntimeRouter{}.route(
        model.blocks.front(), ood_context, candidates, ood_registry, bundle);
    require(std::none_of(ood_plan.steps.begin(), ood_plan.steps.end(),
                         [&](const smave::SolveStep& step) {
                             return step.expert_version == artifact.expert_version;
                         }),
            "OOD learned multigrid was not removed from the route");

    auto tampered = artifact;
    tampered.smoothing_weight = 1.9;
    tampered.seal();
    bool rejected = false;
    try { tampered.validate(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "tampered learned multigrid artifact was accepted");
}

void test_nonlinear_jacobian_multigrid(const std::filesystem::path& root) {
    const auto source = root / "NonlinearJacobianMultigrid.mo";
    std::ofstream model_file(source);
    model_file << "model NonlinearJacobianMultigrid\n";
    for (int index = 1; index <= 8; ++index) {
        model_file << "parameter Real b" << index << " = 1;\n"
                   << "Real x" << index << "(start=1);\n";
    }
    model_file << "equation\nx1*x1*x1+1.5*x1-x2=b1;\n";
    for (int index = 2; index <= 7; ++index) {
        model_file << "x" << index << "*x" << index << "*x" << index
                   << "+1.5*x" << index << "-x" << index - 1 << "-x" << index + 1
                   << "=b" << index << ";\n";
    }
    model_file << "x8*x8*x8+1.5*x8-x7=b8;\nend NonlinearJacobianMultigrid;\n";
    model_file.close();
    const auto model = smave::compile_model(source);
    require(!model.blocks.front().linear && model.blocks.front().smooth,
            "nonlinear multigrid fixture was not compiled as a smooth nonlinear block");
    const auto training = root / "nonlinear-multigrid-training";
    std::filesystem::create_directories(training);
    for (int sample = 1; sample <= 3; ++sample) {
        std::ofstream scenario(training / ("sample-" + std::to_string(sample) + ".conf"));
        for (int index = 1; index <= 8; ++index) {
            scenario << "b" << index << '=' << 0.8 + sample * 0.1 + index * 0.01 << '\n';
        }
    }
    const auto artifact = smave::train_learned_multigrid(model, "block-1", training);
    require(artifact.jacobian_mode && artifact.maximum_probe_contraction < 1.0,
            "nonlinear Jacobian multigrid lacks contraction evidence");
    auto registry = smave::make_default_registry(model);
    smave::register_learned_multigrid(registry, artifact);
    auto bundle = smave::make_default_bundle(model);
    bundle.add_expert(
        artifact.expert_version, artifact.artifact_hash,
        registry.grant(artifact.expert_version).evidence_bundle);
    std::unordered_map<std::string, double> scenario;
    for (int index = 1; index <= 8; ++index) {
        scenario["b" + std::to_string(index)] = 1.0 + index * 0.01;
    }
    const auto accelerated = smave::Runtime(model, registry, bundle).solve(
        [&] {
            auto values = scenario;
            for (int index = 1; index <= 8; ++index) {
                values["x" + std::to_string(index) + "_previous"] = 1.0;
            }
            return values;
        }(),
        root / "nonlinear-multigrid-traces");
    require(accelerated.success && accelerated.corrected_count == 1 &&
                accelerated.fallback_count == 0 &&
                accelerated.blocks.front().preconditioner_version == artifact.expert_version &&
                accelerated.blocks.front().krylov_iterations > 0 &&
                accelerated.blocks.front().krylov_final_residual <
                    accelerated.blocks.front().krylov_initial_residual,
            "nonlinear Newton did not use learned multigrid with true Krylov residuals");

    auto unsafe_initial = scenario;
    for (int index = 1; index <= 8; ++index) {
        unsafe_initial["x" + std::to_string(index) + "_previous"] = 0.0;
    }
    const auto fallback = smave::Runtime(model, std::move(registry), bundle).solve(
        unsafe_initial, root / "nonlinear-multigrid-fallback-traces");
    require(fallback.success && fallback.fallback_count == 1 &&
                fallback.blocks.front().path == smave::SolvePath::full_fallback &&
                fallback.blocks.front().attempted_experts.front() == artifact.expert_version &&
                fallback.blocks.front().attempt_records.front().outcome == "rejected" &&
                fallback.blocks.front().attempt_records.front().reason.find("non-SPD") !=
                    std::string::npos,
            "non-SPD Newton Jacobian did not reject multigrid and use original fallback");
}

void test_nonsymmetric_gmres_ilut(const std::filesystem::path& root) {
    smave::LinearSystem system;
    system.unknowns = {"x1", "x2", "x3", "x4"};
    system.matrix = {
        {4.0, -1.0, 0.0, 0.0},
        {2.0, 4.0, -1.0, 0.0},
        {0.0, 2.0, 4.0, -1.0},
        {0.0, 0.0, 2.0, 4.0},
    };
    system.right_hand_side = {3.0, 5.0, 5.0, 6.0};
    system.symmetric = false;
    std::vector<std::vector<int>> dense_sparsity(4, std::vector<int>(4));
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            dense_sparsity[row][column] = system.matrix[row][column] != 0.0;
        }
    }
    const auto sparsity = smave::SparsityPattern::from_dense(dense_sparsity);
    const auto colors = sparsity.greedy_column_coloring();
    require(colors.size() == 4 && colors[0] == 0 && colors[1] == 1 &&
                colors[2] == 2 && colors[3] == 0,
            "CSR deterministic column coloring is incorrect");
    const auto gmres = smave::restarted_gmres(
        system, std::vector<double>(4),
        smave::incomplete_lu_threshold_preconditioner(system, 1.0e-6, 4),
        1.0e-12, 1.0e-10, 20, 4);
    require(gmres.converged && gmres.iterations <= 4 &&
                gmres.residual_history.back() < 1.0e-9 &&
                std::abs(gmres.solution[0] - 1.0) < 1.0e-9 &&
                std::abs(gmres.solution[1] - 1.0) < 1.0e-9 &&
                std::abs(gmres.solution[2] - 1.0) < 1.0e-9 &&
                std::abs(gmres.solution[3] - 1.0) < 1.0e-9,
            "GMRES+ILUT failed the non-symmetric reference system");
    const auto operator_gmres = smave::restarted_gmres(
        system.size(),
        [&](const std::vector<double>& input, std::vector<double>& output) {
            output = system.multiply(input);
            return true;
        },
        system.right_hand_side, std::vector<double>(4),
        smave::incomplete_lu_threshold_preconditioner(system, 1.0e-6, 4),
        1.0e-12, 1.0e-10, 20, 4);
    require(operator_gmres.converged && operator_gmres.solution.size() == 4 &&
                std::abs(operator_gmres.solution[0] - gmres.solution[0]) < 1.0e-12 &&
                std::abs(operator_gmres.solution[3] - gmres.solution[3]) < 1.0e-12,
            "operator GMRES diverged from the matrix wrapper");
    smave::LinearSystem rank_deficient;
    rank_deficient.matrix = {
        {1.0, 0.0, 1.0},
        {0.0, 1.0, 1.0},
        {0.0, 0.0, 0.0},
    };
    rank_deficient.right_hand_side = {2.0, 2.0, 0.0};
    const auto lsqr = smave::least_squares_qr(
        rank_deficient, std::vector<double>(3), 1.0e-12, 1.0e-10, 20);
    const auto lsqr_product = rank_deficient.multiply(lsqr.solution);
    require(lsqr.converged && lsqr.solution.size() == 3 &&
                lsqr_product.size() == 3 &&
                std::abs(lsqr_product[0] - 2.0) < 1.0e-10 &&
                std::abs(lsqr_product[1] - 2.0) < 1.0e-10 &&
                std::abs(lsqr_product[2]) < 1.0e-10,
            "LSQR failed a consistent rank-deficient system");
    const auto pcg = smave::preconditioned_conjugate_gradient(
        system, std::vector<double>(4), smave::jacobi_preconditioner(system),
        1.0e-12, 1.0e-10, 20);
    require(pcg.breakdown && !pcg.converged,
            "non-symmetric system bypassed the PCG structural gate");

    const auto source = root / "NonsymmetricLinear.mo";
    std::ofstream(source)
        << "model NonsymmetricLinear\n"
        << "parameter Real b1=3;\nparameter Real b2=5;\n"
        << "parameter Real b3=5;\nparameter Real b4=6;\n"
        << "Real x1(start=0);\nReal x2(start=0);\n"
        << "Real x3(start=0);\nReal x4(start=0);\n"
        << "equation\n4*x1-x2=b1;\n2*x1+4*x2-x3=b2;\n"
        << "2*x2+4*x3-x4=b3;\n2*x3+4*x4=b4;\n"
        << "end NonsymmetricLinear;\n";
    const auto model = smave::compile_model(source);
    smave::RoutingConfig routing;
    routing.top_k = 4;
    auto gmres_routing = routing;
    gmres_routing.expert_allowlist.insert("gmres-ilut-cpu-v1");
    const auto outcome = smave::Runtime(
        model, smave::Tolerance{}, gmres_routing).solve(
            {}, root / "gmres-ilu-traces");
    require(outcome.success && !outcome.blocks.front().linear_spd &&
                outcome.blocks.front().path == smave::SolvePath::direct_accept &&
                outcome.blocks.front().preconditioner_version ==
                    "gmres-ilut-cpu-v1" &&
                std::find(
                    outcome.blocks.front().attempted_experts.begin(),
                    outcome.blocks.front().attempted_experts.end(),
                    "gmres-ilut-cpu-v1") !=
                    outcome.blocks.front().attempted_experts.end() &&
                outcome.blocks.front().krylov_iterations <= 4 &&
                outcome.blocks.front().gate.decision ==
                    smave::GateDecision::direct_accept,
            "runtime did not route the non-symmetric block through GMRES+ILUT");
    require(outcome.blocks.front().attempt_records.size() == 1 &&
                outcome.blocks.front().attempt_records.front().expert_version ==
                    "gmres-ilut-cpu-v1" &&
                outcome.blocks.front().attempt_records.front().outcome == "accepted" &&
                std::none_of(
                    outcome.blocks.front().attempted_experts.begin(),
                    outcome.blocks.front().attempted_experts.end(),
                    [](const std::string& version) { return version.starts_with("pcg-"); }),
            "runtime did not retain per-expert structure and acceptance reasons");

    auto singular = system;
    singular.matrix[0][0] = 0.0;
    singular.matrix[0][1] = 1.0;
    require(!smave::incomplete_lu_zero_preconditioner(singular, sparsity),
            "ILU(0) accepted a zero pivot without fallback");
    require(!smave::incomplete_lu_threshold_preconditioner(singular, 1.0e-4, 4),
            "ILUT accepted a zero pivot without fallback");
    singular.right_hand_side = {1.0, 5.0, 5.0, 6.0};
    const auto pivoted = smave::sparse_ordered_threshold_pivot_solve(singular);
    require(pivoted.solved && pivoted.solution.size() == 4 &&
                pivoted.column_order.size() == 4 &&
                pivoted.minimum_scaled_pivot > 0.0 &&
                std::all_of(
                    pivoted.solution.begin(), pivoted.solution.end(),
                    [](double value) { return std::abs(value - 1.0) < 1.0e-12; }),
            "ordered scaled-threshold sparse pivoting failed a nonsingular system");

    const auto pivot_source = root / "NonsymmetricPivot.mo";
    std::ofstream(pivot_source)
        << "model NonsymmetricPivot\n"
        << "parameter Real p=0;\n"
        << "Real x1(start=0); Real x2(start=0); Real x3(start=0); Real x4(start=0);\n"
        << "equation\np*x1+x2=1; 2*x1+x3=3; 3*x2+x4=4; 2*x3+x4=3;\n"
        << "end NonsymmetricPivot;\n";
    const auto pivot_model = smave::compile_model(pivot_source);
    const auto pivot_outcome = smave::Runtime(
        pivot_model, smave::Tolerance{}, routing).solve(
            {}, root / "sparse-pivot-traces");
    require(pivot_outcome.success && pivot_outcome.blocks.size() == 1,
            "zero-pivot runtime fixture did not produce one successful block");
    const auto& pivot_block = pivot_outcome.blocks.front();
    const auto ilut_attempt = std::find_if(
        pivot_block.attempt_records.begin(), pivot_block.attempt_records.end(),
        [](const smave::ExpertAttemptRecord& attempt) {
            return attempt.expert_version == "gmres-ilut-cpu-v1";
        });
    const auto sparse_attempt = std::find_if(
        pivot_block.attempt_records.begin(), pivot_block.attempt_records.end(),
        [](const smave::ExpertAttemptRecord& attempt) {
            return attempt.expert_version == "sparse-ordered-threshold-pivot-cpu-v2";
        });
    const auto industrial_attempt = std::find_if(
        pivot_block.attempt_records.begin(), pivot_block.attempt_records.end(),
        [](const smave::ExpertAttemptRecord& attempt) {
            return attempt.expert_version == "accelerate-sparse-qr-cpu-v1";
        });
    const auto ilu0_attempt = std::find_if(
        pivot_block.attempt_records.begin(), pivot_block.attempt_records.end(),
        [](const smave::ExpertAttemptRecord& attempt) {
            return attempt.expert_version == "gmres-ilu0-cpu-v1";
        });
    require(ilut_attempt != pivot_block.attempt_records.end() &&
                ilut_attempt->outcome == "rejected" &&
                ilut_attempt->reason == "invalid GMRES input" &&
                ((ilu0_attempt != pivot_block.attempt_records.end() &&
                  ilu0_attempt->outcome == "accepted") ||
                 (sparse_attempt != pivot_block.attempt_records.end() &&
                  sparse_attempt->outcome == "accepted") ||
                 (industrial_attempt != pivot_block.attempt_records.end() &&
                  industrial_attempt->outcome == "accepted")) &&
                std::abs(pivot_outcome.values.at("x1") - 1.0) < 1.0e-12 &&
                std::abs(pivot_outcome.values.at("x4") - 1.0) < 1.0e-12,
            "runtime did not recover through CSR ILU(0), industrial QR, or sparse partial pivoting");
}

void test_structural_row_alignment_for_spd(const std::filesystem::path& root) {
    const auto source = root / "Grid.mo";
    std::ofstream file(source);
    file << "model Grid\n";
    for (int index = 1; index <= 9; ++index) {
        file << "parameter Real b" << index << "=1; Real x" << index << ";\n";
    }
    file << "equation\n"
         << "4*x1-x2-x4=b1;\n"
         << "4*x2-x1-x3-x5=b2;\n"
         << "4*x3-x2-x6=b3;\n"
         << "4*x4-x1-x5-x7=b4;\n"
         << "4*x5-x2-x4-x6-x8=b5;\n"
         << "4*x6-x3-x5-x9=b6;\n"
         << "4*x7-x4-x8=b7;\n"
         << "4*x8-x5-x7-x9=b8;\n"
         << "4*x9-x6-x8=b9;\nend Grid;\n";
    file.close();
    const auto model = smave::compile_model(source);
    std::unordered_map<std::string, smave::Expression> residuals;
    for (const auto& equation : model.equations) {
        residuals.emplace(equation.id, smave::Expression(equation.residual));
    }
    const auto system = smave::assemble_linear_system(
        model, model.blocks.front(), residuals, {});
    require(system.symmetric && system.positive_definite,
            "matched equation rows are not aligned with block unknown ordering");
}

void test_tensor_bucket_and_local_fallback(const std::filesystem::path& root) {
    const auto source = root / "BatchSpd.mo";
    std::ofstream file(source);
    file << "model BatchSpd\n";
    for (int index = 1; index <= 8; ++index) {
        file << "parameter Real b" << index << "=1; Real x" << index << ";\n";
    }
    file << "equation\n4*x1-x2=b1;\n";
    for (int index = 2; index <= 7; ++index) {
        file << "-x" << index - 1 << "+4*x" << index << "-x" << index + 1
             << "=b" << index << ";\n";
    }
    file << "-x7+4*x8=b8;\nend BatchSpd;\n";
    file.close();
    const auto model = smave::compile_model(source);
    const auto training = root / "batch-training";
    std::filesystem::create_directories(training);
    for (int sample = 1; sample <= 3; ++sample) {
        std::ofstream scenario(training / ("sample-" + std::to_string(sample) + ".conf"));
        for (int index = 1; index <= 8; ++index) {
            scenario << "b" << index << '=' << sample + index * 0.01 << '\n';
        }
    }
    const auto artifact = smave::train_linear_preconditioner(
        model, "block-1", training);
    const smave::LearnedLinearPreconditionerExpert expert(artifact);
    std::vector<std::unordered_map<std::string, double>> scenarios(64);
    for (std::size_t item = 0; item < scenarios.size(); ++item) {
        for (int index = 1; index <= 8; ++index) {
            scenarios[item]["b" + std::to_string(index)] =
                1.0 + static_cast<double>(item % 3) + index * 0.01;
        }
    }
    const smave::Runtime fallback(model);
    const auto batch = smave::TensorBucketScheduler(32).solve_linear_batch(
        model,
        model.blocks.front(),
        expert,
        scenarios,
        fallback,
        root / "batch-traces");
    require(batch.metrics.requests == 64 && batch.metrics.batches == 2 &&
                batch.metrics.maximum_batch == 32 && batch.metrics.average_batch == 32.0,
            "tensor bucket metrics are incorrect");
    require(batch.metrics.accepted == 64 && batch.metrics.fallback_count == 0,
            "valid tensor batch did not pass per-instance gates");
    require(batch.metrics.sequential_baseline_us > 0.0 &&
                batch.metrics.throughput_speedup > 0.0 &&
                batch.metrics.baseline_failures == 0,
            "tensor batch lacks a valid sequential baseline comparison");
    require(std::filesystem::exists(root / "batch-traces" / "tensor-batch.trace"),
            "tensor batch audit trace was not persisted");
    require(std::all_of(
                batch.outcomes.begin(), batch.outcomes.end(),
                [](const smave::SolveOutcome& outcome) {
                    return outcome.success && !outcome.blocks.empty() &&
                        outcome.blocks.front().gate.decision == smave::GateDecision::direct_accept;
                }),
            "tensor batch committed an unverified result");

    auto ood_scenarios = scenarios;
    ood_scenarios.front()["b1"] = 100.0;
    const auto local_fallback = smave::TensorBucketScheduler(32).solve_linear_batch(
        model,
        model.blocks.front(),
        expert,
        ood_scenarios,
        fallback,
        root / "batch-ood-traces");
    require(local_fallback.metrics.fallback_count >= 1 &&
                local_fallback.outcomes.front().success,
            "OOD batch item did not locally fallback to the original portfolio");
}

void test_cegis_verified_cells_and_certificate_binding(const std::filesystem::path& root) {
    const auto certificate = smave::verify_cells(
        "expert-v1",
        "artifact-hash",
        "block-fingerprint",
        {"p"},
        {0.0},
        {1.0},
        [](const std::unordered_map<std::string, double>& context) {
            const double value = context.at("p");
            const bool accepted = value < 0.45 || value > 0.55;
            return smave::ProbeResult{
                .accepted = accepted,
                .residual = accepted ? 0.1 : 10.0,
                .risk = accepted ? 0.01 : 1.0,
                .reason = accepted ? "safe" : "synthetic singular band",
            };
        },
        5,
        1.0e-3);
    require(!certificate.cells.empty() && !certificate.counterexamples.empty(),
            "CEGIS did not retain safe cells and counterexamples");
    for (std::size_t left = 0; left < certificate.counterexamples.size(); ++left) {
        for (std::size_t right = left + 1; right < certificate.counterexamples.size(); ++right) {
            require(certificate.counterexamples[left].context !=
                        certificate.counterexamples[right].context,
                    "CEGIS retained duplicate counterexample contexts");
        }
    }
    require(certificate.contains({{"p", 0.1}}), "verified safe context was rejected");
    require(!certificate.contains({{"p", 0.5}}), "counterexample band was certified");
    const auto path = root / "cells.verify";
    certificate.write(path);
    const auto restored = smave::VerificationCertificate::read(path);
    require(restored.certificate_hash == certificate.certificate_hash,
            "verification certificate roundtrip lost integrity");
    const auto exported = root / "exported-counterexamples";
    restored.export_counterexamples(exported);
    require(std::distance(
                std::filesystem::directory_iterator(exported),
                std::filesystem::directory_iterator{}) ==
                static_cast<std::ptrdiff_t>(restored.counterexamples.size()),
            "verification counterexamples were not exported as training scenarios");
    auto tampered = certificate;
    tampered.cells.front().upper.front() += 1.0;
    bool tamper_rejected = false;
    try { tampered.validate(); } catch (const std::invalid_argument&) { tamper_rejected = true; }
    require(tamper_rejected, "tampered verification certificate was accepted");

    const auto source = root / "CertificateModel.mo";
    std::ofstream(source)
        << "model CertificateModel\nparameter Real p=2; Real x(start=1);\n"
        << "equation\nx*x=p;\nend CertificateModel;\n";
    const auto model = smave::compile_model(source);
    const auto training = root / "certificate-training";
    std::filesystem::create_directories(training);
    std::ofstream(training / "one.conf") << "p=1\n";
    std::ofstream(training / "two.conf") << "p=2\n";
    std::ofstream(training / "three.conf") << "p=3\n";
    const auto artifact = smave::train_affine_warm_start(
        model, "block-1", training, root / "certificate-labels");
    const auto valid_certificate = smave::verify_affine_warm_start(model, artifact, 2);
    auto registry = smave::make_default_registry(model);
    smave::register_affine_expert(
        registry,
        artifact,
        "domain-v1",
        "default",
        "cpu",
        valid_certificate);
    auto bundle = smave::make_default_bundle(model);
    bundle.add_expert(
        artifact.expert_version,
        artifact.artifact_hash,
        valid_certificate.certificate_hash);
    registry.validate_bundle(bundle, model);

    auto wrong_certificate = valid_certificate;
    wrong_certificate.artifact_hash = "wrong-artifact";
    wrong_certificate.seal();
    bool mismatch_rejected = false;
    try {
        smave::register_affine_expert(
            registry,
            artifact,
            "domain-v1",
            "default",
            "cpu",
            wrong_certificate);
    } catch (const std::invalid_argument&) {
        mismatch_rejected = true;
    }
    require(mismatch_rejected, "expert accepted a certificate for another artifact");
}

void test_equation_embedding_family_retrieval(const std::filesystem::path& root) {
    const auto source_a = root / "FamilyA.mo";
    std::ofstream(source_a)
        << "model FamilyA\nparameter Real p=1; Real x; Real y;\n"
        << "equation\n4*x-y=p;\n-x+4*y=p;\nend FamilyA;\n";
    const auto source_b = root / "FamilyB.mo";
    std::ofstream(source_b)
        << "model FamilyB\nparameter Real q=2; Real u; Real v;\n"
        << "equation\n4*u-v=q;\n-u+4*v=q;\nend FamilyB;\n";
    const auto source_c = root / "FamilyC.mo";
    std::ofstream(source_c)
        << "model FamilyC\nparameter Real q=2; Real u(start=1); Real v(start=1);\n"
        << "equation\nu*u+sin(v)=q;\nu+v*v=q;\nend FamilyC;\n";
    const auto model_a = smave::compile_model(source_a);
    const auto model_b = smave::compile_model(source_b);
    const auto model_c = smave::compile_model(source_c);
    const auto embedding_a = smave::encode_block(model_a, model_a.blocks.front());
    const auto embedding_b = smave::encode_block(model_b, model_b.blocks.front());
    const auto embedding_c = smave::encode_block(model_c, model_c.blocks.front());
    require(model_a.blocks.front().fingerprint != model_b.blocks.front().fingerprint,
            "exact fingerprint unexpectedly ignored variable/context identity");
    require(smave::embedding_similarity(embedding_a, embedding_b) > 0.95,
            "isomorphic unseen family was not retrieved by structural embedding");
    require(smave::embedding_similarity(embedding_a, embedding_c) < 0.90,
            "heterogeneous nonlinear family was too similar to linear family");

    smave::FamilyIndex index;
    index.add(smave::FamilyEntry{
        .family_id = "two-by-two-spd",
        .expert_version = "family-expert-v1",
        .artifact_hash = "family-artifact",
        .embedding = embedding_a,
    });
    const auto matches = index.query(embedding_b, 4, 0.8);
    require(matches.size() == 1 && matches.front().similarity > 0.95,
            "family index failed to return isomorphic unseen family");
    const auto path = root / "families.index";
    index.write(path);
    const auto restored = smave::FamilyIndex::read(path).query(embedding_b, 4, 0.8);
    require(restored.size() == 1 && restored.front().family_id == "two-by-two-spd",
            "family index roundtrip failed");

    const auto registry = smave::make_default_registry(model_b);
    const auto bundle = smave::make_default_bundle(model_b);
    const auto candidates = smave::CompileRouter{}.lookup(
        model_b.blocks.front(), registry, bundle);
    require(std::none_of(
                candidates.begin(), candidates.end(),
                [](const smave::CandidateExpert& candidate) {
                    return candidate.expert_version == "family-expert-v1";
                }),
            "embedding retrieval silently granted runtime permission");
}

void test_expert_competition_and_calibration(const std::filesystem::path& root) {
    smave::PerformanceReport performance;
    performance.schema_version = 2;
    performance.dataset_id = "benchmark-suite";
    performance.dataset_version = std::string(64, '1');
    performance.dataset_manifest_hash = std::string(64, '2');
    performance.scenarios = 2;
    performance.repetitions = 3;
    performance.samples = 6;
    performance.baseline_wall_us = {.mean = 11.0, .median = 10.0, .p90 = 12.0, .p99 = 13.0};
    performance.accelerated_wall_us = {.mean = 6.0, .median = 5.0, .p90 = 7.0, .p99 = 8.0};
    performance.baseline_iterations = {.mean = 4.0, .median = 4.0};
    performance.accelerated_iterations = {.mean = 2.0, .median = 2.0};
    performance.median_speedup = 2.0;
    performance.p99_speedup = 1.625;
    performance.paired_median_speedup = 2.0;
    performance.paired_p01_speedup = 1.5;
    performance.paired_win_rate = 1.0;
    performance.paired_speedup_ci95_lower = 1.8;
    performance.paired_speedup_ci95_upper = 2.2;
    performance.bootstrap_samples = 2000;
    performance.same_accuracy = true;
    performance.p99_not_regressed = true;
    performance.seal();
    const auto performance_path = root / "performance-lineage.txt";
    smave::write_performance_report(performance, performance_path);
    const auto restored_performance = smave::read_performance_report(performance_path);
    require(restored_performance.schema_version == 2 &&
                restored_performance.dataset_id == "benchmark-suite" &&
                restored_performance.report_hash == performance.report_hash,
            "performance v2 dataset lineage roundtrip failed");
    auto tampered_performance = restored_performance;
    tampered_performance.dataset_version = std::string(64, '3');
    bool performance_tamper_rejected = false;
    try { tampered_performance.validate(); }
    catch (const std::invalid_argument&) { performance_tamper_rejected = true; }
    require(performance_tamper_rejected,
            "performance report accepted tampered dataset lineage");

    const auto source = root / "Competition.mo";
    std::ofstream(source)
        << "model Competition\nparameter Real p=2; Real x(start=1);\n"
        << "equation\nx*x=p;\nend Competition;\n";
    const auto model = smave::compile_model(source);
    const auto training = root / "competition-training";
    const auto scenarios = root / "competition-scenarios";
    std::filesystem::create_directories(training);
    std::filesystem::create_directories(scenarios);
    std::ofstream(training / "one.conf") << "p=1\n";
    std::ofstream(training / "two.conf") << "p=2\n";
    std::ofstream(training / "three.conf") << "p=3\n";
    std::ofstream(scenarios / "one.conf") << "p=1.25\n";
    std::ofstream(scenarios / "two.conf") << "p=2.25\n";
    const auto artifact = smave::train_affine_warm_start(
        model, "block-1", training, root / "competition-labels");
    const auto certificate = smave::verify_affine_warm_start(model, artifact, 2);
    auto registry = smave::make_default_registry(model);
    smave::register_affine_expert(
        registry, artifact, "domain-v1", "default", "cpu", certificate);
    auto bundle = smave::make_default_bundle(model);
    bundle.add_expert(
        artifact.expert_version,
        artifact.artifact_hash,
        certificate.certificate_hash);
    const auto report = smave::compete_experts(
        model, registry, bundle, scenarios, root / "competition-traces");
    require(std::any_of(
                report.entries.begin(), report.entries.end(),
                [&](const smave::CompetitionEntry& entry) {
                    return entry.expert_version == artifact.expert_version;
                }) &&
                std::any_of(
                    report.entries.begin(), report.entries.end(),
                    [&](const smave::CompetitionEntry& entry) {
                        return entry.expert_version == bundle.terminal_fallback;
                    }),
            "competition omitted learned expert or terminal baseline");
    require(!report.winner.empty(), "competition did not select a safe winner");
    for (const auto& entry : report.entries) {
        require(entry.attempts == 2 && entry.failures == 0,
                "competition did not execute every scenario safely");
        require(entry.predicted_pass_rate >= 0.0 && entry.predicted_pass_rate <= 1.0 &&
                    entry.calibration_error >= 0.0 && entry.calibration_error <= 1.0,
                "competition calibration metric is outside probability bounds");
    }
    const auto winner = std::find_if(
        report.entries.begin(), report.entries.end(),
        [&](const smave::CompetitionEntry& entry) {
            return entry.expert_version == report.winner;
        });
    require(winner != report.entries.end() && winner->passes == winner->attempts,
            "competition selected a candidate that did not pass every runtime gate");
    const auto fastest_safe = std::min_element(
        report.entries.begin(), report.entries.end(),
        [](const smave::CompetitionEntry& left, const smave::CompetitionEntry& right) {
            return left.median_wall_us < right.median_wall_us;
        });
    require(winner->median_wall_us <= fastest_safe->median_wall_us + std::max(
                fastest_safe->median_wall_us * 0.02, 100.0),
            "competition winner is outside the deterministic practical equivalence band");

    const auto report_path = root / "competition.txt";
    smave::write_competition_report(report, report_path);
    const auto restored = smave::read_competition_report(report_path);
    require(restored.winner == report.winner &&
                restored.block_fingerprint == report.block_fingerprint &&
                restored.entries.size() == report.entries.size(),
            "competition report roundtrip failed");
    auto lineage_report = report;
    lineage_report.schema_version = 4;
    lineage_report.dataset_id = "competition-suite";
    lineage_report.dataset_version = std::string(64, 'a');
    lineage_report.dataset_manifest_hash = std::string(64, 'b');
    lineage_report.seal();
    const auto lineage_report_path = root / "competition-lineage.txt";
    smave::write_competition_report(lineage_report, lineage_report_path);
    const auto restored_lineage = smave::read_competition_report(lineage_report_path);
    require(restored_lineage.schema_version == 4 &&
                restored_lineage.dataset_id == "competition-suite" &&
                restored_lineage.report_hash == lineage_report.report_hash,
            "competition v4 dataset lineage roundtrip failed");
    auto incomplete_lineage = lineage_report;
    incomplete_lineage.dataset_version.clear();
    incomplete_lineage.seal();
    bool incomplete_lineage_rejected = false;
    try { incomplete_lineage.validate(); }
    catch (const std::invalid_argument&) { incomplete_lineage_rejected = true; }
    require(incomplete_lineage_rejected,
            "competition v4 accepted incomplete dataset lineage");
    smave::RoutingConfig calibrated_routing;
    smave::apply_competition_profile(restored, calibrated_routing);
    const auto calibrated = smave::RuntimeRouter(calibrated_routing).route(
        model.blocks.front(), {}, smave::CompileRouter{}.lookup(
            model.blocks.front(), registry, bundle), registry, bundle);
    if (restored.winner == bundle.terminal_fallback) {
        require(calibrated.steps.empty(),
                "calibrated Router ignored a terminal-fallback competition winner");
    } else {
        const auto winner_entry = std::find_if(
            restored.entries.begin(), restored.entries.end(),
            [&](const smave::CompetitionEntry& entry) {
                return entry.expert_version == restored.winner;
            });
        require(!calibrated.steps.empty() &&
                    calibrated.steps.front().expert_version == restored.winner,
                "calibrated Router did not prioritize the safe full-cost winner");
        require(winner_entry != restored.entries.end() &&
                    calibrated.steps.front().budget.work_iterations ==
                        static_cast<int>(std::ceil(winner_entry->median_iterations)),
                "calibrated Router did not propagate the observed correction budget");
        require(calibrated.steps.front().selection_reason.find(
                    "calibrated correction budget=") != std::string::npos,
                "calibrated Router did not expose its correction-budget decision");
    }
    auto unsafe_profile = restored;
    unsafe_profile.winner = artifact.expert_version;
    auto unsafe_winner = std::find_if(
        unsafe_profile.entries.begin(), unsafe_profile.entries.end(),
        [&](const smave::CompetitionEntry& entry) {
            return entry.expert_version == unsafe_profile.winner;
        });
    unsafe_winner->fallbacks = 1;
    unsafe_winner->passes = unsafe_winner->attempts - 1;
    unsafe_profile.seal();
    bool unsafe_rejected = false;
    try {
        smave::apply_competition_profile(unsafe_profile, calibrated_routing);
    } catch (const std::invalid_argument&) {
        unsafe_rejected = true;
    }
    require(unsafe_rejected, "Router accepted an unverified competition winner");
    auto tampered_profile = restored;
    tampered_profile.entries.front().median_wall_us = 0.0;
    bool tampered_rejected = false;
    try {
        smave::apply_competition_profile(tampered_profile, calibrated_routing);
    } catch (const std::invalid_argument&) {
        tampered_rejected = true;
    }
    require(tampered_rejected, "Router accepted a tampered competition profile");

    smave::RoutingConfig routing;
    routing.top_k = 1;
    routing.expert_allowlist.insert("not-a-real-expert");
    const auto fallback = smave::Runtime(
        model, registry, bundle, {}, routing).solve(
            {{"p", 2.0}}, root / "competition-fallback-traces");
    require(fallback.success && fallback.blocks.front().path == smave::SolvePath::full_fallback,
            "expert allowlist removed the mandatory terminal fallback");
}

void test_heldout_family_router_evidence(const std::filesystem::path& root) {
    const auto source_path = root / "RouterSource.mo";
    std::ofstream(source_path)
        << "model RouterSource\n"
        << "parameter Real b1=1; parameter Real b2=1; parameter Real b3=1; parameter Real b4=1;\n"
        << "Real x1; Real x2; Real x3; Real x4;\n"
        << "equation\n4*x1-x2=b1;\n-x1+4*x2-x3=b2;\n"
        << "-x2+4*x3-x4=b3;\n-x3+4*x4=b4;\nend RouterSource;\n";
    const auto heldout_path = root / "RouterHeldout.mo";
    std::ofstream(heldout_path)
        << "model RouterHeldout\n"
        << "parameter Real q1=1; parameter Real q2=1; parameter Real q3=1; parameter Real q4=1;\n"
        << "Real u1; Real u2; Real u3; Real u4;\n"
        << "equation\n4*u1-u2=q1;\n-u1+4*u2-u3=q2;\n"
        << "-u2+4*u3-u4=q3;\n-u3+4*u4=q4;\nend RouterHeldout;\n";
    const auto source = smave::compile_model(source_path);
    const auto heldout = smave::compile_model(heldout_path);
    smave::CompetitionReport source_profile;
    source_profile.block_fingerprint = source.blocks.front().fingerprint;
    source_profile.winner = "dense-direct-cpu-v1";
    source_profile.entries.push_back(smave::CompetitionEntry{
        .expert_version = "dense-direct-cpu-v1",
        .attempts = 20,
        .passes = 20,
        .empirical_pass_rate = 1.0,
        .predicted_pass_rate = 0.999,
        .calibration_error = 0.001,
        .median_wall_us = 10.0,
    });
    source_profile.seal();
    const auto scenarios = root / "router-heldout-scenarios";
    std::filesystem::create_directories(scenarios);
    std::ofstream(scenarios / "one.conf")
        << "q1=1.1\nq2=1.2\nq3=1.3\nq4=1.4\n";
    const auto registry = smave::make_default_registry(heldout);
    const auto bundle = smave::make_default_bundle(heldout);
    const auto evaluation = smave::evaluate_family_router(
        source,
        source_profile,
        heldout,
        registry,
        bundle,
        scenarios,
        root / "router-heldout-traces",
        0.95,
        1.000001,
        1);
    evaluation.validate();
    require(evaluation.distinct_instance && evaluation.embedding_similarity > 0.99,
            "family Router evaluation did not use a distinct isomorphic instance");
    require(evaluation.calibrated_expert == "dense-direct-cpu-v1" &&
                evaluation.source_competition_hash == source_profile.report_hash,
            "family Router evidence did not bind the transferable builtin policy");
    require(evaluation.safe && evaluation.calibrated_dangerous_misroutes == 0,
            "held-out family policy introduced a dangerous misroute");

    auto source_lineage_profile = source_profile;
    source_lineage_profile.schema_version = 4;
    source_lineage_profile.dataset_id = "source-competition";
    source_lineage_profile.dataset_version = std::string(64, 'c');
    source_lineage_profile.dataset_manifest_hash = std::string(64, 'd');
    source_lineage_profile.seal();
    smave::DatasetManifest heldout_dataset;
    heldout_dataset.dataset_id = "heldout-family";
    heldout_dataset.version = std::string(64, 'e');
    heldout_dataset.manifest_hash = std::string(64, 'f');
    const auto lineage_evaluation = smave::evaluate_family_router(
        source,
        source_lineage_profile,
        heldout,
        registry,
        bundle,
        scenarios,
        root / "router-heldout-lineage-traces",
        0.95,
        1.000001,
        1,
        {},
        heldout_dataset);
    require(lineage_evaluation.schema_version == 3 &&
                lineage_evaluation.source_dataset_id == "source-competition" &&
                lineage_evaluation.heldout_dataset_id == "heldout-family",
            "family Router v3 did not bind source and heldout lineage");
    auto mismatched_mode_profile = source_profile;
    bool snapshot_source_rejected = false;
    try {
        (void)smave::evaluate_family_router(
            source, mismatched_mode_profile, heldout, registry, bundle, scenarios,
            root / "router-heldout-mismatch-traces", 0.95, 1.000001, 1, {},
            heldout_dataset);
    } catch (const std::invalid_argument&) {
        snapshot_source_rejected = true;
    }
    require(snapshot_source_rejected,
            "snapshot family evaluation accepted a lineage-free source profile");

    auto learned_profile = source_profile;
    learned_profile.winner = "source-only-learned-expert";
    learned_profile.entries.front().expert_version = learned_profile.winner;
    learned_profile.seal();
    const auto conservative = smave::evaluate_family_router(
        source,
        learned_profile,
        heldout,
        registry,
        bundle,
        scenarios,
        root / "router-heldout-conservative-traces",
        0.95,
        1.000001,
        1);
    require(conservative.calibrated_expert == conservative.fixed_expert,
            "family retrieval transferred a source-only learned artifact");
    auto release_evidence = evaluation;
    release_evidence.fixed_median_wall_us = 11.0;
    release_evidence.calibrated_median_wall_us = 10.0;
    release_evidence.calibrated_speedup = 1.1;
    release_evidence.fixed_p99_wall_us = 12.0;
    release_evidence.calibrated_p99_wall_us = 11.0;
    release_evidence.paired_median_speedup = 1.1;
    release_evidence.paired_win_rate = 1.0;
    release_evidence.paired_speedup_ci95_lower = 1.05;
    release_evidence.paired_speedup_ci95_upper = 1.15;
    release_evidence.bootstrap_samples = 2000;
    release_evidence.maximum_mixed_qoi_error = 0.0;
    release_evidence.fixed_failures = 0;
    release_evidence.calibrated_failures = 0;
    release_evidence.gate_mismatches = 0;
    release_evidence.p99_not_regressed = true;
    release_evidence.same_accuracy = true;
    release_evidence.improved = true;
    release_evidence.safe = true;
    release_evidence.seal();
    const auto report_path = root / "family-router-evaluation.txt";
    smave::write_family_router_evaluation(release_evidence, report_path);
    const auto restored = smave::read_family_router_evaluation(report_path);
    require(restored.report_hash == release_evidence.report_hash,
            "family Router evidence report roundtrip failed");
    smave::RoutingConfig family_routing;
    smave::apply_family_router_evaluation(restored, family_routing);
    const auto plan = smave::RuntimeRouter(family_routing).route(
        heldout.blocks.front(), {}, smave::CompileRouter{}.lookup(
            heldout.blocks.front(), registry, bundle), registry, bundle);
    require(!plan.steps.empty() &&
                plan.steps.front().expert_version == release_evidence.calibrated_expert,
            "Runtime did not consume the held-out family Router evidence");

    auto tampered = evaluation;
    tampered.calibrated_speedup += 1.0;
    bool rejected = false;
    try { tampered.validate(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "tampered family Router evidence was accepted");
}

void test_latent_operator_permissions_and_gate(const std::filesystem::path& root) {
    const auto source = root / "OperatorModel.mo";
    std::ofstream(source)
        << "model OperatorModel\n"
        << "parameter Real b1=1; parameter Real b2=1; parameter Real b3=1; parameter Real b4=1;\n"
        << "Real x1; Real x2; Real x3; Real x4;\n"
        << "equation\n4*x1-x2=b1;\n-x1+4*x2-x3=b2;\n"
        << "-x2+4*x3-x4=b3;\n-x3+4*x4=b4;\nend OperatorModel;\n";
    const auto model = smave::compile_model(source);
    const auto training = root / "operator-training";
    std::filesystem::create_directories(training);
    for (int sample = 0; sample < 8; ++sample) {
        std::ofstream scenario(training / ("sample-" + std::to_string(sample) + ".conf"));
        for (int index = 1; index <= 4; ++index) {
            scenario << "b" << index << '=' << 1.0 + sample * 0.2 + index * 0.01 << '\n';
        }
    }
    const auto artifact = smave::train_latent_operator(
        model,
        "block-1",
        training,
        root / "operator-label-traces",
        3,
        {"x1", "x4"});
    require(artifact.training_samples == 8 && artifact.outputs.size() == 4 &&
                artifact.qoi_outputs == std::vector<std::string>({"x1", "x4"}) &&
                artifact.output_permission == "full-state-corrected",
            "latent operator did not preserve full-state and QoI contracts");
    const auto artifact_path = root / "operator.expert";
    artifact.write(artifact_path);
    const auto restored = smave::LatentOperatorArtifact::read(artifact_path);
    require(restored.artifact_hash == artifact.artifact_hash,
            "latent operator artifact roundtrip failed");

    auto lineage_artifact = artifact;
    lineage_artifact.schema_version = "smave.latent-operator.v2";
    lineage_artifact.training_dataset_id = "operator-training";
    lineage_artifact.training_dataset_version = std::string(64, 'a');
    lineage_artifact.training_dataset_manifest_hash = std::string(64, 'b');
    lineage_artifact.expert_version.clear();
    lineage_artifact.seal();
    const auto lineage_artifact_path = root / "operator-lineage.expert";
    lineage_artifact.write(lineage_artifact_path);
    const auto restored_lineage_artifact =
        smave::LatentOperatorArtifact::read(lineage_artifact_path);
    require(restored_lineage_artifact.schema_version == "smave.latent-operator.v2" &&
                restored_lineage_artifact.training_dataset_id == "operator-training" &&
                restored_lineage_artifact.artifact_hash == lineage_artifact.artifact_hash,
            "latent operator v2 training lineage roundtrip failed");

    auto lineage_certificate = smave::verify_cells(
        lineage_artifact.expert_version,
        lineage_artifact.artifact_hash,
        lineage_artifact.block_fingerprint,
        lineage_artifact.features,
        lineage_artifact.feature_minimum,
        lineage_artifact.feature_maximum,
        [](const std::unordered_map<std::string, double>&) {
            return smave::ProbeResult{.accepted = true};
        },
        0);
    lineage_certificate.schema_version = "smave.verified-cells.v2";
    lineage_certificate.training_dataset_id = lineage_artifact.training_dataset_id;
    lineage_certificate.training_dataset_version = lineage_artifact.training_dataset_version;
    lineage_certificate.training_dataset_manifest_hash =
        lineage_artifact.training_dataset_manifest_hash;
    lineage_certificate.seal();
    const auto lineage_certificate_path = root / "operator-lineage.verify";
    lineage_certificate.write(lineage_certificate_path);
    const auto restored_lineage_certificate =
        smave::VerificationCertificate::read(lineage_certificate_path);
    require(restored_lineage_certificate.schema_version == "smave.verified-cells.v2" &&
                restored_lineage_certificate.training_dataset_version ==
                    lineage_artifact.training_dataset_version,
            "verification certificate v2 training lineage roundtrip failed");
    smave::LatentOperatorExpert lineage_expert(
        lineage_artifact, restored_lineage_certificate);

    auto mismatched_certificate = restored_lineage_certificate;
    mismatched_certificate.training_dataset_version = std::string(64, 'c');
    mismatched_certificate.seal();
    bool mismatched_lineage_rejected = false;
    try {
        smave::LatentOperatorExpert mismatched(lineage_artifact, mismatched_certificate);
    } catch (const std::invalid_argument&) {
        mismatched_lineage_rejected = true;
    }
    require(mismatched_lineage_rejected,
            "operator accepted a certificate for a different training snapshot");

    auto legacy_certificate = restored_lineage_certificate;
    legacy_certificate.schema_version = "smave.verified-cells.v1";
    legacy_certificate.training_dataset_id.clear();
    legacy_certificate.training_dataset_version.clear();
    legacy_certificate.training_dataset_manifest_hash.clear();
    legacy_certificate.seal();
    bool legacy_certificate_rejected = false;
    try {
        smave::LatentOperatorExpert mismatched(lineage_artifact, legacy_certificate);
    } catch (const std::invalid_argument&) {
        legacy_certificate_rejected = true;
    }
    require(legacy_certificate_rejected,
            "operator v2 accepted a lineage-free verification certificate");

    auto registry = smave::make_default_registry(model);
    smave::register_latent_operator(registry, artifact);
    require(registry.grant(artifact.expert_version).permission == smave::Permission::corrected &&
                registry.grant(artifact.expert_version).evidence_level ==
                    smave::EvidenceLevel::e2,
            "latent operator received unsafe Direct permission");
    auto bundle = smave::make_default_bundle(model);
    bundle.add_expert(
        artifact.expert_version,
        artifact.artifact_hash,
        registry.grant(artifact.expert_version).evidence_bundle);
    smave::RoutingConfig routing;
    routing.top_k = 1;
    routing.expert_allowlist.insert(artifact.expert_version);
    const std::unordered_map<std::string, double> scenario{
        {"b1", 1.31}, {"b2", 1.32}, {"b3", 1.33}, {"b4", 1.34}};
    const auto outcome = smave::Runtime(
        model, registry, bundle, {}, routing).solve(
            scenario, root / "operator-runtime-traces");
    require(outcome.success && outcome.blocks.front().path == smave::SolvePath::corrected_accept &&
                outcome.blocks.front().attempted_experts.front() == artifact.expert_version &&
                outcome.blocks.front().gate.decision == smave::GateDecision::direct_accept,
            "latent operator candidate bypassed correction/gate or was not used");

    auto zero_budget_routing = routing;
    zero_budget_routing.calibration_block_fingerprint = model.blocks.front().fingerprint;
    zero_budget_routing.calibration_winner = artifact.expert_version;
    zero_budget_routing.calibrations.emplace(
        artifact.expert_version,
        smave::RouteCalibration{
            .attempts = 1,
            .passes = 1,
            .pass_probability = 1.0,
            .median_wall_us = 1.0,
            .work_iterations = 0,
        });
    const auto zero_budget_outcome = smave::Runtime(
        model, registry, bundle, {}, zero_budget_routing).solve(
            scenario, root / "operator-zero-budget-traces");
    require(zero_budget_outcome.success &&
                zero_budget_outcome.blocks.front().path ==
                    smave::SolvePath::corrected_accept &&
                zero_budget_outcome.blocks.front().expert_iterations == 0 &&
                zero_budget_outcome.blocks.front().gate.decision ==
                    smave::GateDecision::direct_accept,
            "zero correction budget skipped raw-candidate residual acceptance");

    const smave::LatentOperatorExpert expert(artifact);
    smave::BlockContext ood;
    ood.values = {{"b1", 100.0}, {"b2", 100.0}, {"b3", 100.0}, {"b4", 100.0}};
    require(expert.solve(model.blocks.front(), ood, {}).status == "ood",
            "latent operator accepted an OOD context");
    smave::BlockContext in_domain;
    in_domain.values = scenario;
    const auto batch = expert.solve_batch(model.blocks.front(), {in_domain, ood});
    require(batch.size() == 2 && batch.front().status == "candidate" &&
                batch.front().candidate.size() == model.blocks.front().unknowns.size() &&
                batch.back().status == "ood" && batch.back().candidate.empty(),
            "operator batch did not isolate an OOD item");
    smave::DeviceExecutionResult metal_execution;
    const auto metal_batch = expert.solve_batch_on_device(
        "metal-gpu", model.blocks.front(), {in_domain, ood},
        &metal_execution);
    require(expert.in_domain_batch_size({in_domain, ood}) == 1,
            "latent operator residency batch shape did not exclude OOD inputs");
    if (!metal_execution.available) {
        require(!metal_execution.executed && !metal_execution.verified &&
                    !metal_execution.reason.empty() && metal_batch.size() == 2 &&
                    metal_batch.front().status == "failure" &&
                    metal_batch.front().candidate.empty() &&
                    metal_batch.back().status == "ood" &&
                    metal_batch.back().candidate.empty(),
                "unavailable Metal latent operator batch did not fail closed");
    } else {
        require(metal_execution.executed && metal_execution.verified &&
                    metal_batch.size() == 2 &&
                    metal_batch.front().status == "candidate" &&
                    metal_batch.back().status == "ood" &&
                    metal_batch.back().candidate.empty(),
                "latent operator did not isolate OOD input in a verified Metal batch");
        for (const auto& output : artifact.outputs) {
            require(std::abs(
                        metal_batch.front().candidate.at(output) -
                        batch.front().candidate.at(output)) <= 2.0e-5,
                    "Metal latent operator diverged from the CPU folded affine model");
        }
    }
    if (smave::coreml_neural_engine_available()) {
        smave::DeviceExecutionResult neural_execution;
        const auto neural_batch = expert.solve_batch_on_device(
            "coreml-neural-engine", model.blocks.front(),
            {in_domain, in_domain}, &neural_execution);
        const auto neural_resident = expert.operator_batch_is_resident(
            "coreml-neural-engine", 2);
        require(neural_execution.executed && neural_execution.verified &&
                    neural_batch.size() == 2 &&
                    neural_batch.front().status == "candidate" &&
                    neural_resident,
                "latent operator ANE batch failed: executed=" +
                    std::to_string(neural_execution.executed) +
                    " verified=" + std::to_string(neural_execution.verified) +
                    " resident=" + std::to_string(neural_resident) +
                    " backend=" + neural_execution.backend +
                    " max_abs=" +
                    std::to_string(neural_execution.maximum_absolute_error) +
                    " max_rel=" +
                    std::to_string(neural_execution.maximum_relative_error) +
                    " reason=" + neural_execution.reason);
        smave::DeviceExecutionResult neural_warm_execution;
        const auto neural_warm_batch = expert.solve_batch_on_device(
            "coreml-neural-engine", model.blocks.front(),
            {in_domain, in_domain}, &neural_warm_execution);
        require(neural_warm_execution.executed && neural_warm_execution.verified &&
                    neural_warm_execution.upload_us <= neural_execution.upload_us &&
                    neural_warm_batch.front().status == "candidate",
                "latent operator did not reuse the resident ANE model");
        for (const auto& output : artifact.outputs) {
            require(std::abs(
                        neural_warm_batch.front().candidate.at(output) -
                        batch.front().candidate.at(output)) <= 2.5e-4,
                    "ANE latent operator diverged from the CPU folded affine model");
        }
    }
    auto invalid_candidate = batch.front().candidate;
    invalid_candidate["extra"] = 1.0;
    const auto local_fallback = smave::Runtime(model).correct_candidate(
        scenario,
        "block-1",
        invalid_candidate,
        artifact.expert_version,
        root / "operator-invalid-candidate-traces");
    require(local_fallback.success &&
                local_fallback.blocks.front().path == smave::SolvePath::direct_accept,
            "invalid batch candidate did not locally fallback to classic Runtime");
    bool unknown_context_rejected = false;
    try {
        (void)smave::Runtime(model).correct_candidate(
            {{"unknown", 1.0}},
            "block-1",
            batch.front().candidate,
            artifact.expert_version,
            root / "operator-unknown-context-traces");
    } catch (const std::invalid_argument&) {
        unknown_context_rejected = true;
    }
    require(unknown_context_rejected,
            "batch correction accepted an unknown context field");

    std::unordered_map<std::string, double> corrected_candidate;
    for (const auto& unknown : model.blocks.front().unknowns) {
        corrected_candidate.emplace(unknown, outcome.values.at(unknown));
    }
    const smave::Runtime commit_runtime(model);
    const auto committed = commit_runtime.commit_corrected_candidate(
        scenario,
        "block-1",
        corrected_candidate,
        "test-corrected-candidate",
        root / "operator-corrected-commit-traces");
    require(committed.success && !committed.blocks.empty() &&
                committed.blocks.front().path == smave::SolvePath::corrected_accept &&
                committed.blocks.front().gate.decision ==
                    smave::GateDecision::direct_accept,
            "corrected candidate did not pass independent commit gate");
    auto invalid_corrected_candidate = corrected_candidate;
    invalid_corrected_candidate["extra"] = 1.0;
    const auto committed_fallback = commit_runtime.commit_corrected_candidate(
        scenario,
        "block-1",
        invalid_corrected_candidate,
        "test-invalid-corrected-candidate",
        root / "operator-invalid-corrected-commit-traces");
    require(committed_fallback.success && !committed_fallback.blocks.empty() &&
                committed_fallback.blocks.front().path == smave::SolvePath::direct_accept,
            "invalid corrected candidate did not use original solver fallback");
    bool corrected_unknown_context_rejected = false;
    try {
        (void)commit_runtime.commit_corrected_candidate(
            {{"unknown", 1.0}},
            "block-1",
            corrected_candidate,
            "test-corrected-candidate",
            root / "operator-corrected-unknown-context-traces");
    } catch (const std::invalid_argument&) {
        corrected_unknown_context_rejected = true;
    }
    require(corrected_unknown_context_rejected,
            "corrected candidate commit accepted an unknown context field");

    auto tampered = artifact;
    tampered.state_basis.front().front() += 1.0;
    bool rejected = false;
    try { tampered.validate(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "tampered latent operator artifact was accepted");

    auto invalid_qoi = artifact;
    invalid_qoi.qoi_outputs.push_back("not-a-state");
    invalid_qoi.seal();
    bool qoi_rejected = false;
    try { invalid_qoi.validate(); } catch (const std::invalid_argument&) { qoi_rejected = true; }
    require(qoi_rejected, "QoI-only name was allowed to masquerade as full state");

    smave::OperatorBenchmarkReport benchmark;
    benchmark.requests = 1;
    benchmark.repetitions = 1;
    benchmark.batches = 1;
    benchmark.average_batch = 1.0;
    benchmark.accepted = 1;
    benchmark.acceptance_rate = 1.0;
    benchmark.baseline_median_us = 2.0;
    benchmark.operator_median_us = 1.0;
    benchmark.online_speedup = 2.0;
    benchmark.training_wall_us = 0.5;
    benchmark.break_even_queries = 1;
    benchmark.projected_queries = 10;
    benchmark.amortized_speedup = 20.0 / 10.5;
    benchmark.candidate_qoi_within_tolerance = true;
    benchmark.same_accuracy = true;
    benchmark.break_even_met = true;
    benchmark.artifact_hash = artifact.artifact_hash;
    benchmark.certificate_hash = "test-certificate";
    benchmark.seal();
    benchmark.validate();
    benchmark.online_speedup = 3.0;
    bool report_rejected = false;
    try { benchmark.validate(); } catch (const std::invalid_argument&) {
        report_rejected = true;
    }
    require(report_rejected, "tampered operator benchmark evidence was accepted");

    smave::OperatorBenchmarkReport paired_benchmark;
    paired_benchmark.schema_version = 3;
    paired_benchmark.requests = 1;
    paired_benchmark.repetitions = 1;
    paired_benchmark.batches = 1;
    paired_benchmark.average_batch = 1.0;
    paired_benchmark.accepted = 1;
    paired_benchmark.acceptance_rate = 1.0;
    paired_benchmark.baseline_median_us = 0.9;
    paired_benchmark.operator_median_us = 1.0;
    paired_benchmark.online_speedup = 1.1;
    paired_benchmark.paired_speedup_ci95_lower = 1.05;
    paired_benchmark.paired_speedup_ci95_upper = 1.15;
    paired_benchmark.paired_median_saving_us = 0.1;
    paired_benchmark.training_wall_us = 0.5;
    paired_benchmark.break_even_queries = 5;
    paired_benchmark.projected_queries = 10;
    paired_benchmark.amortized_speedup = 11.0 / 10.5;
    paired_benchmark.maximum_candidate_qoi_error = 2.0;
    paired_benchmark.candidate_qoi_within_tolerance = false;
    paired_benchmark.same_accuracy = true;
    paired_benchmark.break_even_met = true;
    paired_benchmark.artifact_hash = artifact.artifact_hash;
    paired_benchmark.certificate_hash = "paired-test-certificate";
    paired_benchmark.seal();
    paired_benchmark.validate();
    const auto paired_report_path = root / "paired-operator-benchmark.txt";
    smave::write_operator_benchmark_report(paired_benchmark, paired_report_path);
    const auto restored_paired =
        smave::read_operator_benchmark_report(paired_report_path);
    require(restored_paired.schema_version == 3 &&
                restored_paired.online_speedup == paired_benchmark.online_speedup &&
                restored_paired.paired_speedup_ci95_lower ==
                    paired_benchmark.paired_speedup_ci95_lower &&
                restored_paired.paired_median_saving_us ==
                    paired_benchmark.paired_median_saving_us,
            "paired operator benchmark roundtrip failed");
}

void test_latent_operator_smooth_nonlinear_capability(const std::filesystem::path& root) {
    const auto source = root / "NonlinearOperatorModel.mo";
    std::ofstream(source)
        << "model NonlinearOperatorModel\n"
        << "parameter Real p=1; Real x(start=0); Real y(start=0);\n"
        << "equation\n"
        << "(x-(p+1))+0.1*(x-(p+1))^3+0.1*(y-(2*p+1))=0;\n"
        << "(y-(2*p+1))+0.1*(y-(2*p+1))^3+0.1*(x-(p+1))=0;\n"
        << "end NonlinearOperatorModel;\n";
    const auto model = smave::compile_model(source);
    require(!model.blocks.front().linear && model.blocks.front().smooth,
            "nonlinear operator regression fixture did not compile as smooth nonlinear");
    const auto training = root / "nonlinear-operator-training";
    std::filesystem::create_directories(training);
    for (int sample = 0; sample < 8; ++sample) {
        std::ofstream(training / ("sample-" + std::to_string(sample) + ".conf"))
            << "p=" << 0.5 + sample * 0.1 << '\n';
    }
    const auto artifact = smave::train_latent_operator(
        model, "block-1", training, root / "nonlinear-operator-label-traces", 2, {"x", "y"});
    const smave::LatentOperatorExpert expert(artifact);
    const auto capability = expert.match(model.blocks.front());
    require(!capability.linear && capability.nonlinear && !capability.event_related,
            "latent operator did not expose smooth nonlinear capability");
    auto registry = smave::make_default_registry(model);
    smave::register_latent_operator(registry, artifact);
    auto bundle = smave::make_default_bundle(model);
    bundle.add_expert(
        artifact.expert_version,
        artifact.artifact_hash,
        registry.grant(artifact.expert_version).evidence_bundle);
    smave::RoutingConfig routing;
    routing.top_k = 1;
    routing.expert_allowlist.insert(artifact.expert_version);
    const auto outcome = smave::Runtime(model, registry, bundle, {}, routing).solve(
        {{"p", 1.1}}, root / "nonlinear-operator-runtime-traces");
    require(outcome.success && !outcome.blocks.empty() &&
                outcome.blocks.front().path == smave::SolvePath::corrected_accept &&
                outcome.blocks.front().attempted_experts.front() == artifact.expert_version &&
                outcome.blocks.front().gate.decision == smave::GateDecision::direct_accept,
            "smooth nonlinear latent operator did not pass correction and gate");
}

void test_block_graph_import_and_model_group(const std::filesystem::path& root) {
    const auto source = root / "GroupModel.mo";
    std::ofstream(source)
        << "model GroupModel\nparameter Real p = 0;\nReal x(start=0);\n"
        << "equation\nx = 2*p;\nend GroupModel;\n";
    const auto model = smave::compile_model(source);
    model.write(root / "group-model.ir");
    const auto bridge = root / "group.sbg";
    std::ofstream(bridge)
        << "SMAVE_SIMULINK_EXPORT 1\n"
        << "MODEL \"DelayedGroup\"\n"
        << "NODE \"source\" \"constant\" 0.1 1 \"value\" \"3\"\n"
        << "NODE \"delay\" \"unit_delay\" 0.1 1 \"initial\" \"1\"\n"
        << "NODE \"plant\" \"algebraic_model\" 0.1 1 \"ir\" \"group-model.ir\"\n"
        << "CONNECTION \"source\" \"out\" \"delay\" \"in\"\n"
        << "CONNECTION \"delay\" \"out\" \"plant\" \"p\"\n"
        << "END\n";
    const auto graph = smave::import_block_graph(bridge);
    require(graph.commit_order == std::vector<std::string>({"source", "delay", "plant"}),
            "block graph schedule is not deterministic");
    const auto graph_path = root / "group.ir";
    graph.write(graph_path);
    smave::ModelGroupRuntime runtime(smave::BlockGraphIR::read(graph_path), root);
    const auto first = runtime.execute({}, root / "group-first.trace");
    const auto second = runtime.execute({}, root / "group-second.trace");
    require(first.success && second.success, "model group execution failed");
    require(std::abs(first.outputs.at("plant.x") - 2.0) < 1.0e-12 &&
                std::abs(second.outputs.at("plant.x") - 6.0) < 1.0e-12,
            "unit delay did not commit group state at the tick boundary");
    require(first.maximum_connection_error == 0.0 && first.local_fallback_count == 0,
            "model group coupling gate failed");

    const auto native_slx = root / "native-basic.slx";
    const std::string native_diagram =
        "<?xml version=\"1.0\"?><Model Name=\"NativeBasic\"><System>"
        "<Block BlockType=\"Constant\" Name=\"source\" SID=\"1\">"
        "<P Name=\"Value\">3</P><P Name=\"SampleTime\">0.1</P></Block>"
        "<Block BlockType=\"Gain\" Name=\"scale\" SID=\"2\">"
        "<P Name=\"Gain\">2</P><P Name=\"SampleTime\">0.1</P></Block>"
        "<Block BlockType=\"UnitDelay\" Name=\"delay\" SID=\"3\">"
        "<P Name=\"InitialCondition\">1</P><P Name=\"SampleTime\">0.1</P></Block>"
        "<Line><P Name=\"Src\">1#out:1</P><P Name=\"Dst\">2#in:1</P></Line>"
        "<Line><P Name=\"Src\">2#out:1</P><P Name=\"Dst\">3#in:1</P></Line>"
        "</System></Model>";
    write_stored_zip(native_slx, {{"simulink/blockdiagram.xml", native_diagram}});
    const auto native_graph = smave::import_block_graph(native_slx);
    require(native_graph.model_id == "NativeBasic" && native_graph.nodes.size() == 3 &&
                native_graph.connections.size() == 2 &&
                native_graph.commit_order ==
                    std::vector<std::string>({"source", "scale", "delay"}),
            "native SLX block graph was not imported deterministically");
    smave::ModelGroupRuntime native_runtime(native_graph, root);
    const auto native_first = native_runtime.execute({}, root / "native-slx-first.trace");
    const auto native_second = native_runtime.execute({}, root / "native-slx-second.trace");
    require(native_first.success && native_second.success &&
                native_first.outputs.at("delay.out") == 1.0 &&
                native_second.outputs.at("delay.out") == 6.0 &&
                native_second.maximum_connection_error == 0.0,
            "native SLX graph did not execute with atomic UnitDelay semantics");

    const std::string branched_diagram =
        "<?xml version=\"1.0\"?><Model Name=\"NativeBranch\"><System>"
        "<Block BlockType=\"Constant\" Name=\"source\" SID=\"1\">"
        "<P Name=\"Value\">3</P><P Name=\"SampleTime\">0.1</P></Block>"
        "<Block BlockType=\"Gain\" Name=\"double\" SID=\"2\">"
        "<P Name=\"Gain\">2</P><P Name=\"SampleTime\">0.1</P></Block>"
        "<Block BlockType=\"Gain\" Name=\"triple\" SID=\"3\">"
        "<P Name=\"Gain\">3</P><P Name=\"SampleTime\">0.1</P></Block>"
        "<Block BlockType=\"Sum\" Name=\"total\" SID=\"4\">"
        "<P Name=\"Inputs\">+-</P><P Name=\"SampleTime\">0.1</P></Block>"
        "<Line><P Name=\"Src\">1#out:1</P>"
        "<Branch><P Name=\"Dst\">2#in:1</P>"
        "<Branch><P Name=\"Dst\">3#in:1</P></Branch></Branch></Line>"
        "<Line><P Name=\"Src\">2#out:1</P><P Name=\"Dst\">4#in:1</P></Line>"
        "<Line><P Name=\"Src\">3#out:1</P><P Name=\"Dst\">4#in:2</P></Line>"
        "</System></Model>";
    const auto branched_slx = root / "native-branch.slx";
    write_stored_zip(branched_slx, {{"simulink/blockdiagram.xml", branched_diagram}});
    const auto branched_graph = smave::import_block_graph(branched_slx);
    require(branched_graph.connections.size() == 4 &&
                branched_graph.commit_order ==
                    std::vector<std::string>({"source", "double", "triple", "total"}),
            "nested native SLX Branch fan-out was not flattened deterministically");
    const auto branched_result = smave::ModelGroupRuntime(branched_graph, root).execute(
        {}, root / "native-slx-branch.trace");
    require(branched_result.success && branched_result.outputs.at("total.out") == -3.0 &&
                branched_result.maximum_connection_error == 0.0,
            "native SLX signed Sum did not preserve Branch signal semantics");

    auto duplicate_branch = branched_diagram;
    const auto second_target = duplicate_branch.find("3#in:1");
    duplicate_branch.replace(second_target, std::string("3#in:1").size(), "2#in:1");
    const auto duplicate_branch_slx = root / "duplicate-branch.slx";
    write_stored_zip(
        duplicate_branch_slx, {{"simulink/blockdiagram.xml", duplicate_branch}});
    bool duplicate_branch_rejected{};
    try { (void)smave::import_block_graph(duplicate_branch_slx); }
    catch (const std::invalid_argument&) { duplicate_branch_rejected = true; }
    require(duplicate_branch_rejected,
            "native SLX Branch duplicate target bypassed the multiple-driver gate");

    auto missing_sum_input = branched_diagram;
    missing_sum_input.replace(
        missing_sum_input.find("<P Name=\"Inputs\">+-</P>"),
        std::string("<P Name=\"Inputs\">+-</P>").size(),
        "<P Name=\"Inputs\">+-+</P>");
    const auto missing_sum_input_slx = root / "missing-sum-input.slx";
    write_stored_zip(
        missing_sum_input_slx, {{"simulink/blockdiagram.xml", missing_sum_input}});
    bool missing_sum_input_rejected{};
    try { (void)smave::import_block_graph(missing_sum_input_slx); }
    catch (const std::invalid_argument&) { missing_sum_input_rejected = true; }
    require(missing_sum_input_rejected,
            "native SLX signed Sum accepted an unconnected declared input");

    auto invalid_sum_sign = branched_diagram;
    invalid_sum_sign.replace(
        invalid_sum_sign.find("<P Name=\"Inputs\">+-</P>"),
        std::string("<P Name=\"Inputs\">+-</P>").size(),
        "<P Name=\"Inputs\">+x</P>");
    const auto invalid_sum_sign_slx = root / "invalid-sum-sign.slx";
    write_stored_zip(
        invalid_sum_sign_slx, {{"simulink/blockdiagram.xml", invalid_sum_sign}});
    bool invalid_sum_sign_rejected{};
    try { (void)smave::import_block_graph(invalid_sum_sign_slx); }
    catch (const std::invalid_argument&) { invalid_sum_sign_rejected = true; }
    require(invalid_sum_sign_rejected, "native SLX accepted an invalid Sum sign");

    const auto switch_diagram = [](const std::string& name,
                                   const std::string& control,
                                   const std::string& criterion,
                                   const std::string& threshold) {
        return "<?xml version=\"1.0\"?><Model Name=\"" + name + "\"><System>"
            "<Block BlockType=\"Constant\" Name=\"true_value\" SID=\"1\">"
            "<P Name=\"Value\">10</P><P Name=\"SampleTime\">0.1</P></Block>"
            "<Block BlockType=\"Constant\" Name=\"control\" SID=\"2\">"
            "<P Name=\"Value\">" + control + "</P><P Name=\"SampleTime\">0.1</P></Block>"
            "<Block BlockType=\"Constant\" Name=\"false_value\" SID=\"3\">"
            "<P Name=\"Value\">-10</P><P Name=\"SampleTime\">0.1</P></Block>"
            "<Block BlockType=\"Switch\" Name=\"select\" SID=\"4\">"
            "<P Name=\"Criteria\">" + criterion + "</P><P Name=\"Threshold\">" +
            threshold + "</P><P Name=\"SampleTime\">0.1</P></Block>"
            "<Line><P Name=\"Src\">1#out:1</P><P Name=\"Dst\">4#in:1</P></Line>"
            "<Line><P Name=\"Src\">2#out:1</P><P Name=\"Dst\">4#in:2</P></Line>"
            "<Line><P Name=\"Src\">3#out:1</P><P Name=\"Dst\">4#in:3</P></Line>"
            "</System></Model>";
    };
    const auto execute_switch = [&](const std::string& file_name, const std::string& diagram) {
        const auto slx = root / file_name;
        write_stored_zip(slx, {{"simulink/blockdiagram.xml", diagram}});
        const auto graph = smave::import_block_graph(slx);
        return std::pair{
            graph,
            smave::ModelGroupRuntime(graph, root).execute({}, root / (file_name + ".trace"))};
    };
    const auto [strict_graph, strict_result] = execute_switch(
        "switch-gt.slx", switch_diagram(
            "SwitchGt", "2", "u2 &gt; Threshold", "2"));
    require(strict_result.success && strict_result.outputs.at("select.out") == -10.0 &&
                strict_graph.nodes.back().attributes.at("criterion") == "gt" &&
                strict_graph.nodes.back().attributes.at("threshold") == "2.000000",
            "native SLX strict Switch boundary semantics are incorrect");
    const auto [inclusive_graph, inclusive_result] = execute_switch(
        "switch-ge.slx", switch_diagram(
            "SwitchGe", "2", "u2 &gt;= Threshold", "2"));
    require(inclusive_result.success && inclusive_result.outputs.at("select.out") == 10.0 &&
                inclusive_graph.nodes.back().attributes.at("criterion") == "ge",
            "native SLX inclusive Switch boundary semantics are incorrect");
    const auto [nonzero_graph, nonzero_result] = execute_switch(
        "switch-nonzero.slx", switch_diagram(
            "SwitchNonzero", "-1", "u2 ~= 0", "99"));
    require(nonzero_result.success && nonzero_result.outputs.at("select.out") == 10.0 &&
                nonzero_graph.nodes.back().attributes.at("criterion") == "ne_zero",
            "native SLX nonzero Switch semantics are incorrect");

    const auto invalid_switch = root / "switch-invalid.slx";
    write_stored_zip(invalid_switch, {{"simulink/blockdiagram.xml", switch_diagram(
        "SwitchInvalid", "1", "u2 == Threshold", "1")}});
    bool invalid_switch_rejected{};
    try { (void)smave::import_block_graph(invalid_switch); }
    catch (const std::invalid_argument&) { invalid_switch_rejected = true; }
    require(invalid_switch_rejected, "native SLX accepted an unsupported Switch Criteria");

    auto inherited_diagram = native_diagram;
    inherited_diagram.replace(
        inherited_diagram.find("<P Name=\"SampleTime\">0.1</P>"),
        std::string("<P Name=\"SampleTime\">0.1</P>").size(),
        "<P Name=\"SampleTime\">-1</P>");
    const auto inherited_slx = root / "inherited.slx";
    write_stored_zip(inherited_slx, {{"simulink/blockdiagram.xml", inherited_diagram}});
    bool inherited_rejected{};
    try { (void)smave::import_block_graph(inherited_slx); }
    catch (const std::invalid_argument&) { inherited_rejected = true; }
    require(inherited_rejected, "native SLX inherited sample time was accepted");

    const auto traversal_slx = root / "traversal.slx";
    write_stored_zip(traversal_slx, {
        {"simulink/blockdiagram.xml", native_diagram}, {"../escape.xml", "bad"}});
    bool traversal_rejected{};
    try { (void)smave::import_block_graph(traversal_slx); }
    catch (const std::invalid_argument&) { traversal_rejected = true; }
    require(traversal_rejected, "native SLX ZIP path traversal was accepted");

    std::ofstream(root / "invalid.sbg")
        << "SMAVE_SIMULINK_EXPORT 1\nMODEL \"Bad\"\n"
        << "NODE \"x\" \"unsupported_block\" 0.1 0\nEND\n";
    bool rejected = false;
    try { (void)smave::import_block_graph(root / "invalid.sbg"); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "unsupported Simulink bridge block was silently accepted");
}

void test_restricted_hybrid_event_gate(const std::filesystem::path& root) {
    const auto path = root / "hybrid.ir";
    std::ofstream(path)
        << "SMAVE_HYBRID 1\n"
        << "MODEL \"PriorityReset\"\n"
        << "SAMPLE_TIME 0.1\n"
        << "INITIAL_MODE \"charging\"\n"
        << "STATE 2 \"x\" 0 \"y\" 0\n"
        << "MODES 2\n"
        << "MODE \"charging\" 2 \"x\" \"x + 1\" \"y\" \"y\"\n"
        << "MODE \"done\" 0\n"
        << "TRANSITIONS 2\n"
        << "TRANSITION \"low-priority\" \"charging\" \"done\" 1 \"x - 1.5\" 0\n"
        << "TRANSITION \"high-priority\" \"charging\" \"done\" 10 \"x - 1.5\" 2 "
           "\"x\" \"x + 10\" \"y\" \"x + 20\"\n"
        << "END\n";
    const auto program = smave::HybridProgramIR::read(path);
    require(program.sample_offset == 0.0,
            "hybrid v1 compatibility did not default sample offset to zero");
    const std::vector<smave::EventCandidate> candidates{
        {1, "high-priority", "charging"},
        {2, "low-priority", "charging"},
        {2, "high-priority", "wrong-mode"}};
    const auto result = smave::run_hybrid(program, 3, candidates);
    require(result.success && result.events.size() == 1,
            "hybrid event runner produced the wrong event count");
    require(result.events.front().tick == 2 &&
                result.events.front().transition_id == "high-priority",
            "same-tick event priority/order was not deterministic");
    require(std::abs(result.final_state.at("x") - 12.0) < 1.0e-12 &&
                std::abs(result.final_state.at("y") - 22.0) < 1.0e-12,
            "hybrid resets were not evaluated atomically from pre-reset state");
    require(result.candidate_count == 3 && result.accepted_candidates == 0 &&
                result.rejected_candidates == 3 && result.missed_events == 1 &&
                result.event_recall == 0.0 && !result.events.front().candidate_accepted,
            "wrong-time/mode event candidate bypassed the authoritative guard gate");

    const auto offset_path = root / "hybrid-offset.ir";
    std::ofstream(offset_path)
        << "SMAVE_HYBRID 2\n"
        << "MODEL \"OffsetPriorityReset\"\n"
        << "SAMPLE_TIME 0.5\n"
        << "SAMPLE_OFFSET 0.25\n"
        << "INITIAL_MODE \"charging\"\n"
        << "STATE 1 \"x\" 0\n"
        << "MODES 2\n"
        << "MODE \"charging\" 1 \"x\" \"x + 1\"\n"
        << "MODE \"done\" 0\n"
        << "TRANSITIONS 1\n"
        << "TRANSITION \"fire\" \"charging\" \"done\" 1 \"x - 0.5\" 0\n"
        << "END\n";
    const auto offset_program = smave::HybridProgramIR::read(offset_path);
    const auto offset_result = smave::run_hybrid(
        offset_program, 3, {{0, "fire", "charging"}});
    require(offset_result.success && offset_result.events.size() == 1 &&
                offset_result.events.front().tick == 0 &&
                std::abs(offset_result.events.front().time - 0.25) < 1.0e-12 &&
                offset_result.events.front().candidate_accepted,
            "standalone hybrid scheduler did not honor the first offset sample boundary");

    auto invalid_offset = offset_program;
    invalid_offset.sample_offset = invalid_offset.sample_time;
    bool invalid_offset_rejected{};
    try { invalid_offset.validate(); }
    catch (const std::invalid_argument&) { invalid_offset_rejected = true; }
    require(invalid_offset_rejected,
            "hybrid sample offset equal to the period was accepted");
}

void test_signed_release_lifecycle(const std::filesystem::path& root) {
    const auto hash_input = root / "sha256.txt";
    std::ofstream(hash_input) << "abc";
    require(smave::sha256_file(hash_input) ==
                "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            "C++ SHA-256 implementation failed the standard vector");

    const auto source = root / "Release.mo";
    std::ofstream(source)
        << "model Release\nparameter Real p=2;\nReal x(start=0);\n"
        << "equation\nx=p;\nend Release;\n";
    const auto model = smave::compile_model(source);
    const auto model_path = root / "release-model.ir";
    const auto bundle_path = root / "runtime.bundle";
    model.write(model_path);
    smave::AffineWarmStartArtifact release_artifact;
    release_artifact.model_source_hash = model.source_hash;
    release_artifact.block_fingerprint = model.blocks.front().fingerprint;
    release_artifact.features = {"p"};
    release_artifact.outputs = {"x"};
    release_artifact.feature_minimum = {1.0};
    release_artifact.feature_maximum = {3.0};
    release_artifact.coefficients = {{0.0, 1.0}};
    release_artifact.training_samples = 3;
    release_artifact.training_rmse = 0.0;
    release_artifact.seal();
    const auto expert_path = root / "release.expert";
    release_artifact.write(expert_path);
    const auto release_certificate = smave::verify_affine_warm_start(
        model, release_artifact, 1);
    const auto certificate_path = root / "release.verify";
    release_certificate.write(certificate_path);
    auto release_bundle = smave::make_default_bundle(model);
    release_bundle.add_expert(
        release_artifact.expert_version,
        release_artifact.artifact_hash,
        release_certificate.certificate_hash);
    release_bundle.write(bundle_path);

    const auto validation = root / "release-validation.txt";
    std::ofstream(validation)
        << "SMAVE_VALIDATION 2\n"
        << "scenarios=100\nsuccessful_scenarios=100\nadmitted_invocations=100\n"
        << "top_k_passes=100\nfull_fallbacks=0\noriginal_solver_failures=0\n"
        << "erroneous_accepts=0\nsafety_evaluations=100\n"
        << "safety_confidence_level=0.95\n"
        << "erroneous_accept_rate_upper_bound=0.029513049607039932\n"
        << "maximum_erroneous_accept_rate=0.05\n"
        << "top_k_pass_rate=1\nfallback_rate=0\n"
        << "top_k_target_met=1\nsafety_target_met=1\n"
        << "confidence_target_met=1\nEND\n";
    const auto performance = root / "release-performance.txt";
    std::ofstream(performance)
        << "SMAVE_PERFORMANCE 1\nscenarios=100\nrepetitions=1\nsamples=100\n"
        << "median_speedup=1.2\np99_speedup=1.01\n"
        << "baseline_failures=0\naccelerated_failures=0\ngate_mismatches=0\n"
        << "same_accuracy=1\np99_not_regressed=1\nEND\n";
    const auto shadow = smave::audit_release(
        model_path, bundle_path, validation, performance,
        "shadow", 0.0, 24.0, 100, 24.0);
    const auto shadow_path = root / "shadow.audit";
    shadow.write(shadow_path);
    auto legacy_shadow = shadow;
    legacy_shadow.schema_version = 2;
    legacy_shadow.seal();
    const auto legacy_shadow_path = root / "legacy-shadow.audit";
    legacy_shadow.write(legacy_shadow_path);
    require(smave::ReleaseAudit::read(legacy_shadow_path).schema_version == 2,
            "legacy release audit v2 is no longer readable");
    const auto audit = smave::audit_release(
        model_path, bundle_path, validation, performance,
        "canary", 0.1, 24.0, 100, 24.0, 0.95, shadow_path);
    require(audit.promotion_ready, "valid canary evidence did not pass release gates");
    const auto audit_path = root / "release.audit";
    audit.write(audit_path);

    const auto release_dataset_source = root / "release-dataset-source";
    std::filesystem::create_directories(release_dataset_source);
    std::ofstream(release_dataset_source / "case.conf") << "p=2\n";
    const smave::DatasetStore release_dataset_store(root / "release-dataset-store");
    const auto release_dataset = release_dataset_store.snapshot(
        release_dataset_source, "release-validation");
    const auto release_dataset_manifest = release_dataset_store.version_directory(
        release_dataset.dataset_id, release_dataset.version) / "dataset.manifest";
    smave::ValidationReport lineage_validation_report;
    lineage_validation_report.schema_version = 3;
    lineage_validation_report.dataset_id = release_dataset.dataset_id;
    lineage_validation_report.dataset_version = release_dataset.version;
    lineage_validation_report.dataset_manifest_hash = release_dataset.manifest_hash;
    lineage_validation_report.scenarios = 100;
    lineage_validation_report.successful_scenarios = 100;
    lineage_validation_report.admitted_invocations = 100;
    lineage_validation_report.top_k_passes = 100;
    lineage_validation_report.safety_evaluations = 100;
    lineage_validation_report.top_k_pass_rate = 1.0;
    lineage_validation_report.erroneous_accept_rate_upper_bound =
        smave::binomial_proportion_upper_bound(0, 100);
    lineage_validation_report.top_k_target_met = true;
    lineage_validation_report.safety_target_met = true;
    lineage_validation_report.confidence_target_met = true;
    const auto lineage_validation_path = root / "lineage-validation.txt";
    smave::write_validation_report(
        lineage_validation_report, lineage_validation_path);
    smave::OperatorBenchmarkReport lineage_performance_report;
    lineage_performance_report.schema_version = 2;
    lineage_performance_report.dataset_id = release_dataset.dataset_id;
    lineage_performance_report.dataset_version = release_dataset.version;
    lineage_performance_report.dataset_manifest_hash = release_dataset.manifest_hash;
    lineage_performance_report.requests = 100;
    lineage_performance_report.repetitions = 1;
    lineage_performance_report.batches = 1;
    lineage_performance_report.average_batch = 100.0;
    lineage_performance_report.accepted = 100;
    lineage_performance_report.acceptance_rate = 1.0;
    lineage_performance_report.baseline_median_us = 120.0;
    lineage_performance_report.operator_median_us = 100.0;
    lineage_performance_report.online_speedup = 1.2;
    lineage_performance_report.training_wall_us = 1000.0;
    lineage_performance_report.break_even_queries = 50;
    lineage_performance_report.projected_queries = 100;
    lineage_performance_report.amortized_speedup = 12000.0 / 11000.0;
    lineage_performance_report.candidate_qoi_within_tolerance = true;
    lineage_performance_report.same_accuracy = true;
    lineage_performance_report.break_even_met = true;
    lineage_performance_report.artifact_hash = release_artifact.artifact_hash;
    lineage_performance_report.certificate_hash =
        release_certificate.certificate_hash;
    lineage_performance_report.seal();
    const auto lineage_performance_path = root / "lineage-performance.txt";
    smave::write_operator_benchmark_report(
        lineage_performance_report, lineage_performance_path);
    const auto lineage_shadow = smave::audit_release(
        model_path, bundle_path, lineage_validation_path, lineage_performance_path,
        "shadow", 0.0, 24.0, 100, 24.0, 0.95, {}, release_dataset_manifest);
    const auto lineage_shadow_path = root / "lineage-shadow.audit";
    lineage_shadow.write(lineage_shadow_path);
    const auto lineage_audit = smave::audit_release(
        model_path, bundle_path, lineage_validation_path, lineage_performance_path,
        "canary", 0.1, 24.0, 100, 24.0, 0.95,
        lineage_shadow_path, release_dataset_manifest);
    require(lineage_audit.promotion_ready &&
                lineage_audit.dataset_version == release_dataset.version,
            "release audit did not bind the verified dataset version");
    const auto lineage_audit_path = root / "lineage-canary.audit";
    lineage_audit.write(lineage_audit_path);

    std::ofstream(release_dataset_source / "case.conf", std::ios::app) << "q=3\n";
    const auto changed_dataset = release_dataset_store.snapshot(
        release_dataset_source, "release-validation");
    const auto changed_manifest = release_dataset_store.version_directory(
        changed_dataset.dataset_id, changed_dataset.version) / "dataset.manifest";
    bool dataset_parent_rejected{};
    try {
        (void)smave::audit_release(
            model_path, bundle_path, lineage_validation_path, lineage_performance_path,
            "canary", 0.1, 24.0, 100, 24.0, 0.95,
            lineage_shadow_path, changed_manifest);
    } catch (const std::invalid_argument&) {
        dataset_parent_rejected = true;
    }
    require(dataset_parent_rejected,
            "canary audit changed dataset version relative to its shadow parent");

    const auto insufficient_validation = root / "insufficient-validation.txt";
    std::ofstream(insufficient_validation)
        << "SMAVE_VALIDATION 2\n"
        << "scenarios=2\nsuccessful_scenarios=2\nadmitted_invocations=2\n"
        << "top_k_passes=2\nfull_fallbacks=0\noriginal_solver_failures=0\n"
        << "erroneous_accepts=0\nsafety_evaluations=2\n"
        << "safety_confidence_level=0.95\n"
        << "erroneous_accept_rate_upper_bound=0.7763932022500211\n"
        << "maximum_erroneous_accept_rate=0.05\n"
        << "top_k_pass_rate=1\nfallback_rate=0\n"
        << "top_k_target_met=1\nsafety_target_met=1\n"
        << "confidence_target_met=0\nEND\n";
    const auto insufficient_audit = smave::audit_release(
        model_path, bundle_path, insufficient_validation, performance,
        "shadow", 0.0, 24.0, 2, 24.0);
    require(!insufficient_audit.safety_met && !insufficient_audit.promotion_ready,
            "zero observed errors with an insufficient confidence bound was promoted");

    const auto forged_validation = root / "forged-validation.txt";
    std::filesystem::copy_file(insufficient_validation, forged_validation);
    auto forged_text = std::ifstream(forged_validation);
    std::string forged_contents{
        std::istreambuf_iterator<char>(forged_text),
        std::istreambuf_iterator<char>()};
    const auto forged_position = forged_contents.find(
        "erroneous_accept_rate_upper_bound=0.7763932022500211");
    forged_contents.replace(
        forged_position,
        std::string("erroneous_accept_rate_upper_bound=0.7763932022500211").size(),
        "erroneous_accept_rate_upper_bound=0.01");
    std::ofstream(forged_validation, std::ios::trunc) << forged_contents;
    bool forged_rejected = false;
    try {
        (void)smave::audit_release(
            model_path, bundle_path, forged_validation, performance,
            "shadow", 0.0, 24.0, 2, 24.0);
    } catch (const std::invalid_argument&) {
        forged_rejected = true;
    }
    require(forged_rejected,
            "forged safety confidence upper bound bypassed release recomputation");

    const auto regressed = root / "regressed-performance.txt";
    std::ofstream(regressed)
        << "SMAVE_PERFORMANCE 1\nmedian_speedup=1.2\nbaseline_failures=0\n"
        << "accelerated_failures=0\ngate_mismatches=0\nsame_accuracy=1\n"
        << "p99_not_regressed=0\nEND\n";
    const auto blocked = smave::audit_release(
        model_path, bundle_path, validation, regressed,
        "canary", 0.1, 24.0, 100, 24.0, 0.95, shadow_path);
    require(!blocked.performance_met && !blocked.promotion_ready,
            "P99 regression was allowed through the release gate");
    bool parent_required = false;
    try {
        (void)smave::audit_release(
            model_path, bundle_path, validation, performance,
            "canary", 0.1, 24.0, 100, 24.0);
    } catch (const std::invalid_argument&) { parent_required = true; }
    require(parent_required, "canary audit bypassed the passed shadow evidence chain");

    const auto key_path = root / "release.key";
    std::ofstream(key_path) << "0123456789abcdef0123456789abcdef";
    auto first_manifest = smave::create_release_manifest(
        bundle_path, audit_path, key_path, "release-1",
        model_path, expert_path, certificate_path);
    const auto first_path = root / "release-1.manifest";
    first_manifest.write(first_path);
    first_manifest.validate("0123456789abcdef0123456789abcdef");
    auto legacy_manifest = first_manifest;
    legacy_manifest.schema_version = 1;
    legacy_manifest.seal_and_sign("0123456789abcdef0123456789abcdef");
    const auto legacy_manifest_path = root / "legacy-release.manifest";
    legacy_manifest.write(legacy_manifest_path);
    const auto restored_legacy_manifest = smave::ReleaseManifest::read(
        legacy_manifest_path);
    restored_legacy_manifest.validate("0123456789abcdef0123456789abcdef");
    require(restored_legacy_manifest.schema_version == 1,
            "legacy signed release manifest v1 is no longer readable");

    const auto lineage_manifest = smave::create_release_manifest(
        bundle_path, lineage_audit_path, key_path, "release-lineage",
        model_path, expert_path, certificate_path, release_dataset_manifest);
    require(lineage_manifest.dataset_id == release_dataset.dataset_id &&
                lineage_manifest.dataset_version == release_dataset.version,
            "signed release manifest lost dataset lineage");
    bool mismatched_dataset_rejected{};
    try {
        (void)smave::create_release_manifest(
            bundle_path, lineage_audit_path, key_path, "release-wrong-data",
            model_path, expert_path, certificate_path, changed_manifest);
    } catch (const std::invalid_argument&) {
        mismatched_dataset_rejected = true;
    }
    require(mismatched_dataset_rejected,
            "release manifest accepted a dataset differing from its audit");
    const auto lineage_manifest_path = root / "release-lineage.manifest";
    lineage_manifest.write(lineage_manifest_path);
    smave::ReleaseStore lineage_store(root / "release-lineage-store");
    (void)lineage_store.activate(
        lineage_manifest_path, bundle_path, lineage_audit_path,
        lineage_shadow_path, model_path, expert_path, certificate_path,
        key_path, release_dataset_manifest);
    std::ofstream(
        lineage_store.active_directory() / "dataset.manifest",
        std::ios::app) << "tamper";
    bool dataset_manifest_rejected{};
    try {
        (void)lineage_store.verify_active(key_path);
    } catch (const std::invalid_argument&) {
        dataset_manifest_rejected = true;
    }
    require(dataset_manifest_rejected,
            "active release accepted a tampered dataset manifest");
    auto tampered_manifest = first_manifest;
    tampered_manifest.bundle_hash.front() = tampered_manifest.bundle_hash.front() == '0' ? '1' : '0';
    bool signature_rejected = false;
    try { tampered_manifest.validate("0123456789abcdef0123456789abcdef"); }
    catch (const std::invalid_argument&) { signature_rejected = true; }
    require(signature_rejected, "tampered signed release manifest was accepted");

    smave::ReleaseStore store(root / "release-store");
    const auto first_state = store.activate(
        first_path, bundle_path, audit_path, shadow_path,
        model_path, expert_path, certificate_path, key_path);
    require(first_state.schema_version == 2 && first_state.generation == 1 &&
                first_state.current_release == "release-1" &&
                !first_state.key_id.empty() && !first_state.signature.empty(),
            "first release activation produced the wrong state");
    first_state.validate("0123456789abcdef0123456789abcdef");
    smave::ReleaseState legacy_state;
    legacy_state.schema_version = 1;
    legacy_state.generation = 1;
    legacy_state.current_release = "release-1";
    legacy_state.seal();
    const auto legacy_state_path = root / "legacy-release.state";
    legacy_state.write_atomic(legacy_state_path);
    require(smave::ReleaseState::read(legacy_state_path).schema_version == 1,
            "legacy release state v1 is no longer readable");
    bool legacy_state_authentication_rejected{};
    try {
        legacy_state.validate("0123456789abcdef0123456789abcdef");
    } catch (const std::invalid_argument&) {
        legacy_state_authentication_rejected = true;
    }
    require(legacy_state_authentication_rejected,
            "unauthenticated legacy release state was accepted for production use");
    bool immutable_rejected = false;
    try { (void)store.activate(
        first_path, bundle_path, audit_path, shadow_path,
        model_path, expert_path, certificate_path, key_path); }
    catch (const std::invalid_argument&) { immutable_rejected = true; }
    require(immutable_rejected, "an immutable release id was overwritten");

    const auto second_manifest = smave::create_release_manifest(
        bundle_path, audit_path, key_path, "release-2",
        model_path, expert_path, certificate_path);
    const auto second_path = root / "release-2.manifest";
    second_manifest.write(second_path);
    const auto second_state = store.activate(
        second_path, bundle_path, audit_path, shadow_path,
        model_path, expert_path, certificate_path, key_path);
    require(second_state.generation == 2 && second_state.current_release == "release-2" &&
                second_state.previous_release == "release-1",
            "atomic release promotion did not retain rollback state");
    const auto rolled_back = store.rollback(key_path);
    require(rolled_back.generation == 3 && rolled_back.current_release == "release-1" &&
                rolled_back.previous_release == "release-2",
            "one-step signed release rollback failed");
    require(store.verify_active(key_path).release_id == "release-1",
            "active production release was not reverified after rollback");
    require(std::filesystem::exists(
                root / "release-store/state-history/00000000000000000003.state"),
            "release state generation was not durably journaled");

    std::ofstream(root / "release-store/state", std::ios::trunc) << "torn state";
    const auto recovered = store.status();
    require(recovered.generation == 3 && recovered.current_release == "release-1" &&
                smave::ReleaseState::read(root / "release-store/state").state_hash ==
                    recovered.state_hash,
            "release status did not recover a torn primary state from history");

    const auto resumed_root = root / "release-resume-store";
    std::filesystem::create_directories(resumed_root / "releases");
    std::filesystem::copy(
        root / "release-store/releases/release-2",
        resumed_root / "releases/release-2",
        std::filesystem::copy_options::recursive);
    smave::ReleaseStore resumed_store(resumed_root);
    const auto resumed = resumed_store.activate(
        second_path, bundle_path, audit_path, shadow_path,
        model_path, expert_path, certificate_path, key_path);
    require(resumed.generation == 1 && resumed.current_release == "release-2",
            "activation did not resume after the immutable release directory committed");

#if !defined(_WIN32)
    const auto lock_path = root / "release-store/.release-lock-v2";
    const auto crashed_locker = ::fork();
    require(crashed_locker >= 0, "could not fork crash-recovery lock probe");
    if (crashed_locker == 0) {
        const auto descriptor = ::open(lock_path.c_str(), O_CREAT | O_RDWR, 0600);
        if (descriptor < 0 || ::flock(descriptor, LOCK_EX) != 0) std::_Exit(2);
        std::_Exit(0);
    }
    int crashed_status{};
    require(::waitpid(crashed_locker, &crashed_status, 0) == crashed_locker &&
                WIFEXITED(crashed_status) && WEXITSTATUS(crashed_status) == 0,
            "crash-recovery lock probe failed before abrupt exit");
    const auto after_crashed_lock = store.rollback(key_path);
    require(after_crashed_lock.generation == 4 &&
                after_crashed_lock.current_release == "release-2",
            "release store remained locked after an owning process exited abruptly");

    const auto concurrent_rollback = [&]() -> pid_t {
        const auto process = ::fork();
        require(process >= 0, "could not fork concurrent release operation");
        if (process == 0) {
            try {
                (void)smave::ReleaseStore(root / "release-store").rollback(key_path);
                std::_Exit(0);
            } catch (const std::exception&) {
                std::_Exit(3);
            }
        }
        return process;
    };
    const auto first_process = concurrent_rollback();
    const auto second_process = concurrent_rollback();
    int first_process_status{};
    int second_process_status{};
    require(::waitpid(first_process, &first_process_status, 0) == first_process &&
                ::waitpid(second_process, &second_process_status, 0) == second_process &&
                WIFEXITED(first_process_status) && WEXITSTATUS(first_process_status) == 0 &&
                WIFEXITED(second_process_status) && WEXITSTATUS(second_process_status) == 0,
            "concurrent release operations were not serialized across processes");
    const auto concurrent_state = store.status();
    require(concurrent_state.generation == 6 &&
                concurrent_state.current_release == "release-2" &&
                concurrent_state.previous_release == "release-1",
            "concurrent release operations lost a committed generation");
#endif
    const auto verified_snapshot = store.verified_active(key_path);
    require(verified_snapshot.state.current_release ==
                verified_snapshot.manifest.release_id &&
                verified_snapshot.directory.filename() ==
                    verified_snapshot.state.current_release,
            "verified active release mixed state and payload generations");

    const auto forged_store_root = root / "forged-state-store";
    std::filesystem::copy(
        root / "release-store", forged_store_root,
        std::filesystem::copy_options::recursive);
    auto forged_state = smave::ReleaseStore(forged_store_root).status();
    forged_state.current_release = "release-1";
    forged_state.previous_release = "release-2";
    forged_state.seal();
    forged_state.write_atomic(forged_store_root / "state");
    forged_state.write_atomic(
        forged_store_root / "state-history/00000000000000000006.state");
    bool forged_state_rejected{};
    try {
        (void)smave::ReleaseStore(forged_store_root).verified_active(key_path);
    } catch (const std::invalid_argument&) {
        forged_state_rejected = true;
    }
    require(forged_state_rejected,
            "rehashed but unsigned active-release rollback bypassed state authority");

    auto tampered_bundle = root / "tampered.bundle";
    std::filesystem::copy_file(bundle_path, tampered_bundle);
    std::ofstream(tampered_bundle, std::ios::app) << "tamper";
    const auto third_manifest = smave::create_release_manifest(
        bundle_path, audit_path, key_path, "release-3",
        model_path, expert_path, certificate_path);
    const auto third_path = root / "release-3.manifest";
    third_manifest.write(third_path);
    bool bundle_rejected = false;
    try { (void)store.activate(
        third_path, tampered_bundle, audit_path, shadow_path,
        model_path, expert_path, certificate_path, key_path); }
    catch (const std::exception&) { bundle_rejected = true; }
    require(bundle_rejected, "tampered bundle bypassed signed activation");
}

void test_continuous_modelica_zero_crossing(const std::filesystem::path& root) {
    const auto source = root / "ContinuousAtomic.mo";
    std::ofstream(source)
        << "model ContinuousAtomic\n"
        << "  Real x(start=0, nominal=1);\n"
        << "  Real y(start=0, nominal=1);\n"
        << "equation\n"
        << "  der(x) = 1;\n"
        << "  der(y) = 0;\n"
        << "  when x >= 1 then\n"
        << "    reinit(x, pre(x) + 10);\n"
        << "    reinit(y, pre(x) + 20);\n"
        << "  end when;\n"
        << "end ContinuousAtomic;\n";
    const auto model = smave::compile_continuous_model(source, "ContinuousAtomic");
    require(model.states.size() == 2 && model.events.size() == 1 &&
                model.events.front().direction == 1,
            "continuous Modelica frontend lost state/event semantics");
    const auto ir_path = root / "continuous.ir";
    model.write(ir_path);
    const auto restored = smave::ContinuousHybridIR::read(ir_path);
    auto result = smave::simulate_continuous(restored, 0.0, 1.1, 0.2);
    require(result.success && result.events.size() == 1,
            "continuous zero-crossing runner failed");
    require(std::abs(result.events.front().time - 1.0) < 1.0e-8 &&
                result.events.front().guard_residual < 1.0e-8,
            "continuous root localization did not meet the event gate");
    require(std::abs(result.events.front().post_state.at("x") - 11.0) < 1.0e-7 &&
                std::abs(result.events.front().post_state.at("y") - 21.0) < 1.0e-7,
            "continuous resets were not atomic from one pre-event state");

    const auto reference = root / "continuous.ref";
    std::ofstream(reference)
        << "SMAVE_CONTINUOUS_REFERENCE 1\nEVENTS 1\n"
        << "EVENT \"event-1\" 1.0 0.00000001\nEND\n";
    smave::validate_continuous_reference(result, reference);
    require(result.reference_order_matched && result.reference_time_matched,
            "valid continuous reference was rejected");
    const auto wrong_reference = root / "continuous-wrong.ref";
    std::ofstream(wrong_reference)
        << "SMAVE_CONTINUOUS_REFERENCE 1\nEVENTS 1\n"
        << "EVENT \"wrong-event\" 0.9 0.00000001\nEND\n";
    smave::validate_continuous_reference(result, wrong_reference);
    require(!result.reference_order_matched && !result.reference_time_matched,
            "incorrect continuous event order/time was accepted");

    const auto implicit = root / "ImplicitDae.mo";
    std::ofstream(implicit)
        << "model ImplicitDae\nReal x(start=0);\nReal y(start=0);\n"
        << "equation\nder(x)=y;\nx+y=1;\nend ImplicitDae;\n";
    bool rejected = false;
    try { (void)smave::compile_continuous_model(implicit); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "implicit/algebraic DAE was silently treated as explicit ODE");

    const auto binding = root / "StateBinding.mo";
    std::ofstream(binding)
        << "model StateBinding\nReal x = 1;\nequation\nder(x)=0;\nend StateBinding;\n";
    bool binding_rejected = false;
    try { (void)smave::compile_continuous_model(binding); }
    catch (const std::invalid_argument&) { binding_rejected = true; }
    require(binding_rejected, "continuous state binding equation was silently erased");

    auto initially_active = restored;
    initially_active.states.front().start = 2.0;
    const auto initial_result = smave::simulate_continuous(
        initially_active, 0.0, 0.1, 0.01);
    require(initial_result.success && initial_result.events.size() == 1 &&
                std::abs(initial_result.events.front().time) < 1.0e-12 &&
                std::abs(initial_result.events.front().pre_state.at("x") - 2.0) < 1.0e-12 &&
                std::abs(initial_result.events.front().post_state.at("x") - 12.0) < 1.0e-12 &&
                std::abs(initial_result.events.front().post_state.at("y") - 22.0) < 1.0e-12,
            "initially active when condition did not execute atomically");

    const auto initial_cascade_source = root / "InitialCascade.mo";
    std::ofstream(initial_cascade_source)
        << "model InitialCascade\n"
        << "Real x(start=1);\nReal y(start=0);\nReal z(start=0);\n"
        << "equation\nder(x)=0;\nder(y)=0;\nder(z)=0;\n"
        << "when x >= 1 then\nreinit(y,pre(y)+1);\nend when;\n"
        << "when y >= 1 then\nreinit(z,pre(y)+10);\nend when;\n"
        << "end InitialCascade;\n";
    const auto initial_cascade = smave::compile_continuous_model(initial_cascade_source);
    const auto initial_cascade_result = smave::simulate_continuous(
        initial_cascade, 0.0, 0.1, 0.01);
    require(initial_cascade_result.success && initial_cascade_result.events.size() == 2 &&
                std::abs(initial_cascade_result.events[0].time) < 1.0e-12 &&
                std::abs(initial_cascade_result.events[1].time) < 1.0e-12 &&
                std::abs(initial_cascade_result.final_state.at("y") - 1.0) < 1.0e-12 &&
                std::abs(initial_cascade_result.final_state.at("z") - 10.0) < 1.0e-12,
            "initial event iteration did not reach the expected fixed point");

    const auto initial_conflict_source = root / "InitialConflict.mo";
    std::ofstream(initial_conflict_source)
        << "model InitialConflict\nReal x(start=1);\nReal y(start=0);\n"
        << "equation\nder(x)=0;\nder(y)=0;\n"
        << "when x >= 1 then\nreinit(y,1);\nend when;\n"
        << "when x >= 1 then\nreinit(y,2);\nend when;\n"
        << "end InitialConflict;\n";
    const auto initial_conflict = smave::compile_continuous_model(initial_conflict_source);
    const auto initial_conflict_result = smave::simulate_continuous(
        initial_conflict, 0.0, 0.1, 0.01);
    require(!initial_conflict_result.success && initial_conflict_result.events.empty() &&
                initial_conflict_result.message.find("conflicting reset") != std::string::npos &&
                std::abs(initial_conflict_result.final_state.at("y")) < 1.0e-12,
            "conflicting initial events were partially committed");

    const auto exponential_source = root / "Exponential.mo";
    std::ofstream(exponential_source)
        << "model Exponential\nReal x(start=1);\nequation\nder(x)=x;\nend Exponential;\n";
    const auto exponential = smave::compile_continuous_model(exponential_source);
    const auto exponential_result = smave::simulate_continuous(
        exponential, 0.0, 1.0, 1.0);
    require(exponential_result.success && exponential_result.rejected_steps > 0 &&
                std::abs(exponential_result.final_state.at("x") - std::exp(1.0)) < 2.0e-6,
            "adaptive continuous integrator did not reject/coarsen an inaccurate step");

    const auto simultaneous_source = root / "Simultaneous.mo";
    std::ofstream(simultaneous_source)
        << "model Simultaneous\nReal x(start=0);\nequation\nder(x)=1;\n"
        << "when x >= 1 then\nreinit(x,2);\nend when;\n"
        << "when x >= 1 then\nreinit(x,3);\nend when;\n"
        << "end Simultaneous;\n";
    const auto simultaneous = smave::compile_continuous_model(simultaneous_source);
    const auto simultaneous_result = smave::simulate_continuous(
        simultaneous, 0.0, 1.1, 0.2);
    require(!simultaneous_result.success &&
                simultaneous_result.message.find("conflicting reset") != std::string::npos,
            "conflicting simultaneous continuous resets were partially committed");

    const auto atomic_simultaneous_source = root / "AtomicSimultaneous.mo";
    std::ofstream(atomic_simultaneous_source)
        << "model AtomicSimultaneous\n"
        << "Real x(start=0);\nReal y(start=0);\nReal z(start=0);\n"
        << "equation\nder(x)=1;\nder(y)=0;\nder(z)=0;\n"
        << "when x >= 1 then\nreinit(y,pre(x)+10);\nend when;\n"
        << "when x >= 1 then\nreinit(z,pre(x)+20);\nend when;\n"
        << "end AtomicSimultaneous;\n";
    const auto atomic_simultaneous = smave::compile_continuous_model(
        atomic_simultaneous_source);
    const auto atomic_result = smave::simulate_continuous(
        atomic_simultaneous, 0.0, 1.1, 0.2);
    require(atomic_result.success && atomic_result.events.size() == 2 &&
                std::abs(atomic_result.events[0].time - 1.0) < 1.0e-8 &&
                std::abs(atomic_result.events[1].time - 1.0) < 1.0e-8,
            "non-conflicting simultaneous continuous events were not batched");
    require(std::abs(atomic_result.events[0].pre_state.at("x") - 1.0) < 1.0e-8 &&
                atomic_result.events[0].pre_state == atomic_result.events[1].pre_state &&
                atomic_result.events[0].post_state == atomic_result.events[1].post_state &&
                std::abs(atomic_result.final_state.at("y") - 11.0) < 1.0e-8 &&
                std::abs(atomic_result.final_state.at("z") - 21.0) < 1.0e-8,
            "simultaneous resets did not use one pre-state and one atomic post-state");

    const auto grazing_source = root / "Grazing.mo";
    std::ofstream(grazing_source)
        << "model Grazing\n"
        << "Real x(start=-0.9);\nReal y(start=0);\n\n"
        << "equation\nder(x)=1;\nder(y)=0;\n"
        << "when x*x <= 0 then\nreinit(y,pre(y)+1);\nend when;\n"
        << "end Grazing;\n";
    const auto grazing = smave::compile_continuous_model(grazing_source);
    const auto grazing_result = smave::simulate_continuous(
        grazing, 0.0, 2.0, 0.5);
    require(grazing_result.success && grazing_result.events.size() == 1 &&
                grazing_result.grazing_events == 1 &&
                grazing_result.events.front().grazing &&
                std::abs(grazing_result.events.front().time - 0.9) < 1.0e-6 &&
                std::abs(grazing_result.final_state.at("y") - 1.0) < 1.0e-8,
            "tangential continuous root was not localized and committed");

    const auto cascading_source = root / "CascadingEvents.mo";
    std::ofstream(cascading_source)
        << "model CascadingEvents\n"
        << "Real x(start=0);\nReal y(start=0);\nReal z(start=0);\n"
        << "equation\nder(x)=1;\nder(y)=0;\nder(z)=0;\n"
        << "when x >= 1 then\nreinit(y,pre(y)+1);\nend when;\n"
        << "when y >= 1 then\nreinit(z,pre(y)+10);\nend when;\n"
        << "end CascadingEvents;\n";
    const auto cascading = smave::compile_continuous_model(cascading_source);
    require(cascading.events[0].resets[0].expression.find("__smave_pre_y") !=
                std::string::npos,
            "continuous compiler erased pre(state) from reset expression");
    const auto cascading_result = smave::simulate_continuous(
        cascading, 0.0, 1.1, 0.2);
    require(cascading_result.success && cascading_result.events.size() == 2 &&
                cascading_result.events[0].id == "event-1" &&
                cascading_result.events[1].id == "event-2" &&
                std::abs(cascading_result.events[0].time - 1.0) < 1.0e-8 &&
                std::abs(cascading_result.events[1].time - 1.0) < 1.0e-8 &&
                std::abs(cascading_result.events[1].pre_state.at("y") - 1.0) < 1.0e-8 &&
                std::abs(cascading_result.final_state.at("z") - 10.0) < 1.0e-8,
            "continuous event iteration did not reach the expected fixed point");

    const auto cascading_conflict_source = root / "CascadingConflict.mo";
    std::ofstream(cascading_conflict_source)
        << "model CascadingConflict\n"
        << "Real x(start=0);\nReal y(start=0);\nReal z(start=0);\n"
        << "equation\nder(x)=1;\nder(y)=0;\nder(z)=0;\n"
        << "when x >= 1 then\nreinit(y,1);\nend when;\n"
        << "when y >= 1 then\nreinit(z,2);\nend when;\n"
        << "when y >= 1 then\nreinit(z,3);\nend when;\n"
        << "end CascadingConflict;\n";
    const auto cascading_conflict = smave::compile_continuous_model(
        cascading_conflict_source);
    const auto cascading_conflict_result = smave::simulate_continuous(
        cascading_conflict, 0.0, 1.1, 0.2);
    require(!cascading_conflict_result.success &&
                cascading_conflict_result.message.find("conflicting reset") !=
                    std::string::npos &&
                cascading_conflict_result.events.empty() &&
                cascading_conflict_result.final_state.contains("z") &&
                std::abs(cascading_conflict_result.final_state.at("y")) < 1.0e-12 &&
                std::abs(cascading_conflict_result.final_state.at("z")) < 1.0e-12,
            "conflicting cascading reset was partially committed");
}

void test_multirate_model_group_zero_order_hold(const std::filesystem::path& root) {
    const auto bridge = root / "multirate.sbg";
    std::ofstream(bridge)
        << "SMAVE_SIMULINK_EXPORT 2\n"
        << "MODEL \"MultirateAccumulator\"\n"
        << "NODE \"one\" \"constant\" 0.1 0 1 \"value\" \"1\"\n"
        << "NODE \"fast-delay\" \"unit_delay\" 0.1 0 1 \"initial\" \"0\"\n"
        << "NODE \"accumulator\" \"sum\" 0.1 0 0\n"
        << "NODE \"slow-gain\" \"gain\" 0.2 0.1 2 \"gain\" \"10\" \"initial_output\" \"-10\"\n"
        << "NODE \"slow-delay\" \"unit_delay\" 0.2 0.1 1 \"initial\" \"-20\"\n"
        << "CONNECTION \"one\" \"out\" \"accumulator\" \"one\"\n"
        << "CONNECTION \"fast-delay\" \"out\" \"accumulator\" \"feedback\"\n"
        << "CONNECTION \"accumulator\" \"out\" \"fast-delay\" \"in\"\n"
        << "CONNECTION \"accumulator\" \"out\" \"slow-gain\" \"in\"\n"
        << "CONNECTION \"slow-gain\" \"out\" \"slow-delay\" \"in\"\n"
        << "END\n";
    const auto graph = smave::import_block_graph(bridge);
    smave::ModelGroupRuntime runtime(graph, root);
    const auto result = runtime.execute_multirate(
        0.4, 0.1, {}, root / "multirate-traces");
    require(result.success && result.ticks.size() == 5,
            "multirate model group did not execute all base ticks");
    require(result.ticks[0].executed_nodes ==
                std::vector<std::string>({"one", "fast-delay", "accumulator"}) &&
                result.ticks[1].executed_nodes ==
                std::vector<std::string>({"one", "fast-delay", "accumulator", "slow-gain", "slow-delay"}),
            "phased slow-rate nodes executed outside their offset boundary");
    require(std::abs(result.ticks[0].held_outputs.at("slow-gain.out") + 10.0) < 1.0e-12 &&
                std::abs(result.ticks[0].held_outputs.at("slow-delay.out") + 20.0) < 1.0e-12,
            "offset nodes did not expose deterministic pre-activation holds");
    require(std::abs(result.ticks[1].held_outputs.at("slow-gain.out") - 20.0) < 1.0e-12 &&
                std::abs(result.ticks[1].held_outputs.at("slow-delay.out") + 20.0) < 1.0e-12 &&
                std::abs(result.ticks[3].held_outputs.at("slow-delay.out") - 20.0) < 1.0e-12,
            "phased unit delay did not sample/commit on offset boundaries");
    require(result.local_fallback_count == 0 && result.maximum_connection_error == 0.0,
            "multirate coupling/fallback accounting failed");

    auto invalid_graph = graph;
    const auto slow = std::find_if(
        invalid_graph.nodes.begin(), invalid_graph.nodes.end(),
        [](const smave::BlockGraphNode& node) { return node.id == "slow-gain"; });
    slow->sample_time = 0.15;
    smave::ModelGroupRuntime invalid_runtime(invalid_graph, root);
    bool rejected = false;
    try { (void)invalid_runtime.execute_multirate(0.4, 0.1); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "non-integral multirate sample ratio was silently rounded");

    auto invalid_offset = graph;
    const auto phased = std::find_if(
        invalid_offset.nodes.begin(), invalid_offset.nodes.end(),
        [](const smave::BlockGraphNode& node) { return node.id == "slow-gain"; });
    phased->sample_offset = 0.15;
    bool offset_rejected = false;
    try { (void)smave::ModelGroupRuntime(invalid_offset, root).execute_multirate(0.4, 0.1); }
    catch (const std::invalid_argument&) { offset_rejected = true; }
    require(offset_rejected, "non-integral sample offset was silently rounded");
}

void test_coupled_continuous_sampled_scheduler(const std::filesystem::path& root) {
    const auto source = root / "CoupledScheduler.mo";
    std::ofstream(source)
        << "model CoupledScheduler\n"
        << "parameter Real u = 1;\n"
        << "Real x(start=0);\n"
        << "equation\nder(x)=u;\nend CoupledScheduler;\n";
    const auto continuous = smave::compile_continuous_model(source);
    const auto hybrid_path = root / "coupled.hybrid";
    std::ofstream(hybrid_path)
        << "SMAVE_HYBRID 1\nMODEL \"controller\"\nSAMPLE_TIME 0.5\n"
        << "INITIAL_MODE \"up\"\nSTATE 1 \"u\" 1\nMODES 2\n"
        << "MODE \"up\" 1 \"u\" \"u\"\n"
        << "MODE \"down\" 1 \"u\" \"u\"\nTRANSITIONS 2\n"
        << "TRANSITION \"down\" \"up\" \"down\" 1 \"x-0.75\" 1 \"u\" \"-1\"\n"
        << "TRANSITION \"up\" \"down\" \"up\" 1 \"0.25-x\" 1 \"u\" \"1\"\nEND\n";
    const auto sampled = smave::HybridProgramIR::read(hybrid_path);
    const auto result = smave::simulate_coupled(continuous, sampled, 2.5, 0.1);
    require(result.success && result.samples.size() == 6 &&
                result.sampled_events.size() == 2,
            "coupled continuous/sample scheduler failed");
    require(result.sampled_events[0].tick == 2 &&
                result.sampled_events[1].tick == 4,
            "sampled transitions did not observe continuous state at boundaries");
    require(result.samples.front().tick == 0 &&
                std::abs(result.samples.front().continuous_state.at("x")) < 1.0e-12 &&
                std::abs(result.samples[2].continuous_state.at("x") - 1.0) < 1.0e-10 &&
                result.samples[2].post_discrete_state.at("u") == -1.0 &&
                std::abs(result.final_continuous_state.at("x") - 0.5) < 1.0e-10,
            "zero-order hold or atomic sampled reset semantics failed");

    const auto offset_hybrid_path = root / "coupled-offset.hybrid";
    std::ofstream(offset_hybrid_path)
        << "SMAVE_HYBRID 2\nMODEL \"offset-controller\"\n"
        << "SAMPLE_TIME 0.5\nSAMPLE_OFFSET 0.25\n"
        << "INITIAL_MODE \"up\"\nSTATE 1 \"u\" 1\nMODES 2\n"
        << "MODE \"up\" 1 \"u\" \"u\"\nMODE \"down\" 1 \"u\" \"u\"\n"
        << "TRANSITIONS 1\n"
        << "TRANSITION \"down\" \"up\" \"down\" 1 \"x-0.2\" 1 \"u\" \"-1\"\n"
        << "END\n";
    const auto offset_sampled = smave::HybridProgramIR::read(offset_hybrid_path);
    const auto offset_result = smave::simulate_coupled(
        continuous, offset_sampled, 1.0, 0.05, {}, {{0, "down", "up"}});
    require(offset_result.success && offset_result.samples.size() == 2 &&
                offset_result.samples.front().tick == 0 &&
                std::abs(offset_result.samples.front().time - 0.25) < 1.0e-12 &&
                std::abs(offset_result.samples.front().continuous_state.at("x") - 0.25) <
                    1.0e-10 &&
                offset_result.sampled_events.size() == 1 &&
                offset_result.sampled_events.front().candidate_accepted &&
                std::abs(offset_result.sampled_events.front().time - 0.25) < 1.0e-12 &&
                offset_result.final_mode == "down" &&
                offset_result.final_discrete_state.at("u") == -1.0 &&
                std::abs(offset_result.final_continuous_state.at("x") + 0.5) < 1.0e-10,
            "coupled scheduler did not preserve the hold before an offset first sample");

    const auto boundary_source = root / "CoupledBoundary.mo";
    std::ofstream(boundary_source)
        << "model CoupledBoundary\nparameter Real u=1;\nReal x(start=0);\n"
        << "equation\nder(x)=u;\nwhen x >= 1 then\n"
        << "reinit(x,0);\nend when;\nend CoupledBoundary;\n";
    const auto boundary_continuous = smave::compile_continuous_model(boundary_source);
    const auto boundary_hybrid_path = root / "coupled-boundary.hybrid";
    std::ofstream(boundary_hybrid_path)
        << "SMAVE_HYBRID 1\nMODEL \"boundary-controller\"\nSAMPLE_TIME 1\n"
        << "INITIAL_MODE \"waiting\"\nSTATE 1 \"u\" 1\nMODES 2\n"
        << "MODE \"waiting\" 1 \"u\" \"u\"\nMODE \"fired\" 0\nTRANSITIONS 1\n"
        << "TRANSITION \"sample-fire\" \"waiting\" \"fired\" 1 \"x-0.5\" 0\nEND\n";
    const auto boundary_sampled = smave::HybridProgramIR::read(boundary_hybrid_path);
    const auto boundary_result = smave::simulate_coupled(
        boundary_continuous, boundary_sampled, 1.0, 0.2);
    require(boundary_result.success && boundary_result.continuous_events.size() == 1 &&
                boundary_result.sampled_events.empty() &&
                std::abs(boundary_result.samples.back().continuous_state.at("x")) < 1.0e-10,
            "sampled guard did not observe the post-continuous-event state");

    auto failing_boundary_sampled = boundary_sampled;
    failing_boundary_sampled.transitions.front().guard_expression = "tick-x-0.5";
    failing_boundary_sampled.transitions.front().resets.push_back({"u", "0/0"});
    const auto failing_boundary_result = smave::simulate_coupled(
        boundary_continuous, failing_boundary_sampled, 1.0, 0.2);
    require(!failing_boundary_result.success &&
                failing_boundary_result.message.find("sampled reset produced NaN/Inf") !=
                    std::string::npos &&
                failing_boundary_result.continuous_events.empty() &&
                failing_boundary_result.sampled_events.empty() &&
                failing_boundary_result.samples.size() == 1 &&
                std::abs(failing_boundary_result.final_continuous_state.at("x") - 1.0) <
                    1.0e-8 &&
                failing_boundary_result.final_mode == "waiting" &&
                failing_boundary_result.final_discrete_state.at("u") == 1.0,
            "failed sampled boundary did not roll back its coincident continuous root");

    const auto initial_source = root / "CoupledInitialEvent.mo";
    std::ofstream(initial_source)
        << "model CoupledInitialEvent\nparameter Real u=1;\n"
        << "Real x(start=1);\nReal y(start=0);\n"
        << "equation\nder(x)=0;\nder(y)=0;\n"
        << "when x >= 1 then\nreinit(y,2);\nend when;\n"
        << "end CoupledInitialEvent;\n";
    const auto initial_continuous = smave::compile_continuous_model(initial_source);
    const auto initial_hybrid_path = root / "coupled-initial.hybrid";
    std::ofstream(initial_hybrid_path)
        << "SMAVE_HYBRID 1\nMODEL \"initial-controller\"\nSAMPLE_TIME 0.5\n"
        << "INITIAL_MODE \"waiting\"\nSTATE 1 \"u\" 1\nMODES 2\n"
        << "MODE \"waiting\" 1 \"u\" \"u\"\nMODE \"fired\" 0\nTRANSITIONS 1\n"
        << "TRANSITION \"initial-fire\" \"waiting\" \"fired\" 1 \"y-1\" 1 \"u\" \"-1\"\nEND\n";
    const auto initial_sampled = smave::HybridProgramIR::read(initial_hybrid_path);
    const auto initial_result = smave::simulate_coupled(
        initial_continuous, initial_sampled, 0.5, 0.1);
    require(initial_result.success && initial_result.continuous_events.size() == 1 &&
                initial_result.sampled_events.size() == 1 &&
                initial_result.continuous_events.front().time == 0.0 &&
                initial_result.sampled_events.front().tick == 0 &&
                initial_result.sampled_events.front().time == 0.0 &&
                initial_result.samples.front().post_mode == "fired" &&
                initial_result.samples.front().continuous_state.at("y") == 2.0 &&
                initial_result.samples.front().post_discrete_state.at("u") == -1.0 &&
                initial_result.continuous_events.size() == 1,
            "coupled initialization did not order continuous events before tick-zero sampling");

    auto failing_initial_sampled = initial_sampled;
    failing_initial_sampled.transitions.front().resets.front().expression = "0/0";
    const auto failing_initial_result = smave::simulate_coupled(
        initial_continuous, failing_initial_sampled, 0.5, 0.1);
    require(!failing_initial_result.success &&
                failing_initial_result.continuous_events.empty() &&
                failing_initial_result.sampled_events.empty() &&
                failing_initial_result.samples.empty() &&
                failing_initial_result.final_continuous_state.at("y") == 0.0 &&
                failing_initial_result.final_discrete_state.at("u") == 1.0 &&
                failing_initial_result.final_mode == "waiting",
            "failed tick-zero sampled transaction committed continuous initialization");

    const auto superdense_source = root / "CoupledSuperdense.mo";
    std::ofstream(superdense_source)
        << "model CoupledSuperdense\nparameter Real u=0;\n"
        << "Real x(start=0);\nReal y(start=0);\n"
        << "equation\nder(x)=0;\nder(y)=0;\n"
        << "when u >= 1 then\nreinit(x,pre(x)+1);\nend when;\n"
        << "when x >= 1 then\nreinit(y,pre(y)+2);\nend when;\n"
        << "end CoupledSuperdense;\n";
    const auto superdense_continuous = smave::compile_continuous_model(superdense_source);
    const auto superdense_hybrid_path = root / "coupled-superdense.hybrid";
    std::ofstream(superdense_hybrid_path)
        << "SMAVE_HYBRID 1\nMODEL \"superdense-controller\"\nSAMPLE_TIME 1\n"
        << "INITIAL_MODE \"waiting\"\nSTATE 1 \"u\" 0\nMODES 3\n"
        << "MODE \"waiting\" 1 \"u\" \"u\"\n"
        << "MODE \"armed\" 1 \"u\" \"u\"\nMODE \"done\" 0\nTRANSITIONS 2\n"
        << "TRANSITION \"arm\" \"waiting\" \"armed\" 2 \"tick-0.5\" 1 \"u\" \"1\"\n"
        << "TRANSITION \"finish\" \"armed\" \"done\" 1 \"y-1\" 1 \"u\" \"-1\"\nEND\n";
    const auto superdense_sampled = smave::HybridProgramIR::read(superdense_hybrid_path);
    const auto superdense_result = smave::simulate_coupled(
        superdense_continuous, superdense_sampled, 1.0, 0.2);
    require(superdense_result.success && superdense_result.sampled_events.size() == 2 &&
                superdense_result.continuous_events.size() == 2 &&
                superdense_result.sampled_events[0].transition_id == "arm" &&
                superdense_result.continuous_events[0].id == "event-1" &&
                superdense_result.continuous_events[1].id == "event-2" &&
                superdense_result.sampled_events[1].transition_id == "finish" &&
                superdense_result.sampled_events[0].time == 1.0 &&
                superdense_result.continuous_events[0].time == 1.0 &&
                superdense_result.sampled_events[1].time == 1.0 &&
                superdense_result.final_mode == "done" &&
                superdense_result.final_discrete_state.at("u") == -1.0 &&
                superdense_result.final_continuous_state.at("x") == 1.0 &&
                superdense_result.final_continuous_state.at("y") == 2.0 &&
                superdense_result.superdense_microsteps == 4 &&
                superdense_result.maximum_superdense_iterations == 2 &&
                superdense_result.continuous_events[0].pre_state.at("x") == 0.0 &&
                superdense_result.continuous_events[1].pre_state.at("y") == 0.0,
            "cross-domain sampled/continuous superdense fixed point was not completed");

    auto failing_superdense = superdense_sampled;
    failing_superdense.transitions[1].resets.front().expression = "0/0";
    const auto failing_superdense_result = smave::simulate_coupled(
        superdense_continuous, failing_superdense, 1.0, 0.2);
    require(!failing_superdense_result.success &&
                failing_superdense_result.message.find("sampled reset produced NaN/Inf") !=
                    std::string::npos &&
                failing_superdense_result.sampled_events.empty() &&
                failing_superdense_result.continuous_events.empty() &&
                failing_superdense_result.samples.size() == 1 &&
                failing_superdense_result.final_mode == "waiting" &&
                failing_superdense_result.final_discrete_state.at("u") == 0.0 &&
                failing_superdense_result.final_continuous_state.at("x") == 0.0 &&
                failing_superdense_result.final_continuous_state.at("y") == 0.0,
            "failed cross-domain microstep partially committed its sample boundary");

    auto invalid = sampled;
    invalid.initial_state["gain"] = 1.0;
    const auto invalid_result = smave::simulate_coupled(continuous, invalid, 1.0, 0.1);
    require(!invalid_result.success &&
                invalid_result.message.find("same-name continuous parameter") != std::string::npos,
            "uncoupled sampled state was silently ignored");
}

void test_dae_multigrid_contract(const std::filesystem::path& root) {
    const auto source = root / "DaeMultigrid.mo";
    std::ofstream(source)
        << "model DaeMultigrid\n"
        << "Real x1(start=1); Real x2(start=2);\n"
        << "Real y1(start=-0.1); Real y2(start=-0.2);\n"
        << "equation\n"
        << "der(x1)=-x1-y1; der(x2)=-x2-y2;\n"
        << "0.1*x1+y1=0; 0.1*x2+y2=0;\n"
        << "end DaeMultigrid;\n";
    const auto model = smave::compile_index_one_dae(source);
    const auto scenarios = root / "dae-multigrid-scenarios";
    std::filesystem::create_directories(scenarios);
    for (int index = 0; index < 2; ++index) {
        std::ofstream(scenarios / ("case-" + std::to_string(index) + ".conf"))
            << "step=0.1\ntime=" << 0.1 * (index + 1) << '\n'
            << "previous.x1=1\nprevious.x2=2\n"
            << "state.x1=0.9174311926605504\n"
            << "state.x2=1.8348623853211008\n"
            << "algebraic.y1=-0.09174311926605504\n"
            << "algebraic.y2=-0.18348623853211008\n";
    }
    const auto artifact = smave::train_dae_multigrid(model, scenarios);
    const auto artifact_path = root / "dae-multigrid.artifact";
    artifact.write(artifact_path);
    const auto restored = smave::DaeMultigridArtifact::read(artifact_path);
    require(restored.artifact_hash == artifact.artifact_hash &&
                restored.unknown_count == 4 && restored.training_samples == 2,
            "DAE multigrid artifact roundtrip lost its binding");
    auto lineage = restored;
    lineage.schema_version = "smave.dae-multigrid.v2";
    lineage.training_dataset_id = "dae-multigrid-training";
    lineage.training_dataset_version = "dataset-version";
    lineage.training_dataset_manifest_hash = "dataset-manifest-hash";
    lineage.hierarchy.schema_version = "smave.learned-multigrid.v3";
    lineage.hierarchy.training_dataset_id = lineage.training_dataset_id;
    lineage.hierarchy.training_dataset_version = lineage.training_dataset_version;
    lineage.hierarchy.training_dataset_manifest_hash =
        lineage.training_dataset_manifest_hash;
    lineage.hierarchy.expert_version.clear();
    lineage.hierarchy.seal();
    lineage.seal();
    const auto lineage_path = root / "dae-multigrid-lineage.artifact";
    lineage.write(lineage_path);
    const auto restored_lineage = smave::DaeMultigridArtifact::read(lineage_path);
    require(restored_lineage.schema_version == "smave.dae-multigrid.v2" &&
                restored_lineage.hierarchy.schema_version ==
                    "smave.learned-multigrid.v3" &&
                restored_lineage.training_dataset_version == "dataset-version" &&
                restored_lineage.hierarchy.training_dataset_version ==
                    restored_lineage.training_dataset_version,
            "DAE multigrid v2 lineage roundtrip lost its dataset binding");
    auto wrapper_mismatch = restored_lineage;
    wrapper_mismatch.training_dataset_version = "different-wrapper-version";
    wrapper_mismatch.seal();
    bool rejected_wrapper_mismatch = false;
    try {
        wrapper_mismatch.validate();
    } catch (const std::invalid_argument&) {
        rejected_wrapper_mismatch = true;
    }
    require(rejected_wrapper_mismatch,
            "DAE wrapper lineage mismatch was accepted after resealing");
    auto hierarchy_mismatch = restored_lineage;
    hierarchy_mismatch.hierarchy.training_dataset_version =
        "different-hierarchy-version";
    hierarchy_mismatch.hierarchy.expert_version.clear();
    hierarchy_mismatch.hierarchy.seal();
    hierarchy_mismatch.seal();
    bool rejected_hierarchy_mismatch = false;
    try {
        hierarchy_mismatch.validate();
    } catch (const std::invalid_argument&) {
        rejected_hierarchy_mismatch = true;
    }
    require(rejected_hierarchy_mismatch,
            "DAE hierarchy lineage mismatch was accepted after resealing");
    const auto accelerated = smave::simulate_index_one_dae(
        model, 0.2, 0.1, {}, &restored);
    require(accelerated.success && accelerated.learned_preconditioned_steps == 2 &&
                accelerated.learned_rejections == 0 &&
                accelerated.dense_step_fallbacks == 0 &&
                accelerated.learned_krylov_iterations > 0,
            "DAE candidate steps did not use learned multigrid PCG");
    auto mismatched = restored;
    mismatched.model_source_hash = "different-source";
    mismatched.hierarchy.model_source_hash = "different-source";
    mismatched.hierarchy.seal();
    mismatched.seal();
    const auto fallback = smave::simulate_index_one_dae(
        model, 0.1, 0.1, {}, &mismatched);
    require(fallback.success && fallback.learned_preconditioned_steps == 0 &&
                fallback.learned_rejections == 1 &&
                fallback.dense_step_fallbacks == 1,
            "DAE source mismatch did not reject acceleration and retry dense Newton");
    auto corrupted = restored;
    corrupted.maximum_step = 0.2;
    const auto corrupted_fallback = smave::simulate_index_one_dae(
        model, 0.1, 0.1, {}, &corrupted);
    require(corrupted_fallback.success &&
                corrupted_fallback.learned_preconditioned_steps == 0 &&
                corrupted_fallback.learned_rejections == 1 &&
                corrupted_fallback.dense_step_fallbacks == 1,
            "invalid in-memory DAE artifact did not fail closed to dense Newton");
}

void test_index_one_dae_frontend_and_runtime(const std::filesystem::path& root) {
    const auto source = root / "IndexOne.mo";
    std::ofstream(source)
        << "model IndexOne\n"
        << "parameter Real k = 2;\n"
        << "Real x(start=0, nominal=1);\n"
        << "Real y(start=0, nominal=1);\n"
        << "initial equation\nx=1;\n"
        << "equation\nder(x)=-y;\ny-k*x=0;\nend IndexOne;\n";
    const auto model = smave::compile_index_one_dae(source, "IndexOne");
    require(model.states.size() == 1 && model.states.front().name == "x" &&
                model.algebraics.size() == 1 && model.algebraics.front().name == "y" &&
                model.constraints.size() == 1 &&
                model.initial_constraints.size() == 1,
            "index-1 DAE frontend lost state/algebraic structure");
    const auto ir = root / "index-one.ir";
    model.write(ir);
    const auto restored = smave::IndexOneDaeIR::read(ir);
    const auto result = smave::simulate_index_one_dae(restored, 1.0, 0.1);
    const double expected_x = std::pow(1.0 / 1.2, 10.0);
    require(result.success && result.steps.size() == 10 &&
                result.initialization_residual_inf < 1.0e-8 &&
                result.algebraic_rank_checks == 11 &&
                result.minimum_algebraic_rank_margin > 0.9 &&
                std::abs(result.initial_state.at("x") - 1.0) < 1.0e-8 &&
                std::abs(result.initial_algebraics.at("y") - 2.0) < 1.0e-8 &&
                result.maximum_residual_inf < 1.0e-8 &&
                std::abs(result.final_state.at("x") - expected_x) < 1.0e-8 &&
                std::abs(result.final_algebraics.at("y") - 2.0 * expected_x) < 1.0e-8,
            "index-1 implicit Euler/Newton solve violated the DAE residual or reference solution");

    const auto no_initial = root / "DaeWithoutInitialEquation.mo";
    std::ofstream(no_initial)
        << "model DaeWithoutInitialEquation\nReal x(start=1);\nReal y(start=0);\n"
        << "equation\nder(x)=-y;\ny-2*x=0;\nend DaeWithoutInitialEquation;\n";
    const auto no_initial_model = smave::compile_index_one_dae(no_initial);
    const auto no_initial_result = smave::simulate_index_one_dae(
        no_initial_model, 0.1, 0.1);
    require(no_initial_result.success &&
                std::abs(no_initial_result.initial_state.at("x") - 1.0) < 1.0e-12 &&
                std::abs(no_initial_result.initial_algebraics.at("y") - 2.0) < 1.0e-8,
            "default DAE initialization did not hold state start and solve algebraic consistency");

    const auto bad_initial = root / "BadInitialDae.mo";
    std::ofstream(bad_initial)
        << "model BadInitialDae\nReal x(start=0);\nReal y;\n"
        << "initial equation\nx^2+1=0;\n"
        << "equation\nder(x)=-y;\ny-x=0;\nend BadInitialDae;\n";
    const auto bad_initial_model = smave::compile_index_one_dae(bad_initial);
    const auto bad_initial_result = smave::simulate_index_one_dae(
        bad_initial_model, 0.1, 0.1);
    require(!bad_initial_result.success && bad_initial_result.steps.empty() &&
                bad_initial_result.final_time == 0.0 &&
                bad_initial_result.message.find("initialization") != std::string::npos,
            "failed DAE consistent initialization advanced simulation time");

    const auto singular = root / "SingularDae.mo";
    std::ofstream(singular)
        << "model SingularDae\nReal x(start=1);\nReal y;\n"
        << "equation\nder(x)=-x;\nx=0;\nend SingularDae;\n";
    bool singular_rejected = false;
    try { (void)smave::compile_index_one_dae(singular); }
    catch (const std::invalid_argument&) { singular_rejected = true; }
    require(singular_rejected,
            "DAE without algebraic structural matching was misclassified as index-1");

    const auto numerically_singular = root / "NumericallySingularDae.mo";
    std::ofstream(numerically_singular)
        << "model NumericallySingularDae\nReal x(start=0);\nReal y(start=0);\n"
        << "equation\nder(x)=0;\ny*y=0;\nend NumericallySingularDae;\n";
    const auto numerically_singular_model =
        smave::compile_index_one_dae(numerically_singular);
    const auto numerically_singular_result = smave::simulate_index_one_dae(
        numerically_singular_model, 0.1, 0.1);
    require(!numerically_singular_result.success &&
                numerically_singular_result.steps.empty() &&
                numerically_singular_result.final_time == 0.0 &&
                numerically_singular_result.algebraic_rank_checks == 1 &&
                numerically_singular_result.minimum_algebraic_rank_margin == 0.0 &&
                numerically_singular_result.message.find("numerical rank gate") !=
                    std::string::npos,
            "residual-zero initialization bypassed the algebraic rank gate");

    const auto runtime_rank_loss = root / "RuntimeRankLossDae.mo";
    std::ofstream(runtime_rank_loss)
        << "model RuntimeRankLossDae\nReal x(start=0);\nReal y(start=0);\n"
        << "equation\nder(x)=0;\n(0.2-time)*y=0;\nend RuntimeRankLossDae;\n";
    const auto runtime_rank_loss_model = smave::compile_index_one_dae(runtime_rank_loss);
    const auto runtime_rank_loss_result = smave::simulate_index_one_dae(
        runtime_rank_loss_model, 0.3, 0.1);
    require(!runtime_rank_loss_result.success &&
                runtime_rank_loss_result.steps.size() == 1 &&
                std::abs(runtime_rank_loss_result.final_time - 0.1) < 1.0e-12 &&
                runtime_rank_loss_result.algebraic_rank_checks == 3 &&
                runtime_rank_loss_result.message.find("numerical rank gate") !=
                    std::string::npos,
            "runtime algebraic rank loss was committed as an index-1 step");

    const auto event = root / "EventDae.mo";
    std::ofstream(event)
        << "model EventDae\nReal x(start=0);\nReal y;\nequation\n"
        << "der(x)=y;\ny=1;\nwhen x > 1 then\nreinit(x,0);\nend when;\n"
        << "end EventDae;\n";
    const auto runtime_event_model = smave::compile_index_one_dae(event);
    const auto runtime_event_result = smave::simulate_index_one_dae(
        runtime_event_model, 1.1, 0.1);
    require(runtime_event_result.success && runtime_event_result.events.size() == 1 &&
                std::abs(runtime_event_result.events.front().time - 1.0) < 1.0e-8 &&
                std::abs(runtime_event_result.events.front().post_state.at("x")) < 1.0e-12 &&
                runtime_event_result.maximum_guard_residual < 1.0e-8 &&
                runtime_event_result.maximum_event_projection_residual_inf < 1.0e-8 &&
                std::abs(runtime_event_result.final_state.at("x") - 0.1) < 1.0e-8 &&
                std::abs(runtime_event_result.final_algebraics.at("y") - 1.0) < 1.0e-8,
            "runtime DAE event localization/reset violated the reference semantics");

    const auto grazing_event = root / "GrazingEventDae.mo";
    std::ofstream(grazing_event)
        << "model GrazingEventDae\n"
        << "Real x(start=-0.9);\nReal z(start=0);\nReal y(start=-0.9);\n"
        << "equation\nder(x)=1;\nder(z)=0;\ny-x=0;\n"
        << "when y*y <= 0 then\nreinit(z,pre(z)+1);\nend when;\n"
        << "end GrazingEventDae;\n";
    const auto grazing_event_model = smave::compile_index_one_dae(grazing_event);
    const auto grazing_event_result = smave::simulate_index_one_dae(
        grazing_event_model, 1.5, 0.5);
    require(grazing_event_result.success &&
                grazing_event_result.events.size() == 1 &&
                grazing_event_result.grazing_events == 1 &&
                grazing_event_result.events.front().grazing &&
                std::abs(grazing_event_result.events.front().time - 0.9) < 1.0e-8 &&
                std::abs(grazing_event_result.events.front().post_state.at("z") - 1.0) <
                    1.0e-12 &&
                std::abs(grazing_event_result.events.front().post_algebraics.at("y")) <
                    1.0e-8 &&
                grazing_event_result.maximum_guard_residual < 1.0e-8 &&
                grazing_event_result.maximum_event_projection_residual_inf < 1.0e-8 &&
                grazing_event_result.minimum_algebraic_rank_margin > 0.9,
            "DAE grazing root did not preserve reset and algebraic projection gates");

    const auto near_grazing_event = root / "NearGrazingEventDae.mo";
    std::ofstream(near_grazing_event)
        << "model NearGrazingEventDae\n"
        << "Real x(start=-0.9);\nReal z(start=0);\nReal y(start=-0.9);\n"
        << "equation\nder(x)=1;\nder(z)=0;\ny-x=0;\n"
        << "when y*y + 0.01 <= 0 then\nreinit(z,pre(z)+1);\nend when;\n"
        << "end NearGrazingEventDae;\n";
    const auto near_grazing_model = smave::compile_index_one_dae(near_grazing_event);
    const auto near_grazing_result = smave::simulate_index_one_dae(
        near_grazing_model, 1.5, 0.5);
    require(near_grazing_result.success && near_grazing_result.events.empty() &&
                near_grazing_result.grazing_events == 0 &&
                std::abs(near_grazing_result.final_state.at("z")) < 1.0e-12,
            "DAE grazing search accepted an internal extremum that did not reach zero");

    const auto runtime_cascade = root / "RuntimeCascadeDae.mo";
    std::ofstream(runtime_cascade)
        << "model RuntimeCascadeDae\n"
        << "Real x(start=0);\nReal z(start=0);\nReal y(start=0);\n"
        << "equation\nder(x)=1;\nder(z)=0;\ny-x-z=0;\n"
        << "when x >= 1 then\nreinit(z,pre(z)+1);\nend when;\n"
        << "when y >= 2 then\nreinit(x,0);\nend when;\n"
        << "end RuntimeCascadeDae;\n";
    const auto runtime_cascade_model = smave::compile_index_one_dae(runtime_cascade);
    const auto runtime_cascade_result = smave::simulate_index_one_dae(
        runtime_cascade_model, 1.1, 0.1);
    require(runtime_cascade_result.success && runtime_cascade_result.events.size() == 2 &&
                runtime_cascade_result.events[0].id == "event-1" &&
                runtime_cascade_result.events[1].id == "event-2" &&
                std::abs(runtime_cascade_result.events[0].time - 1.0) < 1.0e-8 &&
                std::abs(runtime_cascade_result.events[1].time - 1.0) < 1.0e-8 &&
                std::abs(runtime_cascade_result.events[0].post_algebraics.at("y") - 2.0) <
                    1.0e-8 &&
                std::abs(runtime_cascade_result.events[1].post_state.at("x")) < 1.0e-12 &&
                std::abs(runtime_cascade_result.final_state.at("x") - 0.1) < 1.0e-8 &&
                std::abs(runtime_cascade_result.final_algebraics.at("y") - 1.1) < 1.0e-8,
            "runtime DAE event cascade did not reach the projected fixed point");

    const auto initial_event = root / "InitialEventDae.mo";
    std::ofstream(initial_event)
        << "model InitialEventDae\n"
        << "Real x(start=0);\nReal z(start=0);\nReal y(start=0);\n"
        << "initial equation\nx=1;\nz=0;\n"
        << "equation\nder(x)=0;\nder(z)=0;\ny-x-z=0;\n"
        << "when y >= 1 then\nreinit(z,pre(z)+1);\nend when;\n"
        << "when y >= 2 then\nreinit(x,pre(x)+1);\nend when;\n"
        << "end InitialEventDae;\n";
    const auto initial_event_model = smave::compile_index_one_dae(initial_event);
    require(initial_event_model.events.size() == 2 &&
                initial_event_model.events[0].resets[0].expression.find(
                    "__smave_pre_z") != std::string::npos,
            "DAE frontend lost initial event/pre semantics");
    const auto initial_event_result = smave::simulate_index_one_dae(
        initial_event_model, 0.1, 0.1);
    require(initial_event_result.success &&
                initial_event_result.initial_events.size() == 2 &&
                initial_event_result.initial_event_projection_residual_inf < 1.0e-8 &&
                std::abs(initial_event_result.initial_state.at("x") - 2.0) < 1.0e-8 &&
                std::abs(initial_event_result.initial_state.at("z") - 1.0) < 1.0e-8 &&
                std::abs(initial_event_result.initial_algebraics.at("y") - 3.0) < 1.0e-8,
            "DAE initial event cascade did not project to the constraint manifold");

    const auto conflict_event = root / "ConflictingInitialEventDae.mo";
    std::ofstream(conflict_event)
        << "model ConflictingInitialEventDae\nReal x(start=1);\nReal y(start=0);\n"
        << "equation\nder(x)=0;\ny-x=0;\n"
        << "when y >= 1 then\nreinit(x,2);\nend when;\n"
        << "when y >= 1 then\nreinit(x,3);\nend when;\n"
        << "end ConflictingInitialEventDae;\n";
    const auto conflict_event_model = smave::compile_index_one_dae(conflict_event);
    const auto conflict_event_result = smave::simulate_index_one_dae(
        conflict_event_model, 0.1, 0.1);
    require(!conflict_event_result.success &&
                conflict_event_result.initial_events.empty() &&
                conflict_event_result.message.find("conflicting reset") !=
                    std::string::npos &&
                conflict_event_result.final_time == 0.0,
            "conflicting DAE initial events were partially committed");
}

}  // namespace

int main() {
    try {
        const auto root = temporary_directory();
        if (const auto* filter = std::getenv("SMAVE_TEST_FILTER");
            filter != nullptr && std::string_view(filter) == "latent-operator") {
            test_latent_operator_permissions_and_gate(root);
            test_latent_operator_smooth_nonlinear_capability(root);
            std::cout << "latent operator C++ test passed\n";
            return 0;
        }
        if (const auto* filter = std::getenv("SMAVE_TEST_FILTER");
            filter != nullptr && std::string_view(filter) == "competition-router") {
            test_expert_competition_and_calibration(root);
            test_heldout_family_router_evidence(root);
            std::cout << "competition and Router C++ tests passed\n";
            return 0;
        }
        if (const auto* filter = std::getenv("SMAVE_TEST_FILTER");
            filter != nullptr && std::string_view(filter) == "cascade-ordering") {
            test_registry_bundle_and_routing(root);
            std::cout << "cascade ordering C++ test passed\n";
            return 0;
        }
        if (const auto* filter = std::getenv("SMAVE_TEST_FILTER");
            filter != nullptr && std::string_view(filter) == "release-store-recovery") {
            test_signed_release_lifecycle(root);
            std::cout << "release store recovery C++ test passed\n";
            return 0;
        }
        test_expression();
        test_fmi_blackbox_import(root);
        test_fmi2_native_co_simulation(root);
        test_fmi2_co_simulation_discard_event(root);
        test_fmi2_co_simulation_pending(root);
        test_fmi2_native_model_exchange(root);
        test_compile_roundtrip_and_runtime(root);
        test_expert_residency_and_runtime_fallback(root);
        test_dataset_registry_integrity(root);
        test_event_rejection(root);
        test_linear_direct_and_fallback(root);
        test_registry_bundle_and_routing(root);
        test_equation_assessment_and_backend_routing(root);
        test_strict_configuration(root);
        test_validation_accounting(root);
        test_affine_learning_and_ood(root);
        test_krylov_and_direct_cascade(root);
        test_learned_linear_preconditioner(root);
        test_learned_multigrid(root);
        test_nonlinear_jacobian_multigrid(root);
        test_nonsymmetric_gmres_ilut(root);
        test_structural_row_alignment_for_spd(root);
        test_tensor_bucket_and_local_fallback(root);
        test_cegis_verified_cells_and_certificate_binding(root);
        test_equation_embedding_family_retrieval(root);
        test_expert_competition_and_calibration(root);
        test_heldout_family_router_evidence(root);
        test_latent_operator_permissions_and_gate(root);
        test_latent_operator_smooth_nonlinear_capability(root);
        test_block_graph_import_and_model_group(root);
        test_restricted_hybrid_event_gate(root);
        test_signed_release_lifecycle(root);
        test_continuous_modelica_zero_crossing(root);
        test_multirate_model_group_zero_order_hold(root);
        test_coupled_continuous_sampled_scheduler(root);
        test_dae_multigrid_contract(root);
        test_index_one_dae_frontend_and_runtime(root);
        std::cout << "all C++ tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return 1;
    }
}
