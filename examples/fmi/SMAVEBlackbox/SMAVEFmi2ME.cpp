#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

extern "C" {

enum fmi2Status { fmi2OK, fmi2Warning, fmi2Discard, fmi2Error, fmi2Fatal, fmi2Pending };
enum fmi2Type { fmi2ModelExchange, fmi2CoSimulation };
using fmi2Component = void*;
using fmi2ComponentEnvironment = void*;
using fmi2FMUstate = void*;
using fmi2ValueReference = std::uint32_t;
using fmi2Real = double;
using fmi2Integer = int;
using fmi2Boolean = int;
using fmi2String = const char*;
using fmi2Byte = std::uint8_t;
using fmi2CallbackLogger = void (*)(fmi2ComponentEnvironment, fmi2String,
    fmi2Status, fmi2String, fmi2String, ...);
using fmi2CallbackAllocateMemory = void* (*)(std::size_t, std::size_t);
using fmi2CallbackFreeMemory = void (*)(void*);
using fmi2StepFinished = void (*)(fmi2ComponentEnvironment, fmi2Status);
struct fmi2CallbackFunctions {
    fmi2CallbackLogger logger;
    fmi2CallbackAllocateMemory allocateMemory;
    fmi2CallbackFreeMemory freeMemory;
    fmi2StepFinished stepFinished;
    fmi2ComponentEnvironment componentEnvironment;
};
struct fmi2EventInfo {
    fmi2Boolean newDiscreteStatesNeeded;
    fmi2Boolean terminateSimulation;
    fmi2Boolean nominalsOfContinuousStatesChanged;
    fmi2Boolean valuesOfContinuousStatesChanged;
    fmi2Boolean nextEventTimeDefined;
    fmi2Real nextEventTime;
};
struct Instance {
    double time{0.0};
    double state{0.0};
    double rate{1.0};
    bool initialized{false};
    bool root_handled{false};
    bool time_event_handled{false};
    bool grazing{false};
    bool near_grazing{false};
    bool nominal_change{false};
    bool invalid_nominal{false};
    double nominal{1.0};
    fmi2Integer integer_input{0};
    fmi2Integer enumeration_input{0};
    std::string string_input;
    std::string string_output;
    fmi2Boolean boolean_input{false};
    bool invalid_serialized_state{false};
};
struct State { Instance value; };

const char* fmi2GetVersion() { return "2.0"; }
fmi2Component fmi2Instantiate(fmi2String, fmi2Type type, fmi2String guid,
    fmi2String, const fmi2CallbackFunctions* callbacks, fmi2Boolean, fmi2Boolean) {
    if (type != fmi2ModelExchange || guid == nullptr || callbacks == nullptr ||
        callbacks->allocateMemory == nullptr || callbacks->freeMemory == nullptr) {
        return nullptr;
    }
    const bool standard = std::strcmp(guid, "smave-fmi2-me-token") == 0;
    const bool grazing = std::strcmp(guid, "smave-fmi2-me-grazing-token") == 0;
    const bool near_grazing =
        std::strcmp(guid, "smave-fmi2-me-near-grazing-token") == 0;
    const bool nominal = std::strcmp(guid, "smave-fmi2-me-nominal-token") == 0;
    const bool invalid_nominal =
        std::strcmp(guid, "smave-fmi2-me-invalid-nominal-token") == 0;
    const bool scalar_types =
        std::strcmp(guid, "smave-fmi2-me-scalar-types-token") == 0;
    const bool invalid_serialized =
        std::strcmp(guid, "smave-fmi2-me-invalid-serialized-token") == 0;
    if (!standard && !grazing && !near_grazing && !nominal && !invalid_nominal &&
        !scalar_types && !invalid_serialized) {
        return nullptr;
    }
    auto* instance = new (std::nothrow) Instance{};
    if (instance != nullptr) {
        instance->grazing = grazing || near_grazing;
        instance->near_grazing = near_grazing;
        instance->nominal_change = nominal || invalid_nominal;
        instance->invalid_nominal = invalid_nominal;
        instance->invalid_serialized_state = invalid_serialized;
    }
    return instance;
}
void fmi2FreeInstance(fmi2Component component) { delete static_cast<Instance*>(component); }
fmi2Status fmi2SetupExperiment(fmi2Component component, fmi2Boolean, fmi2Real,
    fmi2Real start, fmi2Boolean, fmi2Real) {
    if (component == nullptr || !std::isfinite(start)) return fmi2Error;
    static_cast<Instance*>(component)->time = start;
    return fmi2OK;
}
fmi2Status fmi2EnterInitializationMode(fmi2Component component) {
    return component == nullptr ? fmi2Error : fmi2OK;
}
fmi2Status fmi2ExitInitializationMode(fmi2Component component) {
    if (component == nullptr) return fmi2Error;
    static_cast<Instance*>(component)->initialized = true;
    return fmi2OK;
}
fmi2Status fmi2EnterEventMode(fmi2Component component) {
    return component == nullptr ? fmi2Error : fmi2OK;
}
fmi2Status fmi2NewDiscreteStates(fmi2Component component, fmi2EventInfo* info) {
    if (component == nullptr || info == nullptr) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    info->newDiscreteStatesNeeded = false;
    info->terminateSimulation = false;
    info->nominalsOfContinuousStatesChanged = false;
    info->valuesOfContinuousStatesChanged = false;
    if (!instance->root_handled && instance->state >= 0.05 - 1.0e-9 &&
        !instance->near_grazing) {
        instance->state = 0.5;
        instance->root_handled = true;
        info->valuesOfContinuousStatesChanged = true;
        if (instance->nominal_change) {
            instance->nominal = instance->invalid_nominal ? 0.0 : 2.0;
            info->nominalsOfContinuousStatesChanged = true;
        }
    } else if (instance->root_handled && !instance->time_event_handled &&
               std::abs(instance->time - 0.075) <= 1.0e-9) {
        instance->state += 1.0;
        instance->time_event_handled = true;
        info->valuesOfContinuousStatesChanged = true;
    }
    info->nextEventTimeDefined = !instance->grazing && !instance->nominal_change &&
        !instance->time_event_handled;
    info->nextEventTime = 0.075;
    return fmi2OK;
}
fmi2Status fmi2EnterContinuousTimeMode(fmi2Component component) {
    return component == nullptr ? fmi2Error : fmi2OK;
}
fmi2Status fmi2CompletedIntegratorStep(fmi2Component component, fmi2Boolean,
    fmi2Boolean* enter_event, fmi2Boolean* terminate) {
    if (component == nullptr || enter_event == nullptr || terminate == nullptr) return fmi2Error;
    *enter_event = false;
    *terminate = false;
    return fmi2OK;
}
fmi2Status fmi2SetTime(fmi2Component component, fmi2Real time) {
    if (component == nullptr || !std::isfinite(time)) return fmi2Error;
    static_cast<Instance*>(component)->time = time;
    return fmi2OK;
}
fmi2Status fmi2SetContinuousStates(fmi2Component component,
    const fmi2Real states[], std::size_t count) {
    if (component == nullptr || states == nullptr || count != 1 ||
        !std::isfinite(states[0])) return fmi2Error;
    static_cast<Instance*>(component)->state = states[0];
    return fmi2OK;
}
fmi2Status fmi2GetContinuousStates(fmi2Component component,
    fmi2Real states[], std::size_t count) {
    if (component == nullptr || states == nullptr || count != 1) return fmi2Error;
    states[0] = static_cast<Instance*>(component)->state;
    return fmi2OK;
}
fmi2Status fmi2GetNominalsOfContinuousStates(fmi2Component component,
    fmi2Real nominals[], std::size_t count) {
    if (component == nullptr || nominals == nullptr || count != 1) return fmi2Error;
    nominals[0] = static_cast<Instance*>(component)->nominal;
    return fmi2OK;
}
fmi2Status fmi2GetDerivatives(fmi2Component component,
    fmi2Real derivatives[], std::size_t count) {
    if (component == nullptr || derivatives == nullptr || count != 1) return fmi2Error;
    derivatives[0] = static_cast<Instance*>(component)->rate;
    return fmi2OK;
}
fmi2Status fmi2GetEventIndicators(fmi2Component component,
    fmi2Real indicators[], std::size_t count) {
    if (component == nullptr || indicators == nullptr || count != 1) return fmi2Error;
    const auto* instance = static_cast<Instance*>(component);
    if (instance->root_handled) {
        indicators[0] = 1.0;
    } else if (instance->grazing) {
        const double distance = instance->state - 0.05;
        indicators[0] = distance * distance +
            (instance->near_grazing ? 0.01 : 0.0);
    } else {
        indicators[0] = instance->state - 0.05;
    }
    return fmi2OK;
}
fmi2Status fmi2SetReal(fmi2Component component,
    const fmi2ValueReference references[], std::size_t count, const fmi2Real values[]) {
    if (component == nullptr || references == nullptr || values == nullptr) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    for (std::size_t index = 0; index < count; ++index) {
        if (references[index] != 3 || !std::isfinite(values[index])) return fmi2Error;
        instance->rate = values[index];
    }
    return fmi2OK;
}
fmi2Status fmi2GetReal(fmi2Component component,
    const fmi2ValueReference references[], std::size_t count, fmi2Real values[]) {
    if (component == nullptr || references == nullptr || values == nullptr) return fmi2Error;
    const auto* instance = static_cast<Instance*>(component);
    for (std::size_t index = 0; index < count; ++index) {
        if (references[index] == 1 || references[index] == 4) values[index] = instance->state;
        else if (references[index] == 2) values[index] = instance->rate;
        else return fmi2Error;
    }
    return fmi2OK;
}
fmi2Status fmi2SetInteger(fmi2Component component,
    const fmi2ValueReference references[], std::size_t count,
    const fmi2Integer values[]) {
    if (component == nullptr || references == nullptr || values == nullptr) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    for (std::size_t index = 0; index < count; ++index) {
        if (references[index] == 5) instance->integer_input = values[index];
        else if (references[index] == 9) instance->enumeration_input = values[index];
        else return fmi2Error;
    }
    return fmi2OK;
}
fmi2Status fmi2GetInteger(fmi2Component component,
    const fmi2ValueReference references[], std::size_t count,
    fmi2Integer values[]) {
    if (component == nullptr || references == nullptr || values == nullptr) return fmi2Error;
    const auto* instance = static_cast<Instance*>(component);
    for (std::size_t index = 0; index < count; ++index) {
        if (references[index] == 7) values[index] = instance->integer_input + 1;
        else if (references[index] == 10) values[index] = instance->enumeration_input + 1;
        else return fmi2Error;
    }
    return fmi2OK;
}
fmi2Status fmi2SetBoolean(fmi2Component component,
    const fmi2ValueReference references[], std::size_t count,
    const fmi2Boolean values[]) {
    if (component == nullptr || references == nullptr || values == nullptr) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    for (std::size_t index = 0; index < count; ++index) {
        if (references[index] != 6) return fmi2Error;
        instance->boolean_input = values[index] != 0;
    }
    return fmi2OK;
}
fmi2Status fmi2GetBoolean(fmi2Component component,
    const fmi2ValueReference references[], std::size_t count,
    fmi2Boolean values[]) {
    if (component == nullptr || references == nullptr || values == nullptr) return fmi2Error;
    const auto* instance = static_cast<Instance*>(component);
    for (std::size_t index = 0; index < count; ++index) {
        if (references[index] != 8) return fmi2Error;
        values[index] = !instance->boolean_input;
    }
    return fmi2OK;
}
fmi2Status fmi2SetString(fmi2Component component,
    const fmi2ValueReference references[], std::size_t count,
    const fmi2String values[]) {
    if (component == nullptr || references == nullptr || values == nullptr) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    for (std::size_t index = 0; index < count; ++index) {
        if (references[index] != 11 || values[index] == nullptr) return fmi2Error;
        instance->string_input = values[index];
        instance->string_output = instance->string_input + "-fmi2";
    }
    return fmi2OK;
}
fmi2Status fmi2GetString(fmi2Component component,
    const fmi2ValueReference references[], std::size_t count,
    fmi2String values[]) {
    if (component == nullptr || references == nullptr || values == nullptr) return fmi2Error;
    const auto* instance = static_cast<Instance*>(component);
    for (std::size_t index = 0; index < count; ++index) {
        if (references[index] != 12) return fmi2Error;
        values[index] = instance->string_output.c_str();
    }
    return fmi2OK;
}
fmi2Status fmi2GetFMUstate(fmi2Component component, fmi2FMUstate* state) {
    if (component == nullptr || state == nullptr) return fmi2Error;
    *state = new (std::nothrow) State{*static_cast<Instance*>(component)};
    return *state == nullptr ? fmi2Error : fmi2OK;
}
fmi2Status fmi2SetFMUstate(fmi2Component component, fmi2FMUstate state) {
    if (component == nullptr || state == nullptr) return fmi2Error;
    *static_cast<Instance*>(component) = static_cast<State*>(state)->value;
    return fmi2OK;
}
fmi2Status fmi2FreeFMUstate(fmi2Component, fmi2FMUstate* state) {
    if (state == nullptr || *state == nullptr) return fmi2Error;
    delete static_cast<State*>(*state);
    *state = nullptr;
    return fmi2OK;
}
fmi2Status fmi2SerializedFMUstateSize(
    fmi2Component component, fmi2FMUstate state, std::size_t* size) {
    if (component == nullptr || state == nullptr || size == nullptr) return fmi2Error;
    *size = sizeof(std::uint64_t) + 3 * sizeof(double) + 3;
    return fmi2OK;
}
fmi2Status fmi2SerializeFMUstate(fmi2Component component, fmi2FMUstate state,
    fmi2Byte serialized[], std::size_t size) {
    constexpr std::uint64_t magic = 0x534d415645324d45ULL;
    constexpr std::size_t expected = sizeof(magic) + 3 * sizeof(double) + 3;
    if (component == nullptr || state == nullptr || serialized == nullptr ||
        size != expected) return fmi2Error;
    const auto& value = static_cast<State*>(state)->value;
    const std::uint64_t serialized_magic =
        static_cast<Instance*>(component)->invalid_serialized_state ? 0 : magic;
    std::memcpy(serialized, &serialized_magic, sizeof(serialized_magic));
    std::size_t offset = sizeof(serialized_magic);
    for (const double number : {value.time, value.state, value.rate}) {
        std::memcpy(serialized + offset, &number, sizeof(number));
        offset += sizeof(number);
    }
    serialized[offset++] = value.initialized ? 1U : 0U;
    serialized[offset++] = value.root_handled ? 1U : 0U;
    serialized[offset] = value.time_event_handled ? 1U : 0U;
    return fmi2OK;
}
fmi2Status fmi2DeSerializeFMUstate(fmi2Component component,
    const fmi2Byte serialized[], std::size_t size, fmi2FMUstate* state) {
    constexpr std::uint64_t magic = 0x534d415645324d45ULL;
    constexpr std::size_t expected = sizeof(magic) + 3 * sizeof(double) + 3;
    if (component == nullptr || serialized == nullptr || state == nullptr ||
        *state != nullptr || size != expected) return fmi2Error;
    std::uint64_t observed_magic{};
    std::memcpy(&observed_magic, serialized, sizeof(observed_magic));
    std::size_t offset = sizeof(observed_magic);
    double values[3]{};
    for (double& number : values) {
        std::memcpy(&number, serialized + offset, sizeof(number));
        offset += sizeof(number);
    }
    if (observed_magic != magic || !std::isfinite(values[0]) ||
        !std::isfinite(values[1]) || !std::isfinite(values[2]) ||
        serialized[offset] > 1U || serialized[offset + 1] > 1U ||
        serialized[offset + 2] > 1U) return fmi2Error;
    auto* restored = new (std::nothrow) State{*static_cast<Instance*>(component)};
    if (restored == nullptr) return fmi2Error;
    restored->value.time = values[0];
    restored->value.state = values[1];
    restored->value.rate = values[2];
    restored->value.initialized = serialized[offset] != 0;
    restored->value.root_handled = serialized[offset + 1] != 0;
    restored->value.time_event_handled = serialized[offset + 2] != 0;
    *state = restored;
    return fmi2OK;
}
fmi2Status fmi2Terminate(fmi2Component component) {
    return component == nullptr ? fmi2Error : fmi2OK;
}

}
