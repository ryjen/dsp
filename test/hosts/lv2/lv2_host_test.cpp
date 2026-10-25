#include <lv2/core/lv2.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <limits>
#include <vector>

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
    if (d->deactivate) d->deactivate(h);
    d->cleanup(h);
    dlclose(module);
    return 0;
}
