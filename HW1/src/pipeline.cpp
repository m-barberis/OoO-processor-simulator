#include "pipeline.hpp"
#include "state.hpp"
#include "io_handler.hpp"

void propagate(SystemState& current_state) {
    SystemState next_state = current_state;

    if (current_state.Exception) {
        // Exception recovery mode
        handle_exception_recovery(current_state, next_state);
    } else {
        // Normal mode (evaluate in reverse order to model combinational backward paths)
        commit(current_state, next_state);
        execute(current_state, next_state);
        issue(current_state, next_state);
        rename_and_dispatch(current_state, next_state);
        fetch_and_decode(current_state, next_state);
    }
}


void latch(SystemState& state) {
    // Implementation for advancing clock and latching new state
}

bool noInstruction(SystemState& state) {
    // Implementation for checking if there are no more instructions to fetch
    return state.DecodedPCs.empty() && state.PC == state.instructions.size() && activeListIsEmpty(state); //TODO: To check if correct 
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
        if (examined_instruction.Done == true && examined_instruction.Exception == true) { // Handle exceptions if any
            instruction_exception(current_state, next_state, examined_instruction); // Processor enters exception mode in the next cycle and the PC is set to 10000 to jump to the exception handler
            break; // Stop committing further instructions if an exception is encountered
        }
        else if (examined_instruction.Done == true) { // Mark the instruction as done in the current state
            instruction_commit(current_state, next_state, examined_instruction); // Call the instruction commit function to handle the specific commit logic for the instruction
        }
        else {
            break; // Stop committing further instructions if the current instruction is not done
        }
    }
}

void instruction_commit(SystemState& current_state, SystemState& next_state, ActiveListEntry& examined_instruction) {
    // Implementation for committing instructions
    // Only free the old destination physical register
    next_state.FreeList.push_back(examined_instruction.OldDestination);
    next_state.ActiveList.erase(next_state.ActiveList.begin()); // Remove the instruction from the Active List in the next state

}

void instruction_exception(SystemState& current_state, SystemState& next_state, ActiveListEntry& examined_instruction) {
    next_state.Exception = true; // Set the exception flag in the next state
    next_state.ExceptionPC = examined_instruction.PC; // Set the Exception PC to the PC of the instruction that caused the exception
    //TODO: We have to notify the fetch stage to stop fetching new instructions and to set the PC to 10000 to jump to the exception handler.
    //  This can be done by setting a flag in the next state that the fetch stage will check in the next cycle.
    // We can use the exception flag in the state already set to true.
}

void handle_exception_recovery(SystemState& current_state, SystemState& next_state) {
    // Implementation for exception recovery mode
}

void fetch_and_decode(SystemState& current_state, SystemState& next_state) {
    // Implementation for fetch and decode stage
    
    if (next_state.Exception == true) {
        next_state.PC = 10000; // Set the PC to 10000 to jump to the exception handler
        next_state.IntegerQueue.clear(); // Clear the Integer Queue in the next state
    }
    else if (current_state.backpressure_on == true) {
        // Do nothing, wait for the backpressure to be released
    }
    else {
        // Fetch up to 4 instructions, bounded by how many can fit in the 4-entry queue
        int fetchCapacity = 4 - next_state.DecodedInstructionQueue.size();
        if (fetchCapacity > 0) {
            int instructionsToFetch = std::min(fetchCapacity, static_cast<int>(current_state.instructions.size() - current_state.PC));
            for (int i = 0; i < instructionsToFetch; ++i) {
                ParsedInstruction parsed_instruction = current_state.instructions[current_state.PC + i];
                
                DecodedInstruction di;
                di.PC = current_state.PC + i;
                di.inst = parsed_instruction;
                
                next_state.DecodedInstructionQueue.push_back(di);
            }
            next_state.PC += instructionsToFetch;
        }
    }

    // Always update DecodedPCs to strictly represent the content of DecodedInstructionQueue
    next_state.DecodedPCs.clear();
    for (const auto& di : next_state.DecodedInstructionQueue) {
        next_state.DecodedPCs.push_back(di.PC);
    }
}

void rename_and_dispatch(SystemState& current_state, SystemState& next_state) {
    // Implementation for rename and dispatch stage
    bool backpressure_on = (next_state.ActiveList.size() == 32 && next_state.FreeList.size() == 0 && next_state.IntegerQueue.size() == 32);
    next_state.backpressure_on = backpressure_on;

    if (backpressure_on == true) {
        // Do nothing, wait for the backpressure to be released
    }
    else {
        // Dispatch up to 4 instructions from the Decoded Instruction Queue
        int instructionsToDispatch = std::min(4, static_cast<int>(current_state.DecodedInstructionQueue.size()));
        
        // Remove processed instructions from next_state's queue
        next_state.DecodedInstructionQueue.erase(
            next_state.DecodedInstructionQueue.begin(), 
            next_state.DecodedInstructionQueue.begin() + instructionsToDispatch
        );

        for (int i = 0; i < instructionsToDispatch; ++i) {
            DecodedInstruction di = current_state.DecodedInstructionQueue[i];
            ParsedInstruction parsed_instruction = di.inst;
            IntegerQueueEntry decoded_instruction;

            decoded_instruction.OpCode = parsed_instruction.opcode;
            decoded_instruction.PC = di.PC;
            decoded_instruction.DestRegister = parsed_instruction.dest;
            decoded_instruction.OpAValue = parsed_instruction.opA;

            if (parsed_instruction.is_addi) {
                //TODO
            }
            else {
                decoded_instruction.OpBValue = parsed_instruction.opB;
            }
            next_state.IntegerQueue.push_back(decoded_instruction);
        }
    }

}


void issue(SystemState& current_state, SystemState& next_state) {
    // Implementation for issue stage
}

void execute(SystemState& current_state, SystemState& next_state) {
    // Implementation for execute stage
}
