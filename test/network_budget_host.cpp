#include <cassert>
#include <cstdio>
#include "network_budget_core.h"
int main() {
    NetworkBudget budget; budget.begin(1000, 75000);
    assert(budget.remaining(11000) == 65000);
    assert(budget.phaseTimeout(11000) == 5000);
    assert(budget.phaseTimeout(72000) == 1000);
    assert(!budget.canRequest(72001));
    assert(budget.remaining(76000) == 0 && !budget.acceptBody(76000,0,1,12000));
    assert(!budget.acceptBody(2000,11999,2,12000));
    budget.begin(0xfffffff0u,100);
    assert(budget.remaining(10) == 74); // millis rollover
    puts("network budget: shared deadline, bounded phases, body limit and clock rollover passed");
}
