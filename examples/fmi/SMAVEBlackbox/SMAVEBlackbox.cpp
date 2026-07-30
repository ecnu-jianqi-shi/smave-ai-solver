#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

extern "C" {

enum fmi3Status { fmi3OK, fmi3Warning, fmi3Discard, fmi3Error, fmi3Fatal };
enum fmi3IntervalQualifier {
    fmi3IntervalNotYetKnown,
    fmi3IntervalUnchanged,
    fmi3IntervalChanged
};
using fmi3Instance = void*;
using fmi3InstanceEnvironment = void*;
using fmi3FMUState = void*;
using fmi3ValueReference = std::uint32_t;
using fmi3Float32 = float;
using fmi3Float64 = double;
using fmi3Int8 = std::int8_t;
using fmi3UInt8 = std::uint8_t;
using fmi3Int16 = std::int16_t;
using fmi3UInt16 = std::uint16_t;
using fmi3Int32 = std::int32_t;
using fmi3UInt32 = std::uint32_t;
using fmi3Int64 = std::int64_t;
using fmi3UInt64 = std::uint64_t;
using fmi3Boolean = bool;
using fmi3Clock = bool;
using fmi3String = const char*;
using fmi3Byte = std::uint8_t;
using fmi3LogMessageCallback = void (*)(fmi3InstanceEnvironment, fmi3Status, fmi3String, fmi3String);
using fmi3IntermediateUpdateCallback = void (*)(fmi3InstanceEnvironment, fmi3Float64, fmi3Boolean, fmi3Boolean, fmi3Boolean, fmi3Boolean, fmi3Boolean*, fmi3Float64*);
using fmi3ClockUpdateCallback = void (*)(fmi3InstanceEnvironment);
using fmi3LockPreemptionCallback = void (*)();
using fmi3UnlockPreemptionCallback = void (*)();

struct Instance {
    double time{0.0};
    double gain{2.0};
    double input{1.0};
    bool event_enabled{false};
    bool event_triggered{false};
    bool event_pending{false};
    bool early_return_enabled{false};
    bool force_undeclared_early_return{false};
    bool time_event_enabled{false};
    bool time_event_scheduled{false};
    bool time_event_triggered{false};
    bool invalid_time_event{false};
    bool model_exchange_instance{false};
    bool model_exchange_event_enabled{false};
    bool model_exchange_event_triggered{false};
    bool model_exchange_multi_event{false};
    bool model_exchange_grazing{false};
    bool model_exchange_near_grazing{false};
    bool model_exchange_nominal_change{false};
    bool model_exchange_invalid_nominal{false};
    bool invalid_clock_interval{false};
    bool invalid_serialized_state{false};
    bool scheduled_execution_instance{false};
    bool invalid_preemption_callbacks{false};
    bool model_partition_activated{false};
    fmi3ValueReference last_partition_reference{0};
    double continuous_state_nominal{1.0};
    bool model_exchange_time_event{false};
    bool model_exchange_initial_update{true};
    unsigned model_exchange_event_count{0};
    unsigned event_iteration{0};
    fmi3Int32 int32_input{0};
    fmi3Float32 float32_input{0.0F};
    std::array<fmi3Int32, 3> int32_array_input{};
    std::array<fmi3Float32, 3> float32_array_input{};
    std::array<fmi3Boolean, 3> boolean_array_input{};
    fmi3Int8 int8_input{0};
    fmi3UInt8 uint8_input{0};
    fmi3Int16 int16_input{0};
    fmi3UInt16 uint16_input{0};
    fmi3UInt32 uint32_input{0};
    fmi3Int64 int64_input{0};
    fmi3UInt64 uint64_input{0};
    fmi3Int64 enumeration_input{0};
    std::string string_input;
    std::string string_output;
    std::array<std::string, 3> string_array_input;
    std::array<std::string, 3> string_array_output;
    std::vector<fmi3Byte> binary_input;
    std::vector<fmi3Byte> binary_output;
    std::array<std::vector<fmi3Byte>, 3> binary_array_input;
    std::array<std::vector<fmi3Byte>, 3> binary_array_output;
    std::array<fmi3Float64, 3> array_input{};
    std::array<fmi3Float64, 3> dynamic_array_input{};
    fmi3Boolean boolean_input{false};
    fmi3Clock clock_input{false};
    fmi3InstanceEnvironment environment{};
    fmi3IntermediateUpdateCallback intermediate_update{};
    fmi3ClockUpdateCallback clock_update{};
    fmi3LockPreemptionCallback lock_preemption{};
    fmi3UnlockPreemptionCallback unlock_preemption{};
};
struct State { Instance value; };

const char* fmi3GetVersion() { return "3.0"; }
fmi3Instance fmi3InstantiateCoSimulation(
    fmi3String, fmi3String token, fmi3String, fmi3Boolean, fmi3Boolean,
    fmi3Boolean, fmi3Boolean early_return_allowed,
    const fmi3ValueReference*, std::size_t,
    fmi3InstanceEnvironment environment, fmi3LogMessageCallback,
    fmi3IntermediateUpdateCallback intermediate_update) {
    auto* instance = new (std::nothrow) Instance{};
    if (instance != nullptr && token != nullptr && std::strstr(token, "event") != nullptr) {
        instance->event_enabled = true;
    }
    if (instance != nullptr && token != nullptr && std::strstr(token, "early") != nullptr) {
        instance->early_return_enabled = early_return_allowed;
        instance->environment = environment;
        instance->intermediate_update = intermediate_update;
        instance->force_undeclared_early_return =
            std::strstr(token, "undeclared-early") != nullptr;
    }
    if (instance != nullptr && token != nullptr && std::strstr(token, "time-event") != nullptr) {
        instance->time_event_enabled = true;
        instance->event_enabled = true;
        instance->invalid_time_event = std::strstr(token, "invalid-time-event") != nullptr;
    }
    if (instance != nullptr && token != nullptr &&
        std::strstr(token, "invalid-serialized-state") != nullptr) {
        instance->invalid_serialized_state = true;
    }
    return instance;
}
fmi3Instance fmi3InstantiateScheduledExecution(
    fmi3String, fmi3String token, fmi3String, fmi3Boolean, fmi3Boolean,
    fmi3InstanceEnvironment environment, fmi3LogMessageCallback,
    fmi3ClockUpdateCallback clock_update,
    fmi3LockPreemptionCallback lock_preemption,
    fmi3UnlockPreemptionCallback unlock_preemption) {
    if (clock_update == nullptr || lock_preemption == nullptr ||
        unlock_preemption == nullptr) return nullptr;
    auto* instance = new (std::nothrow) Instance{};
    if (instance == nullptr) return nullptr;
    instance->scheduled_execution_instance = true;
    instance->environment = environment;
    instance->clock_update = clock_update;
    instance->lock_preemption = lock_preemption;
    instance->unlock_preemption = unlock_preemption;
    instance->invalid_preemption_callbacks = token != nullptr &&
        std::strstr(token, "invalid-preemption") != nullptr;
    return instance;
}
void fmi3FreeInstance(fmi3Instance instance) { delete static_cast<Instance*>(instance); }
fmi3Status fmi3EnterInitializationMode(fmi3Instance instance, fmi3Boolean, fmi3Float64, fmi3Float64 start, fmi3Boolean, fmi3Float64) { static_cast<Instance*>(instance)->time = start; return fmi3OK; }
fmi3Status fmi3ExitInitializationMode(fmi3Instance) { return fmi3OK; }
fmi3Status fmi3Terminate(fmi3Instance) { return fmi3OK; }
fmi3Status fmi3SetFloat64(fmi3Instance instance, const fmi3ValueReference refs[], std::size_t count, const fmi3Float64 values[], std::size_t value_count) {
    if (count == 2 && refs[0] == 33 && refs[1] == 30 && value_count == 6) {
        auto* state = static_cast<Instance*>(instance);
        std::copy_n(values, 3, state->dynamic_array_input.begin());
        std::copy_n(values + 3, 3, state->array_input.begin());
        return fmi3OK;
    }
    if (count == 1 && refs[0] == 30 && value_count == 3) {
        std::copy_n(values, 3, static_cast<Instance*>(instance)->array_input.begin());
        return fmi3OK;
    }
    if (count == 1 && refs[0] == 33 && value_count == 3) {
        std::copy_n(values, 3, static_cast<Instance*>(instance)->dynamic_array_input.begin());
        return fmi3OK;
    }
    if (count != value_count) return fmi3Error;
    auto* state = static_cast<Instance*>(instance);
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] == 1) state->gain = values[index];
        else if (refs[index] == 2) state->input = values[index];
        else return fmi3Error;
    }
    return fmi3OK;
}
fmi3Status fmi3GetFloat64(fmi3Instance instance, const fmi3ValueReference refs[], std::size_t count, fmi3Float64 values[], std::size_t value_count) {
    if (count == 2 && refs[0] == 31 && refs[1] == 34 && value_count == 6) {
        const auto* state = static_cast<Instance*>(instance);
        std::transform(
            state->array_input.begin(), state->array_input.end(), values,
            [](double value) { return value * 2.0; });
        std::transform(
            state->dynamic_array_input.begin(), state->dynamic_array_input.end(), values + 3,
            [](double value) { return value * 3.0; });
        return fmi3OK;
    }
    if (count == 1 && refs[0] == 31 && value_count == 3) {
        const auto& input = static_cast<Instance*>(instance)->array_input;
        std::transform(input.begin(), input.end(), values, [](double value) {
            return value * 2.0;
        });
        return fmi3OK;
    }
    if (count == 1 && refs[0] == 34 && value_count == 3) {
        const auto& input = static_cast<Instance*>(instance)->dynamic_array_input;
        std::transform(input.begin(), input.end(), values, [](double value) {
            return value * 3.0;
        });
        return fmi3OK;
    }
    if (count != value_count) return fmi3Error;
    const auto* state = static_cast<Instance*>(instance);
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] == 2) values[index] = state->input;
        else if (refs[index] == 3) values[index] = state->gain * state->input + state->time;
        else return fmi3Error;
    }
    return fmi3OK;
}
fmi3Status fmi3SetInt32(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, const fmi3Int32 values[], std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || values == nullptr) return fmi3Error;
    auto* state = static_cast<Instance*>(instance);
    if (count == 2 && refs[0] == 4 && refs[1] == 35 && value_count == 4) {
        state->int32_input = values[0];
        std::copy_n(values + 1, 3, state->int32_array_input.begin());
        return fmi3OK;
    }
    if (count != value_count) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 4) return fmi3Error;
        state->int32_input = values[index];
    }
    return fmi3OK;
}
fmi3Status fmi3GetInt32(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, fmi3Int32 values[], std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || values == nullptr) return fmi3Error;
    const auto* state = static_cast<Instance*>(instance);
    if (count == 2 && refs[0] == 6 && refs[1] == 36 && value_count == 4) {
        values[0] = state->int32_input + 1;
        std::transform(state->int32_array_input.begin(), state->int32_array_input.end(),
                       values + 1, [](fmi3Int32 value) { return value + 10; });
        return fmi3OK;
    }
    if (count == 1 && refs[0] == 36 && value_count == 3) {
        std::transform(state->int32_array_input.begin(), state->int32_array_input.end(),
                       values, [](fmi3Int32 value) { return value + 10; });
        return fmi3OK;
    }
    if (count != value_count) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 6) return fmi3Error;
        values[index] = state->int32_input + 1;
    }
    return fmi3OK;
}
fmi3Status fmi3SetBoolean(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, const fmi3Boolean values[], std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || values == nullptr) return fmi3Error;
    auto* state = static_cast<Instance*>(instance);
    if (count == 1 && refs[0] == 37 && value_count == 3) {
        std::copy_n(values, 3, state->boolean_array_input.begin());
        return fmi3OK;
    }
    if (count != value_count) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 5) return fmi3Error;
        state->boolean_input = values[index];
    }
    return fmi3OK;
}
fmi3Status fmi3GetBoolean(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, fmi3Boolean values[], std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || values == nullptr) return fmi3Error;
    const auto* state = static_cast<Instance*>(instance);
    if (count == 1 && refs[0] == 38 && value_count == 3) {
        std::transform(state->boolean_array_input.begin(), state->boolean_array_input.end(),
                       values, [](fmi3Boolean value) { return !value; });
        return fmi3OK;
    }
    if (count != value_count) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 7) return fmi3Error;
        values[index] = !state->boolean_input;
    }
    return fmi3OK;
}
fmi3Status fmi3SetClock(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, const fmi3Clock values[], std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || values == nullptr) return fmi3Error;
    auto* state = static_cast<Instance*>(instance);
    if (count != value_count) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 45) return fmi3Error;
        state->clock_input = values[index];
    }
    return fmi3OK;
}
fmi3Status fmi3GetClock(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, fmi3Clock values[], std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || values == nullptr) return fmi3Error;
    const auto* state = static_cast<Instance*>(instance);
    if (count != value_count) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 46) return fmi3Error;
        values[index] = !state->clock_input;
    }
    return fmi3OK;
}
fmi3Status fmi3GetIntervalDecimal(fmi3Instance instance,
    const fmi3ValueReference refs[], std::size_t count, fmi3Float64 intervals[],
    fmi3IntervalQualifier qualifiers[]) {
    if (instance == nullptr || refs == nullptr || intervals == nullptr ||
        qualifiers == nullptr) return fmi3Error;
    const auto* state = static_cast<Instance*>(instance);
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 46 && refs[index] != 47) return fmi3Error;
        intervals[index] = state->invalid_clock_interval ? -0.25 : 0.25;
        if (refs[index] == 47) intervals[index] = 0.5;
        qualifiers[index] = fmi3IntervalChanged;
    }
    return fmi3OK;
}
fmi3Status fmi3GetShiftDecimal(fmi3Instance instance,
    const fmi3ValueReference refs[], std::size_t count, fmi3Float64 shifts[]) {
    if (instance == nullptr || refs == nullptr || shifts == nullptr) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 46 && refs[index] != 47) return fmi3Error;
        shifts[index] = static_cast<Instance*>(instance)->scheduled_execution_instance
            ? 0.0
            : 0.05;
    }
    return fmi3OK;
}
fmi3Status fmi3ActivateModelPartition(
    fmi3Instance instance, fmi3ValueReference clock_reference,
    fmi3Float64 activation_time) {
    if (instance == nullptr || (clock_reference != 46 && clock_reference != 47) ||
        !std::isfinite(activation_time)) return fmi3Error;
    auto* state = static_cast<Instance*>(instance);
    if (!state->scheduled_execution_instance ||
        activation_time < state->time - 1.0e-12) return fmi3Error;
    if (state->model_partition_activated &&
        activation_time <= state->time + 1.0e-12 &&
        clock_reference == state->last_partition_reference) return fmi3Error;
    state->lock_preemption();
    state->time = activation_time;
    state->model_partition_activated = true;
    state->last_partition_reference = clock_reference;
    state->clock_update(state->environment);
    if (!state->invalid_preemption_callbacks) {
        state->unlock_preemption();
    }
    return fmi3OK;
}
#define SMAVE_FMI3_NUMERIC_IO(Type, Suffix, InputRef, OutputRef, Field) \
fmi3Status fmi3Set##Suffix(fmi3Instance instance, const fmi3ValueReference refs[], \
    std::size_t count, const Type values[], std::size_t value_count) { \
    if (instance == nullptr || refs == nullptr || values == nullptr || count != value_count) return fmi3Error; \
    auto* state = static_cast<Instance*>(instance); \
    for (std::size_t index = 0; index < count; ++index) { \
        if (refs[index] != InputRef) return fmi3Error; \
        state->Field = values[index]; \
    } \
    return fmi3OK; \
} \
fmi3Status fmi3Get##Suffix(fmi3Instance instance, const fmi3ValueReference refs[], \
    std::size_t count, Type values[], std::size_t value_count) { \
    if (instance == nullptr || refs == nullptr || values == nullptr || count != value_count) return fmi3Error; \
    const auto* state = static_cast<Instance*>(instance); \
    for (std::size_t index = 0; index < count; ++index) { \
        if (refs[index] != OutputRef) return fmi3Error; \
        values[index] = static_cast<Type>(state->Field + 1); \
    } \
    return fmi3OK; \
}
fmi3Status fmi3SetFloat32(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, const fmi3Float32 values[], std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || values == nullptr) return fmi3Error;
    auto* state = static_cast<Instance*>(instance);
    if (count == 2 && refs[0] == 8 && refs[1] == 39 && value_count == 4) {
        state->float32_input = values[0];
        std::copy_n(values + 1, 3, state->float32_array_input.begin());
        return fmi3OK;
    }
    if (count != value_count) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 8) return fmi3Error;
        state->float32_input = values[index];
    }
    return fmi3OK;
}
fmi3Status fmi3GetFloat32(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, fmi3Float32 values[], std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || values == nullptr) return fmi3Error;
    const auto* state = static_cast<Instance*>(instance);
    if (count == 2 && refs[0] == 9 && refs[1] == 40 && value_count == 4) {
        values[0] = state->float32_input + 1.0F;
        std::transform(state->float32_array_input.begin(), state->float32_array_input.end(),
                       values + 1, [](fmi3Float32 value) { return value + 0.5F; });
        return fmi3OK;
    }
    if (count == 1 && refs[0] == 40 && value_count == 3) {
        std::transform(state->float32_array_input.begin(), state->float32_array_input.end(),
                       values, [](fmi3Float32 value) { return value + 0.5F; });
        return fmi3OK;
    }
    if (count != value_count) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 9) return fmi3Error;
        values[index] = state->float32_input + 1.0F;
    }
    return fmi3OK;
}
SMAVE_FMI3_NUMERIC_IO(fmi3Int8, Int8, 10, 11, int8_input)
SMAVE_FMI3_NUMERIC_IO(fmi3UInt8, UInt8, 12, 13, uint8_input)
SMAVE_FMI3_NUMERIC_IO(fmi3Int16, Int16, 14, 15, int16_input)
SMAVE_FMI3_NUMERIC_IO(fmi3UInt16, UInt16, 16, 17, uint16_input)
SMAVE_FMI3_NUMERIC_IO(fmi3UInt32, UInt32, 18, 19, uint32_input)
SMAVE_FMI3_NUMERIC_IO(fmi3UInt64, UInt64, 22, 23, uint64_input)
#undef SMAVE_FMI3_NUMERIC_IO
fmi3Status fmi3SetInt64(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, const fmi3Int64 values[], std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || values == nullptr || count != value_count) return fmi3Error;
    auto* state = static_cast<Instance*>(instance);
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] == 20) state->int64_input = values[index];
        else if (refs[index] == 24) state->enumeration_input = values[index];
        else return fmi3Error;
    }
    return fmi3OK;
}
fmi3Status fmi3GetInt64(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, fmi3Int64 values[], std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || values == nullptr || count != value_count) return fmi3Error;
    const auto* state = static_cast<Instance*>(instance);
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] == 21) values[index] = state->int64_input + 1;
        else if (refs[index] == 25) values[index] = state->enumeration_input + 1;
        else return fmi3Error;
    }
    return fmi3OK;
}
fmi3Status fmi3SetString(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, const fmi3String values[], std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || values == nullptr) return fmi3Error;
    auto* state = static_cast<Instance*>(instance);
    if (count == 1 && refs[0] == 41 && value_count == 3) {
        for (std::size_t index = 0; index < 3; ++index) {
            if (values[index] == nullptr) return fmi3Error;
            state->string_array_input[index] = values[index];
            state->string_array_output[index] = state->string_array_input[index] + "-array";
        }
        return fmi3OK;
    }
    if (count != value_count) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 26 || values[index] == nullptr) return fmi3Error;
        state->string_input = values[index];
        state->string_output = state->string_input + "-fmi3";
    }
    return fmi3OK;
}
fmi3Status fmi3GetString(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, fmi3String values[], std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || values == nullptr) return fmi3Error;
    const auto* state = static_cast<Instance*>(instance);
    if (count == 1 && refs[0] == 42 && value_count == 3) {
        for (std::size_t index = 0; index < 3; ++index) {
            values[index] = state->string_array_output[index].c_str();
        }
        return fmi3OK;
    }
    if (count != value_count) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 27) return fmi3Error;
        values[index] = state->string_output.c_str();
    }
    return fmi3OK;
}
fmi3Status fmi3SetBinary(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, const std::size_t sizes[], const fmi3Byte* const values[],
    std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || sizes == nullptr || values == nullptr) return fmi3Error;
    auto* state = static_cast<Instance*>(instance);
    if (count == 1 && refs[0] == 43 && value_count == 3) {
        for (std::size_t index = 0; index < 3; ++index) {
            if (sizes[index] != 0 && values[index] == nullptr) return fmi3Error;
            state->binary_array_input[index].clear();
            if (sizes[index] != 0) {
                state->binary_array_input[index].assign(
                    values[index], values[index] + sizes[index]);
            }
            state->binary_array_output[index].resize(sizes[index]);
            for (std::size_t byte = 0; byte < sizes[index]; ++byte) {
                state->binary_array_output[index][byte] =
                    static_cast<fmi3Byte>(state->binary_array_input[index][byte] ^ 0xffU);
            }
        }
        return fmi3OK;
    }
    if (count != value_count) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 28 || (sizes[index] != 0 && values[index] == nullptr)) return fmi3Error;
        state->binary_input.clear();
        if (sizes[index] != 0) {
            state->binary_input.assign(values[index], values[index] + sizes[index]);
        }
        state->binary_output.resize(state->binary_input.size());
        for (std::size_t byte = 0; byte < state->binary_input.size(); ++byte) {
            state->binary_output[byte] = static_cast<fmi3Byte>(state->binary_input[byte] ^ 0xffU);
        }
    }
    return fmi3OK;
}
fmi3Status fmi3GetBinary(fmi3Instance instance, const fmi3ValueReference refs[],
    std::size_t count, std::size_t sizes[], const fmi3Byte* values[],
    std::size_t value_count) {
    if (instance == nullptr || refs == nullptr || sizes == nullptr || values == nullptr) return fmi3Error;
    const auto* state = static_cast<Instance*>(instance);
    if (count == 1 && refs[0] == 44 && value_count == 3) {
        for (std::size_t index = 0; index < 3; ++index) {
            sizes[index] = state->binary_array_output[index].size();
            values[index] = state->binary_array_output[index].empty()
                ? nullptr : state->binary_array_output[index].data();
        }
        return fmi3OK;
    }
    if (count != value_count) return fmi3Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (refs[index] != 29) return fmi3Error;
        sizes[index] = state->binary_output.size();
        values[index] = state->binary_output.empty() ? nullptr : state->binary_output.data();
    }
    return fmi3OK;
}
fmi3Status fmi3GetFMUState(fmi3Instance instance, fmi3FMUState* state) {
    if (state == nullptr) return fmi3Error;
    *state = new (std::nothrow) State{*static_cast<Instance*>(instance)};
    return *state == nullptr ? fmi3Error : fmi3OK;
}
fmi3Status fmi3SetFMUState(fmi3Instance instance, fmi3FMUState state) {
    if (state == nullptr) return fmi3Error;
    *static_cast<Instance*>(instance) = static_cast<State*>(state)->value;
    return fmi3OK;
}
fmi3Status fmi3FreeFMUState(fmi3Instance, fmi3FMUState* state) {
    if (state == nullptr || *state == nullptr) return fmi3Error;
    delete static_cast<State*>(*state); *state = nullptr; return fmi3OK;
}
fmi3Status fmi3SerializedFMUStateSize(
    fmi3Instance instance, fmi3FMUState state, std::size_t* size) {
    if (instance == nullptr || state == nullptr || size == nullptr) return fmi3Error;
    *size = sizeof(std::uint64_t) + 3 * sizeof(double);
    return fmi3OK;
}
fmi3Status fmi3SerializeFMUState(fmi3Instance instance, fmi3FMUState state,
    fmi3Byte serialized[], std::size_t size) {
    constexpr std::uint64_t magic = 0x534d415645334d45ULL;
    constexpr std::size_t expected = sizeof(magic) + 3 * sizeof(double);
    if (instance == nullptr || state == nullptr || serialized == nullptr ||
        size != expected) return fmi3Error;
    const auto& value = static_cast<State*>(state)->value;
    const std::uint64_t serialized_magic =
        static_cast<Instance*>(instance)->invalid_serialized_state ? 0 : magic;
    std::memcpy(serialized, &serialized_magic, sizeof(serialized_magic));
    std::memcpy(serialized + sizeof(magic), &value.time, sizeof(double));
    std::memcpy(serialized + sizeof(magic) + sizeof(double),
                &value.gain, sizeof(double));
    std::memcpy(serialized + sizeof(magic) + 2 * sizeof(double),
                &value.input, sizeof(double));
    return fmi3OK;
}
fmi3Status fmi3DeserializeFMUState(fmi3Instance instance,
    const fmi3Byte serialized[], std::size_t size, fmi3FMUState* state) {
    constexpr std::uint64_t magic = 0x534d415645334d45ULL;
    constexpr std::size_t expected = sizeof(magic) + 3 * sizeof(double);
    if (instance == nullptr || serialized == nullptr || state == nullptr ||
        *state != nullptr || size != expected) return fmi3Error;
    std::uint64_t observed_magic{};
    double time{};
    double gain{};
    double input{};
    std::memcpy(&observed_magic, serialized, sizeof(observed_magic));
    std::memcpy(&time, serialized + sizeof(observed_magic), sizeof(double));
    std::memcpy(&gain, serialized + sizeof(observed_magic) + sizeof(double),
                sizeof(double));
    std::memcpy(&input, serialized + sizeof(observed_magic) + 2 * sizeof(double),
                sizeof(double));
    if (observed_magic != magic || !std::isfinite(time) ||
        !std::isfinite(gain) || !std::isfinite(input)) return fmi3Error;
    auto* restored = new (std::nothrow) State{*static_cast<Instance*>(instance)};
    if (restored == nullptr) return fmi3Error;
    restored->value.time = time;
    restored->value.gain = gain;
    restored->value.input = input;
    *state = restored;
    return fmi3OK;
}
fmi3Status fmi3DoStep(fmi3Instance instance, fmi3Float64 current, fmi3Float64 step, fmi3Boolean, fmi3Boolean* event, fmi3Boolean* terminate, fmi3Boolean* early, fmi3Float64* last) {
    if (event == nullptr || terminate == nullptr || early == nullptr || last == nullptr ||
        !std::isfinite(current) || !std::isfinite(step) || step <= 0.0) return fmi3Error;
    auto* state = static_cast<Instance*>(instance);
    const double requested_end = current + step;
    if ((state->early_return_enabled || state->force_undeclared_early_return) &&
        !state->event_triggered && current < 0.05 - 1.0e-12 &&
        requested_end > 0.05 + 1.0e-12) {
        double return_time = 0.05;
        if (state->early_return_enabled) {
            if (state->intermediate_update == nullptr) return fmi3Error;
            fmi3Boolean early_requested{};
            fmi3Float64 early_time{};
            state->intermediate_update(
                state->environment, return_time, false, false, true, true,
                &early_requested, &early_time);
            if (!early_requested || std::abs(early_time - return_time) > 1.0e-12) {
                return fmi3Error;
            }
            return_time = early_time;
        }
        state->time = return_time;
        state->event_pending = true;
        *event = true; *terminate = false; *early = true; *last = state->time;
        return fmi3OK;
    }
    state->time = requested_end;
    if (state->time_event_enabled) {
        state->event_pending = (!state->time_event_scheduled && state->time >= 0.1 - 1.0e-12) ||
            (state->time_event_scheduled && !state->time_event_triggered &&
             state->time >= 0.15 - 1.0e-12);
        *event = state->event_pending;
        *terminate = false; *early = false; *last = state->time; return fmi3OK;
    }
    state->event_pending = state->event_enabled && !state->event_triggered &&
        state->time >= 0.2 - 1.0e-12;
    *event = state->event_pending;
    *terminate = false; *early = false; *last = state->time; return fmi3OK;
}
fmi3Status fmi3EnterEventMode(fmi3Instance instance) {
    auto* state = static_cast<Instance*>(instance);
    if (!state->event_pending && !state->model_exchange_event_enabled &&
        !state->model_exchange_time_event) return fmi3Error;
    state->event_iteration = 0; return fmi3OK;
}
fmi3Status fmi3UpdateDiscreteStates(fmi3Instance instance, fmi3Boolean* update,
    fmi3Boolean* terminate, fmi3Boolean* nominals_changed,
    fmi3Boolean* continuous_states_changed, fmi3Boolean* next_time_defined,
    fmi3Float64* next_time) {
    if (update == nullptr || terminate == nullptr || nominals_changed == nullptr ||
        continuous_states_changed == nullptr || next_time_defined == nullptr ||
        next_time == nullptr) return fmi3Error;
    auto* state = static_cast<Instance*>(instance);
    if (state->model_exchange_instance && state->model_exchange_initial_update) {
        state->model_exchange_initial_update = false;
        *next_time_defined = state->model_exchange_time_event;
        *next_time = state->model_exchange_time_event
            ? (state->invalid_time_event ? -0.05 : 0.05) : 0.0;
        *continuous_states_changed = false;
        *update = false; *terminate = false; *nominals_changed = false;
        return fmi3OK;
    }
    if (state->model_exchange_time_event) {
        if (!state->time_event_triggered && std::abs(state->time - 0.05) <= 1.0e-9) {
            state->input += 10.0;
            state->time_event_triggered = true;
            *next_time_defined = false;
            *next_time = 0.0;
            *continuous_states_changed = true;
        } else {
            return fmi3Error;
        }
        *update = false; *terminate = false; *nominals_changed = false;
        return fmi3OK;
    }
    if (state->model_exchange_event_enabled) {
        if (state->model_exchange_multi_event) {
            const double threshold = state->model_exchange_event_count == 0 ? 1.03 : 1.07;
            if (state->model_exchange_event_count >= 2 ||
                state->input < threshold - 1.0e-9) return fmi3Error;
            ++state->model_exchange_event_count;
            *continuous_states_changed = false;
        } else {
            if (state->model_exchange_event_triggered ||
                state->model_exchange_near_grazing ||
                state->input < 1.05 - 1.0e-9) {
                return fmi3Error;
            }
            state->input = 0.5;
            state->model_exchange_event_triggered = true;
            *continuous_states_changed = true;
            if (state->model_exchange_nominal_change) {
                state->continuous_state_nominal =
                    state->model_exchange_invalid_nominal ? 0.0 : 2.0;
            }
        }
        *update = false; *terminate = false;
        *nominals_changed = state->model_exchange_nominal_change;
        *next_time_defined = false; *next_time = 0.0;
        return fmi3OK;
    }
    if (state->time_event_enabled) {
        if (!state->time_event_scheduled) {
            state->time_event_scheduled = true;
            state->event_pending = false;
            *update = false;
            *next_time_defined = true;
            *next_time = state->invalid_time_event ? 0.05 : 0.15;
        } else {
            state->input += 10.0;
            state->time_event_triggered = true;
            state->event_pending = false;
            *update = false;
            *next_time_defined = false;
            *next_time = 0.0;
        }
        ++state->event_iteration;
        *terminate = false; *nominals_changed = false;
        *continuous_states_changed = false;
        return fmi3OK;
    }
    if (state->event_iteration == 0) {
        state->input += 10.0;
        state->event_triggered = true;
        *update = true;
    } else {
        state->event_pending = false;
        *update = false;
    }
    ++state->event_iteration;
    *terminate = false; *nominals_changed = false;
    *continuous_states_changed = false; *next_time_defined = false; *next_time = 0.0;
    return fmi3OK;
}
fmi3Status fmi3EnterStepMode(fmi3Instance instance) {
    return static_cast<Instance*>(instance)->event_pending ? fmi3Error : fmi3OK;
}
}

extern "C" {
fmi3Instance fmi3InstantiateModelExchange(
    fmi3String, fmi3String token, fmi3String, fmi3Boolean, fmi3Boolean,
    fmi3InstanceEnvironment, fmi3LogMessageCallback) {
    auto* instance = new (std::nothrow) Instance{};
    if (instance != nullptr) instance->model_exchange_instance = true;
    if (instance != nullptr && token != nullptr && std::strstr(token, "me-time-event") != nullptr) {
        instance->model_exchange_time_event = true;
        instance->invalid_time_event = std::strstr(token, "invalid") != nullptr;
    }
    if (instance != nullptr && token != nullptr &&
        std::strstr(token, "me-event") != nullptr &&
        std::strstr(token, "me-time-event") == nullptr) {
        instance->model_exchange_event_enabled = true;
    }
    if (instance != nullptr && token != nullptr && std::strstr(token, "me-multi-event") != nullptr) {
        instance->model_exchange_event_enabled = true;
        instance->model_exchange_multi_event = true;
    }
    if (instance != nullptr && token != nullptr &&
        std::strstr(token, "me-grazing") != nullptr) {
        instance->model_exchange_event_enabled = true;
        instance->model_exchange_grazing = true;
    }
    if (instance != nullptr && token != nullptr &&
        std::strstr(token, "me-near-grazing") != nullptr) {
        instance->model_exchange_event_enabled = true;
        instance->model_exchange_grazing = true;
        instance->model_exchange_near_grazing = true;
    }
    if (instance != nullptr && token != nullptr &&
        std::strstr(token, "me-nominal") != nullptr) {
        instance->model_exchange_event_enabled = true;
        instance->model_exchange_nominal_change = true;
    }
    if (instance != nullptr && token != nullptr &&
        std::strstr(token, "me-invalid-nominal") != nullptr) {
        instance->model_exchange_event_enabled = true;
        instance->model_exchange_nominal_change = true;
        instance->model_exchange_invalid_nominal = true;
    }
    if (instance != nullptr && token != nullptr &&
        std::strstr(token, "invalid-clock-interval") != nullptr) {
        instance->invalid_clock_interval = true;
    }
    if (instance != nullptr && token != nullptr &&
        std::strstr(token, "invalid-serialized-state") != nullptr) {
        instance->invalid_serialized_state = true;
    }
    return instance;
}
fmi3Status fmi3EnterContinuousTimeMode(fmi3Instance) { return fmi3OK; }
fmi3Status fmi3SetTime(fmi3Instance instance, fmi3Float64 time) {
    if (!std::isfinite(time)) return fmi3Error;
    static_cast<Instance*>(instance)->time = time; return fmi3OK;
}
fmi3Status fmi3SetContinuousStates(fmi3Instance instance, const fmi3Float64 states[], std::size_t count) {
    if (count != 1 || states == nullptr || !std::isfinite(states[0])) return fmi3Error;
    static_cast<Instance*>(instance)->input = states[0]; return fmi3OK;
}
fmi3Status fmi3GetContinuousStates(fmi3Instance instance, fmi3Float64 states[], std::size_t count) {
    if (count != 1 || states == nullptr) return fmi3Error;
    states[0] = static_cast<Instance*>(instance)->input; return fmi3OK;
}
fmi3Status fmi3GetNominalsOfContinuousStates(
    fmi3Instance instance, fmi3Float64 nominals[], std::size_t count) {
    if (instance == nullptr || nominals == nullptr || count != 1) return fmi3Error;
    nominals[0] = static_cast<Instance*>(instance)->continuous_state_nominal;
    return fmi3OK;
}
fmi3Status fmi3GetContinuousStateDerivatives(fmi3Instance instance, fmi3Float64 derivatives[], std::size_t count) {
    if (count != 1 || derivatives == nullptr) return fmi3Error;
    const auto* state = static_cast<Instance*>(instance);
    derivatives[0] = state->gain * state->input; return fmi3OK;
}
fmi3Status fmi3GetNumberOfContinuousStates(fmi3Instance, std::size_t* count) {
    if (count == nullptr) return fmi3Error; *count = 1; return fmi3OK;
}
fmi3Status fmi3GetNumberOfEventIndicators(fmi3Instance instance, std::size_t* count) {
    if (count == nullptr) return fmi3Error;
    const auto* state = static_cast<Instance*>(instance);
    *count = state->model_exchange_multi_event ? 2 :
        (state->model_exchange_event_enabled ? 1 : 0);
    return fmi3OK;
}
fmi3Status fmi3GetEventIndicators(fmi3Instance instance, fmi3Float64 indicators[], std::size_t count) {
    const auto* state = static_cast<Instance*>(instance);
    const std::size_t expected = state->model_exchange_multi_event ? 2 : 1;
    if (!state->model_exchange_event_enabled || count != expected || indicators == nullptr) {
        return fmi3Error;
    }
    if (state->model_exchange_multi_event) {
        indicators[0] = state->input - 1.03;
        indicators[1] = state->input - 1.07;
    } else if (state->model_exchange_grazing) {
        const double distance = state->input - 1.05;
        indicators[0] = distance * distance +
            (state->model_exchange_near_grazing ? 0.01 : 0.0);
    } else {
        indicators[0] = state->input - 1.05;
    }
    return fmi3OK;
}
fmi3Status fmi3CompletedIntegratorStep(fmi3Instance, fmi3Boolean, fmi3Boolean* event_mode, fmi3Boolean* terminate) {
    if (event_mode == nullptr || terminate == nullptr) return fmi3Error;
    *event_mode = false; *terminate = false; return fmi3OK;
}
}
