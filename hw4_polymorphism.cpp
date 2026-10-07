// g++ -std=c++17 -O2 -Iinclude hw4_polymorphism.cpp -o /tmp/w4_strat && /tmp/w4_strat

#include <cstdio>
#include <new>
#include <string>
#include "pool.hpp"

static int live = 0;                       // counts constructed-but-not-destroyed

struct RiskCheck {                         // abstract interface
    RiskCheck()  { ++live; }
    virtual ~RiskCheck() { --live; }        // virtual destructor ensures derived destructors run
    virtual bool check(double price, int qty) = 0;
};

struct PositionLimitCheck : RiskCheck {
    int max_qty = 100;

    bool check(double price, int qty) override {
        return qty <= max_qty;
    }
};

struct NotionalLimitCheck : RiskCheck {
    // String long enough to force dynamic heap allocation beyond SSO limits
    std::string rule_id = "notional_risk_rule_long_name_to_force_heap_allocation_in_sso_land";
    double max_notional = 10000.0;

    bool check(double price, int qty) override {
        return (price * static_cast<double>(qty)) <= max_notional;
    }
};

int main() {
    // Determine pool slot size to fit the largest derived type
    constexpr std::size_t SLOT = sizeof(NotionalLimitCheck) > sizeof(PositionLimitCheck)
                                 ? sizeof(NotionalLimitCheck)
                                 : sizeof(PositionLimitCheck);
    Pool pool(SLOT, 8);

    // Placement new allocation from pool slot
    RiskCheck* checks[2] = {
        new (pool.alloc()) PositionLimitCheck{},
        new (pool.alloc()) NotionalLimitCheck{}
    };

    // Invoke through base pointer
    for (RiskCheck* p : checks) {
        bool passed = p->check(150.00, 50);
        std::printf("risk check status: %s\n", passed ? "PASS" : "FAIL");
    }

    // Teardown: invoke virtual destructor explicitly, then return slot to pool
    for (RiskCheck* p : checks) {
        p->~RiskCheck();                    // virtual call cleans up derived members (e.g. rule_id string heap memory)
        pool.free(p);                      // recycle slot back to free list
    }

    std::printf("live objects after teardown: %d\n", live);
}