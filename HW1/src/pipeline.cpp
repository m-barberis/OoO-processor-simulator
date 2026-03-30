#include "pipeline.hpp"
#include "state.hpp"
#include "io_handler.hpp"

void propagate() {
    // Implementation for propagating state through pipeline stages
}

void latch() {
    // Implementation for advancing clock and latching new state
}

bool noInstruction(SystemState& state) {
    // Implementation for checking if there are no more instructions to fetch
    return state.DecodedPCs.empty();
}

bool activeListIsEmpty(SystemState& state){ {
    // Implementation for checking if the Active List is empty
    return state.ActiveList.empty();
}