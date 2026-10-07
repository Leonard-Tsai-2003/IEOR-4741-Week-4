// g++ -std=c++17 -O2 -Iinclude hw4_virtualCallCost.cpp -o /tmp/w4_vbench && /tmp/w4_vbench
#include <algorithm>
#include <cstdio>
#include <vector>
#include "bench.hpp"

struct RiskCheck {
    virtual ~RiskCheck() = default;
    virtual bool check(double price, int qty) = 0;
};

// Four distinct risk rules so the 4 targets represent genuinely different call sites
struct R0 final : RiskCheck { bool check(double p, int q) override { return (p * q) <= 10000.0; } };
struct R1 final : RiskCheck { bool check(double p, int q) override { return q <= 100; } };
struct R2 final : RiskCheck { bool check(double p, int q) override { return p >= 10.0 && q > 0; } };
struct R3 final : RiskCheck { bool check(double p, int q) override { return (p * q) > 500.0; } };

// Direct baseline: same body as R0, without virtual inheritance
struct DirectRisk { bool check(double p, int q) { return (p * q) <= 10000.0; } };

static double g_sink;                              // benchmark sink to prevent optimization

constexpr long N = 20'000'000;                     //

template <class F> double median_ns(F&& f, int reps = 7) { //
    std::vector<double> v;
    for (int i = 0; i < reps; ++i) v.push_back(ns_per_op(f, N));
    std::sort(v.begin(), v.end());
    return v[reps / 2];
}

int main() {
    R0 r0; R1 r1; R2 r2; R3 r3;
    RiskCheck* one = &r0;                          // single target
    RiskCheck* four[4] = {&r0, &r1, &r2, &r3};

    // Pattern table: 4096 pseudo-random target indices
    std::vector<unsigned char> pat(4096);
    unsigned x = 12345;
    for (auto& p : pat) { x = x * 1664525u + 1013904223u; p = (x >> 24) & 3; } //

    DirectRisk d;

    auto run = [&](auto&& body) { 
        double acc = 0.0; 
        long i = 0;
        return median_ns([&] { 
            double price = 100.0 + (i & 7) * 0.1;
            int qty = static_cast<int>(i & 127);
            acc += body(price, qty, i) ? 1.0 : 0.0; 
            ++i; 
            doNotOptimize(acc); 
            g_sink = acc; 
        }); 
    };

    double t_direct = run([&](double p, int q, long)   { return d.check(p, q); });
    RiskCheck* volatile hide = one;                // hides dynamic type from compiler
    double t_virt1  = run([&](double p, int q, long)   { return hide->check(p, q); });
    double t_virt4r = run([&](double p, int q, long i) { return four[pat[i & 4095]]->check(p, q); });
    double t_virt4p = run([&](double p, int q, long i) { return four[i & 3]->check(p, q); });
    R0* volatile hide0 = &r0;                     // R0 is `final`: compiler devirtualizes
    double t_final  = run([&](double p, int q, long)   { return hide0->check(p, q); });

    std::printf("direct call (no base class)    %6.2f ns/call\n", t_direct); //
    std::printf("virtual, 1 target              %6.2f ns/call\n", t_virt1);  //
    std::printf("virtual, 4 targets, i&3        %6.2f ns/call\n", t_virt4p); //
    std::printf("virtual, 4 targets, random     %6.2f ns/call\n", t_virt4r); //
    std::printf("final class, via R0*           %6.2f ns/call\n", t_final);  //
    std::printf("sink=%g\n", g_sink);              //
}