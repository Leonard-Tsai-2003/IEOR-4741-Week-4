#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <numeric>
#include <vector>

// --- Benchmark Harness Engine ---
struct Stats {
    double p50 = 0, p99 = 0, p999 = 0, mean = 0, min = 0;
    int    samples = 0;
    long   batch = 0;
};

template <class F>
Stats bench(F&& f, long batch, int samples = 1000, int warmup = 50) {
    for (int w = 0; w < warmup; ++w) f();

    std::vector<double> ns;
    ns.reserve(static_cast<std::size_t>(samples));
    for (int s = 0; s < samples; ++s) {
        auto t0 = std::chrono::steady_clock::now();
        f();
        auto t1 = std::chrono::steady_clock::now();
        clobber();
        ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count()
                     / static_cast<double>(batch));
    }
    std::sort(ns.begin(), ns.end());

    auto pct = [&](double p) {
        std::size_t i = static_cast<std::size_t>(p * static_cast<double>(ns.size() - 1) + 0.5);
        return ns[i];
    };
    Stats st;
    st.p50   = pct(0.50);
    st.p99   = pct(0.99);
    st.p999  = pct(0.999);
    st.min   = ns.front();
    st.mean  = std::accumulate(ns.begin(), ns.end(), 0.0) / static_cast<double>(ns.size());
    st.samples = samples;
    st.batch   = batch;
    return st;
}