#include <lv2/core/lv2.h>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <new>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <limits>
#include <vector>

namespace {
std::atomic<bool> track_allocations{false};
std::atomic<std::size_t> allocation_count{0};
}

void* operator new(std::size_t size) {
    if (track_allocations.load(std::memory_order_relaxed))
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* value = std::malloc(size)) return value;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

int main(int argc, char** argv) {
    if (argc != 4) return 1;
    void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) { std::fprintf(stderr, "%s\n", dlerror()); return 2; }
    auto descriptor = reinterpret_cast<const LV2_Descriptor* (*)(uint32_t)>(dlsym(module, "lv2_descriptor"));
    if (!descriptor) return 3;
    const LV2_Descriptor* d = descriptor(0);
    if (!d || descriptor(1) || std::strcmp(d->URI, argv[2]) != 0) return 4;
    // The metadata requires inPlaceBroken. A missing host feature must
    // fail before allocation; a supporting host must instantiate normally.
    const LV2_Feature* no_features[] = {nullptr};
    if (d->instantiate(d, 48000.0, "", no_features)) return 15;
    const LV2_Feature in_place_broken{
        "http://lv2plug.in/ns/lv2core#inPlaceBroken", nullptr
    };
    const LV2_Feature* features[] = {&in_place_broken, nullptr};
    if (d->instantiate(d, std::numeric_limits<double>::quiet_NaN(), "", features)) return 5;
    LV2_Handle h = d->instantiate(d, 48000.0, "", features);
    if (!h) return 6;
    constexpr std::size_t n = 8193; // larger than the prepared block size
    std::vector<float> a(n, 0.0F), b(n, 0.0F), x(n), y(n);
    d->connect_port(h, 0, a.data());
    d->connect_port(h, 1, b.data());
    d->connect_port(h, 2, x.data());
    d->connect_port(h, 3, y.data());
    const bool delay = std::strcmp(argv[3], "delay") == 0;
    float c4 = delay ? 10.0F : 4.0F;
    float c5 = 0.0F, c6 = delay ? 1.0F : 0.0F;
    float c7 = 120.0F, c8 = 0.0F;
    d->connect_port(h, 4, &c4);
    d->connect_port(h, 5, &c5);
    d->connect_port(h, 6, &c6);
    if (delay) {
        d->connect_port(h, 7, &c7);
        d->connect_port(h, 8, &c8);
        a[0] = 1.0F; b[0] = 0.5F;
    } else {
        std::fill(a.begin(), a.end(), 1.0F);
        std::fill(b.begin(), b.end(), 0.5F);
    }
    d->activate(h);
    d->run(h, 0);
    d->run(h, static_cast<uint32_t>(n));
    if (delay) {
        if (std::fabs(x[480] - 1.0F) > 0.05F ||
            std::fabs(y[480] - 0.5F) > 0.05F) return 7;
    } else if (x != a || y != b) return 8;
    for (float v : x) if (!std::isfinite(v)) return 9;
    for (float v : y) if (!std::isfinite(v)) return 10;
    if (d->deactivate) d->deactivate(h);
    if (delay) c6 = 0.0F; else c5 = 1.0F; // wet mix vs tremolo depth
    std::fill(a.begin(), a.end(), 1.0F);
    std::fill(b.begin(), b.end(), 0.5F);
    d->activate(h);
    d->run(h, static_cast<uint32_t>(n));
    if (delay) {
        if (x != a || y != b) return 11;
    } else {
        bool changed = false;
        for (std::size_t i = 0; i < n; ++i) {
            if (std::fabs(x[i] - a[i]) > 0.05F) changed = true;
        }
        if (!changed) return 12;
    }
    c4 = std::numeric_limits<float>::quiet_NaN();
    c5 = std::numeric_limits<float>::infinity();
    d->run(h, static_cast<uint32_t>(n));
    for (float v : x) if (!std::isfinite(v)) return 13;
    for (float v : y) if (!std::isfinite(v)) return 14;
    // The wrapper must safely skip a callback with a disconnected output.
    std::fill(x.begin(), x.end(), 12.0F);
    d->connect_port(h, 2, nullptr);
    d->run(h, static_cast<uint32_t>(n));
    if (x[0] != 12.0F) return 16;
    d->connect_port(h, 2, x.data());

    // With disconnected inputs and a reset state, both channels must be silent.
    d->connect_port(h, 0, nullptr);
    d->connect_port(h, 1, nullptr);
    d->activate(h);
    d->run(h, static_cast<uint32_t>(n));
    for (float v : x) if (v != 0.0F) return 17;
    for (float v : y) if (v != 0.0F) return 18;
    d->connect_port(h, 0, a.data());
    d->connect_port(h, 1, b.data());

    // Crosses chunk boundaries, null/default control path, and DSP changes.
    d->connect_port(h, 4, nullptr);
    d->connect_port(h, 5, nullptr);
    d->activate(h);

    // Check that global new instrumentation works before probing the plugin.
    allocation_count.store(0, std::memory_order_relaxed);
    track_allocations.store(true, std::memory_order_release);
    void* probe = ::operator new(32);
    ::operator delete(probe);
    track_allocations.store(false, std::memory_order_release);
    if (allocation_count.load(std::memory_order_relaxed) != 1) return 19;

    // Linux executable exports operator new for interposition into the
    // dlopen() plugin. Allocation during run() must not occur.
    allocation_count.store(0, std::memory_order_relaxed);
    track_allocations.store(true, std::memory_order_release);
    for (int i = 0; i < 24; ++i)
        d->run(h, static_cast<uint32_t>(n));
    track_allocations.store(false, std::memory_order_release);
    if (allocation_count.load(std::memory_order_relaxed) != 0) return 20;

    if (d->deactivate) d->deactivate(h);
    d->cleanup(h);
    dlclose(module);
    return 0;
}
