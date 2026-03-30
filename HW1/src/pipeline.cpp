#include "pipeline.hpp"
#include "state.hpp"
#include "io_handler.hpp"

void propagate(SystemState& current_state) {
    // Implementation for propagating state through pipeline stages
    SystemState next_state = current_state; // Create a copy of the current state to modify for the next cycle
    commit(current_state, next_state);
}

void latch(SystemState& state) {
    // Implementation for advancing clock and latching new state
}

bool noInstruction(SystemState& state) {
    // Implementation for checking if there are no more instructions to fetch
    return state.DecodedPCs.empty();
}

bool activeListIsEmpty(SystemState& state) {
    // Implementation for checking if the Active List is empty
    return state.ActiveList.empty();
}

void commit(SystemState& current_state, SystemState& next_state) {
    // Implementation for the commit stage of the pipeline
    int instructionsToCommit = std::min(4, static_cast<int>(current_state.ActiveList.size())); // Commit up to 4 instructions
    for (int i = 0; i < instructionsToCommit; ++i) {

        ActiveListEntry examined_instruction = current_state.ActiveList[i];
        // Implementation for committing each instruction
        if (examined_instruction.Done == true) { // Mark the instruction as done in the current state

            instruction_commit(current_state, next_state, examined_instruction, i); // Call the instruction commit function to handle the specific commit logic for the instruction
        }
        else if (examined_instruction.Exception == true) { // Handle exceptions if any
            instruction_exception(current_state, next_state, examined_instruction, i); // Call the instruction exception function to handle the specific logic for exceptions
            break; // Stop committing further instructions if an exception is encountered
        }
    }
}

void instruction_commit(SystemState& current_state, SystemState& next_state, ActiveListEntry& examined_instruction, int i) {
    // Implementation for committing instructions
    next_state.BusyBitTable[current_state.RegisterMapTable[examined_instruction.LogicalDestination]] = false; // Clear the busy bit for the physical register
    next_state.FreeList.push_back(current_state.RegisterMapTable[examined_instruction.LogicalDestination]); // Add the physical register back to the Free List
    next_state.RegisterMapTable[examined_instruction.LogicalDestination] = examined_instruction.OldDestination; // Update the Register Map Table to point back to the old physical register
    

    next_state.ActiveList.erase(next_state.ActiveList.begin() + i); // Remove the instruction from the Active List in the next state
    next_state.PC = examined_instruction.PC + 1; // Increment the PC to point to the next instruction

}

void instruction_exception(SystemState& current_state, SystemState& next_state, ActiveListEntry& examined_instruction, int i) {
    next_state.Exception = true; // Set the exception flag in the next state
    next_state.ExceptionPC = examined_instruction.PC; // Set the Exception PC to the PC of the instruction that caused the exception
    next_state.PC = 10000; // PC set to 10000 to indicate that the processor should jump to the exception handler
}