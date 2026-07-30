#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>

extern "C" {

enum fmi2Status { fmi2OK, fmi2Warning, fmi2Discard, fmi2Error, fmi2Fatal, fmi2Pending };
enum fmi2Type { fmi2ModelExchange, fmi2CoSimulation };
enum fmi2StatusKind {
    fmi2DoStepStatus,
    fmi2PendingStatus,
    fmi2LastSuccessfulTime,
    fmi2Terminated
};
using fmi2Component = void*;
using fmi2ComponentEnvironment = void*;
using fmi2FMUstate = void*;
using fmi2ValueReference = std::uint32_t;
using fmi2Real = double;
using fmi2Integer = int;
using fmi2Boolean = int;
using fmi2String = const char*;
using fmi2Byte = std::uint8_t;
using fmi2CallbackLogger = void (*)(
    fmi2ComponentEnvironment, fmi2String, fmi2Status, fmi2String,
    fmi2String, ...);
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
struct Instance {
    double time{0.0};
    double gain{2.0};
    double input{1.0};
    bool invalid_serialized_state{false};
};
struct State { Instance value; };

const char* fmi2GetVersion() { return "2.0"; }
fmi2Component fmi2Instantiate(
    fmi2String, fmi2Type type, fmi2String guid, fmi2String,
    const fmi2CallbackFunctions* callbacks, fmi2Boolean, fmi2Boolean) {
    if (type != fmi2CoSimulation || guid == nullptr || callbacks == nullptr ||
        callbacks->allocateMemory == nullptr || callbacks->freeMemory == nullptr) {
        return nullptr;
    }
    const bool valid = std::strcmp(guid, "smave-fmi2-token") == 0;
    const bool invalid_serialized =
        std::strcmp(guid, "smave-fmi2-invalid-serialized-token") == 0;
    if (!valid && !invalid_serialized) return nullptr;
    auto* instance = new (std::nothrow) Instance{};
    if (instance != nullptr) instance->invalid_serialized_state = invalid_serialized;
    return instance;
}
void fmi2FreeInstance(fmi2Component component) {
    delete static_cast<Instance*>(component);
}
fmi2Status fmi2SetupExperiment(
    fmi2Component component, fmi2Boolean, fmi2Real, fmi2Real start,
    fmi2Boolean, fmi2Real) {
    if (component == nullptr || !std::isfinite(start)) return fmi2Error;
    static_cast<Instance*>(component)->time = start;
    return fmi2OK;
}
fmi2Status fmi2EnterInitializationMode(fmi2Component component) {
    return component == nullptr ? fmi2Error : fmi2OK;
}
fmi2Status fmi2ExitInitializationMode(fmi2Component component) {
    return component == nullptr ? fmi2Error : fmi2OK;
}
fmi2Status fmi2Terminate(fmi2Component component) {
    return component == nullptr ? fmi2Error : fmi2OK;
}
fmi2Status fmi2SetReal(
    fmi2Component component, const fmi2ValueReference references[],
    std::size_t count, const fmi2Real values[]) {
    if (component == nullptr || references == nullptr || values == nullptr) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    for (std::size_t index = 0; index < count; ++index) {
        if (!std::isfinite(values[index])) return fmi2Error;
        if (references[index] == 1) instance->gain = values[index];
        else if (references[index] == 2) instance->input = values[index];
        else return fmi2Error;
    }
    return fmi2OK;
}
fmi2Status fmi2GetReal(
    fmi2Component component, const fmi2ValueReference references[],
    std::size_t count, fmi2Real values[]) {
    if (component == nullptr || references == nullptr || values == nullptr) return fmi2Error;
    const auto* instance = static_cast<const Instance*>(component);
    for (std::size_t index = 0; index < count; ++index) {
        if (references[index] == 3) {
            values[index] = instance->gain * instance->input + instance->time;
        } else {
            return fmi2Error;
        }
    }
    return fmi2OK;
}
fmi2Status fmi2DoStep(
    fmi2Component component, fmi2Real current, fmi2Real step,
    fmi2Boolean) {
    if (component == nullptr || !std::isfinite(current) ||
        !std::isfinite(step) || step <= 0.0) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    if (std::abs(instance->time - current) > 1.0e-12) return fmi2Error;
    instance->time = current + step;
    return fmi2OK;
}
fmi2Status fmi2GetRealStatus(
    fmi2Component component, fmi2StatusKind kind, fmi2Real* value) {
    if (component == nullptr || value == nullptr ||
        kind != fmi2LastSuccessfulTime) return fmi2Error;
    *value = static_cast<const Instance*>(component)->time;
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
    *size = sizeof(std::uint64_t) + 3 * sizeof(double);
    return fmi2OK;
}
fmi2Status fmi2SerializeFMUstate(fmi2Component component, fmi2FMUstate state,
    fmi2Byte serialized[], std::size_t size) {
    constexpr std::uint64_t magic = 0x534d415645324353ULL;
    constexpr std::size_t expected = sizeof(magic) + 3 * sizeof(double);
    if (component == nullptr || state == nullptr || serialized == nullptr ||
        size != expected) return fmi2Error;
    const auto& value = static_cast<State*>(state)->value;
    const std::uint64_t serialized_magic =
        static_cast<Instance*>(component)->invalid_serialized_state ? 0 : magic;
    std::memcpy(serialized, &serialized_magic, sizeof(serialized_magic));
    std::size_t offset = sizeof(serialized_magic);
    for (const double number : {value.time, value.gain, value.input}) {
        std::memcpy(serialized + offset, &number, sizeof(number));
        offset += sizeof(number);
    }
    return fmi2OK;
}
fmi2Status fmi2DeSerializeFMUstate(fmi2Component component,
    const fmi2Byte serialized[], std::size_t size, fmi2FMUstate* state) {
    constexpr std::uint64_t magic = 0x534d415645324353ULL;
    constexpr std::size_t expected = sizeof(magic) + 3 * sizeof(double);
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
        !std::isfinite(values[1]) || !std::isfinite(values[2])) return fmi2Error;
    auto* restored = new (std::nothrow) State{*static_cast<Instance*>(component)};
    if (restored == nullptr) return fmi2Error;
    restored->value.time = values[0];
    restored->value.gain = values[1];
    restored->value.input = values[2];
    *state = restored;
    return fmi2OK;
}

}
