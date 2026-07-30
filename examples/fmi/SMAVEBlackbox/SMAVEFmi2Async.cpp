#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <thread>

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
using fmi2Boolean = int;
using fmi2String = const char*;
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
    std::mutex mutex;
    std::thread worker;
    double time{0.0};
    double pending_target{0.0};
    double gain{3.0};
    double input{2.0};
    bool pending{false};
    bool cancel_requested{false};
    bool asynchronous_step_completed{false};
    bool stuck{false};
    bool suppress_callback{false};
    fmi2StepFinished step_finished{};
    fmi2ComponentEnvironment environment{};
};
struct State {
    double time{};
    double pending_target{};
    double gain{};
    double input{};
    bool pending{};
    bool cancel_requested{};
    bool asynchronous_step_completed{};
};

void join_worker(Instance& instance) {
    if (instance.worker.joinable()) instance.worker.join();
}

const char* fmi2GetVersion() { return "2.0"; }
fmi2Component fmi2Instantiate(
    fmi2String, fmi2Type type, fmi2String guid, fmi2String,
    const fmi2CallbackFunctions* callbacks, fmi2Boolean, fmi2Boolean) {
    if (type != fmi2CoSimulation || guid == nullptr || callbacks == nullptr ||
        callbacks->allocateMemory == nullptr || callbacks->freeMemory == nullptr ||
        callbacks->stepFinished == nullptr ||
        std::strstr(guid, "smave-fmi2-async-token") == nullptr) {
        return nullptr;
    }
    auto* instance = new (std::nothrow) Instance{};
    if (instance == nullptr) return nullptr;
    instance->stuck = std::strstr(guid, "stuck") != nullptr;
    instance->suppress_callback = std::strstr(guid, "no-callback") != nullptr;
    instance->step_finished = callbacks->stepFinished;
    instance->environment = callbacks->componentEnvironment;
    return instance;
}
void fmi2FreeInstance(fmi2Component component) {
    auto* instance = static_cast<Instance*>(component);
    if (instance == nullptr) return;
    {
        std::lock_guard lock(instance->mutex);
        instance->cancel_requested = true;
        instance->pending = false;
    }
    join_worker(*instance);
    delete instance;
}
fmi2Status fmi2SetupExperiment(
    fmi2Component component, fmi2Boolean, fmi2Real, fmi2Real start,
    fmi2Boolean, fmi2Real) {
    if (component == nullptr || !std::isfinite(start)) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    std::lock_guard lock(instance->mutex);
    instance->time = start;
    return fmi2OK;
}
fmi2Status fmi2EnterInitializationMode(fmi2Component component) {
    return component == nullptr ? fmi2Error : fmi2OK;
}
fmi2Status fmi2ExitInitializationMode(fmi2Component component) {
    return component == nullptr ? fmi2Error : fmi2OK;
}
fmi2Status fmi2Terminate(fmi2Component component) {
    if (component == nullptr) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    join_worker(*instance);
    std::lock_guard lock(instance->mutex);
    return instance->pending ? fmi2Error : fmi2OK;
}
fmi2Status fmi2SetReal(
    fmi2Component component, const fmi2ValueReference references[],
    std::size_t count, const fmi2Real values[]) {
    if (component == nullptr || references == nullptr || values == nullptr) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    std::lock_guard lock(instance->mutex);
    if (instance->pending) return fmi2Error;
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
    auto* instance = static_cast<Instance*>(component);
    std::lock_guard lock(instance->mutex);
    if (instance->pending) return fmi2Error;
    for (std::size_t index = 0; index < count; ++index) {
        if (references[index] != 3) return fmi2Error;
        values[index] = instance->gain * instance->input + instance->time;
    }
    return fmi2OK;
}
fmi2Status fmi2DoStep(
    fmi2Component component, fmi2Real current, fmi2Real step,
    fmi2Boolean) {
    if (component == nullptr || !std::isfinite(current) ||
        !std::isfinite(step) || step <= 0.0) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    join_worker(*instance);
    {
        std::lock_guard lock(instance->mutex);
        if (instance->pending || std::abs(instance->time - current) > 1.0e-12) {
            return fmi2Error;
        }
        const double target = current + step;
        if (!instance->asynchronous_step_completed &&
            current >= 0.1 - 1.0e-12 && current < 0.2 - 1.0e-12) {
            instance->pending = true;
            instance->cancel_requested = false;
            instance->pending_target = target;
            if (instance->stuck) return fmi2Pending;
        } else {
            instance->time = target;
            return fmi2OK;
        }
    }
    instance->worker = std::thread([instance] {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        fmi2StepFinished callback{};
        fmi2ComponentEnvironment environment{};
        bool suppress_callback{};
        {
            std::lock_guard lock(instance->mutex);
            if (instance->cancel_requested || !instance->pending) return;
            instance->time = instance->pending_target;
            instance->pending = false;
            instance->asynchronous_step_completed = true;
            callback = instance->step_finished;
            environment = instance->environment;
            suppress_callback = instance->suppress_callback;
        }
        if (!suppress_callback) callback(environment, fmi2OK);
    });
    return fmi2Pending;
}
fmi2Status fmi2GetStatus(
    fmi2Component component, fmi2StatusKind kind, fmi2Status* value) {
    if (component == nullptr || value == nullptr || kind != fmi2DoStepStatus) {
        return fmi2Error;
    }
    auto* instance = static_cast<Instance*>(component);
    std::lock_guard lock(instance->mutex);
    *value = instance->pending ? fmi2Pending : fmi2OK;
    return fmi2OK;
}
fmi2Status fmi2CancelStep(fmi2Component component) {
    if (component == nullptr) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    {
        std::lock_guard lock(instance->mutex);
        if (!instance->pending) return fmi2Error;
        instance->cancel_requested = true;
        instance->pending = false;
        instance->pending_target = instance->time;
    }
    join_worker(*instance);
    return fmi2OK;
}
fmi2Status fmi2GetFMUstate(fmi2Component component, fmi2FMUstate* state) {
    if (component == nullptr || state == nullptr) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    join_worker(*instance);
    std::lock_guard lock(instance->mutex);
    if (instance->pending) return fmi2Error;
    *state = new (std::nothrow) State{
        instance->time,
        instance->pending_target,
        instance->gain,
        instance->input,
        instance->pending,
        instance->cancel_requested,
        instance->asynchronous_step_completed};
    return *state == nullptr ? fmi2Error : fmi2OK;
}
fmi2Status fmi2SetFMUstate(fmi2Component component, fmi2FMUstate state) {
    if (component == nullptr || state == nullptr) return fmi2Error;
    auto* instance = static_cast<Instance*>(component);
    join_worker(*instance);
    std::lock_guard lock(instance->mutex);
    if (instance->pending) return fmi2Error;
    const auto& saved = *static_cast<State*>(state);
    instance->time = saved.time;
    instance->pending_target = saved.pending_target;
    instance->gain = saved.gain;
    instance->input = saved.input;
    instance->pending = saved.pending;
    instance->cancel_requested = saved.cancel_requested;
    instance->asynchronous_step_completed = saved.asynchronous_step_completed;
    return fmi2OK;
}
fmi2Status fmi2FreeFMUstate(fmi2Component, fmi2FMUstate* state) {
    if (state == nullptr || *state == nullptr) return fmi2Error;
    delete static_cast<State*>(*state);
    *state = nullptr;
    return fmi2OK;
}

}
