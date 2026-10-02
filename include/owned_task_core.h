#pragma once
// FreeRTOS task deletion does not unwind C++ automatic objects. Finish owned
// work in a returning scope before publishing readiness or deleting the task.
template<class Work, class Publish, class Finish>
void runOwnedTask(const Work& work, const Publish& publish, const Finish& finish) {
    const auto result = work();
    publish(result);
    finish();
}
