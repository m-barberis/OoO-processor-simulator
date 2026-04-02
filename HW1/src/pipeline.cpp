#include "pipeline.hpp"
#include "state.hpp"
#include "io_handler.hpp"
#include <algorithm>

void propagate(SystemState& current_state) {
    SystemState next_state = current_state;

    if (current_state.Exception) {
        // Exception recovery mode
        handle_exception_recovery(current_state, next_state);
    } else {
        // Normal mode (evaluate in reverse order to model combinational backward paths)
        commit(current_state, next_state);
        execute(current_state, next_state);
        issue(next_state);
        rename_and_dispatch(current_state, next_state);
        fetch_and_decode(current_state, next_state);
    }

    // advance clock, start next cycle
    latch(current_state, next_state);
}


void latch(SystemState& current_state, SystemState& next_state) {
    // Implementation for advancing clock and latching new state
    current_state = next_state;
}

bool noInstruction(SystemState& state) {
    // Implementation for checking if there are no more instructions to fetch
    return state.DecodedPCs.empty() && state.PC >= state.instructions.size() && activeListIsEmpty(state) && !state.Exception; // Must also wait for exception recovery to fully complete (Exception transitions to false)
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
            instruction_exception(next_state, examined_instruction); // Processor enters exception mode in the next cycle and the PC is set to 10000 to jump to the exception handler
            break; // Stop committing further instructions if an exception is encountered
        }
        else if (examined_instruction.Done == true) { // Mark the instruction as done in the current state
            instruction_commit(next_state, examined_instruction); // Call the instruction commit function to handle the specific commit logic for the instruction
        }
        else {
            break; // Stop committing further instructions if the current instruction is not done
        }
    }
}

void instruction_commit(SystemState& next_state, ActiveListEntry& examined_instruction) {
    // Implementation for committing instructions
    // Only free the old destination physical register
    next_state.FreeList.push_back(examined_instruction.OldDestination);
    next_state.ActiveList.erase(next_state.ActiveList.begin()); // Remove the instruction from the Active List in the next state

}

void instruction_exception(SystemState& next_state, ActiveListEntry& examined_instruction) {
    next_state.Exception = true; // Set the exception flag in the next state
    next_state.ExceptionPC = examined_instruction.PC; // Set the Exception PC to the PC of the instruction that caused the exception

    // Clear ALL in-flight instruction buffers so noInstruction() works correctly during recovery
    next_state.IntegerQueue.clear();
    next_state.ReadyInstructions.clear();
    next_state.ExecutionQueue.clear();
    next_state.DecodedInstructionQueue.clear(); // BUG FIX: must clear this or noInstruction() stays false
    next_state.DecodedPCs.clear();              // BUG FIX: must clear this or noInstruction() stays false
}

void handle_exception_recovery(SystemState& current_state, SystemState& next_state) {
    // Implementation for exception recovery mode
    if (!current_state.ActiveList.empty()) {
        // There are still entries to drain — unroll up to 4 per cycle
        int instructionsToRestore = std::min(4, static_cast<int>(next_state.ActiveList.size()));

        for (int i = 0; i < instructionsToRestore; ++i) {
            // Unroll instructions in reverse program order (newest first = back of the Active List)
            ActiveListEntry instruction = next_state.ActiveList.back();
            
            // Get the physical register that this instruction allocated
            int current_inst_physical_destination = next_state.RegisterMapTable[instruction.LogicalDestination];
            
            // Free the physical register that was allocated by this instruction
            next_state.FreeList.push_back(current_inst_physical_destination);
            
            // Restore the Register Map Table to point to the previous physical register
            next_state.RegisterMapTable[instruction.LogicalDestination] = instruction.OldDestination;
            
            // The physical register returned to the FreeList should no longer be busy 
            next_state.BusyBitTable[current_inst_physical_destination] = false;

            // Remove the instruction from the Active List
            next_state.ActiveList.pop_back();
        }
        // NOTE: Do NOT clear Exception here even if AL is now empty.
        // The reference requires one extra cycle with Exception=True + empty AL
        // before transitioning back to normal mode.
    } else {
        // The AL was already empty at the START of this cycle (current_state).
        // This is the extra cycle after draining — now we can exit exception mode.
        next_state.Exception = false;
        // ExceptionPC is intentionally preserved
    }
}

void fetch_and_decode(SystemState& current_state, SystemState& next_state) {
    // Implementation for fetch and decode stage
    
    if (next_state.Exception == true) {
        next_state.PC = 0x10000; // Set the PC to 0x10000 (65536) to jump to the exception handler
        next_state.DecodedInstructionQueue.clear(); // Clear the Decoded Instruction Queue in the next state
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
    if (next_state.Exception) return;
    // Implementation for rename and dispatch stage
    bool backpressure_on = (next_state.ActiveList.size() > 28 || next_state.FreeList.size() < 4 || next_state.IntegerQueue.size() > 28); // Check if backpressure should be applied based on the specified conditions
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

            if (next_state.FreeList.empty()) {
                // If there are no free physical registers, we cannot dispatch more instructions
                break;
            }
            decoded_instruction.DestRegister = next_state.FreeList.front(); // Get a physical register from the Free List for the destination

            next_state.FreeList.erase(next_state.FreeList.begin()); // Remove the allocated physical register from the Free List
            uint32_t old_dest = next_state.RegisterMapTable[parsed_instruction.dest]; // Store old physical register before updating it

            // Read Operands BEFORE updating the Register Map Table for the destination!
            // Operand A
            decoded_instruction.OpARegTag = next_state.RegisterMapTable[parsed_instruction.opA]; // Get the value of operand A from the Physical Register File using the Register Map Table
            if (next_state.BusyBitTable[decoded_instruction.OpARegTag]) { //FORWARDING PATHS 
                decoded_instruction.OpAIsReady = false; // Operand A is not ready if the corresponding physical register is busy
            } else {
                decoded_instruction.OpAIsReady = true; // Operand A is ready if the corresponding physical register is not busy
                decoded_instruction.OpAValue = next_state.PhysicalRegisterFile[decoded_instruction.OpARegTag]; // Get the value of operand A from the Physical Register File
            }

            // Operand B
            if (!parsed_instruction.is_addi) {
                decoded_instruction.OpBRegTag = next_state.RegisterMapTable[parsed_instruction.opB]; // Get the value of operand B from the Physical Register File using the Register Map Table
                if (next_state.BusyBitTable[decoded_instruction.OpBRegTag]) { //FORWARDING PATHS 
                    decoded_instruction.OpBIsReady = false; // Operand B is not ready if the corresponding physical register is busy
                } else {
                    decoded_instruction.OpBIsReady = true; // Operand B is ready if the corresponding physical register is not busy
                    decoded_instruction.OpBValue = next_state.PhysicalRegisterFile[decoded_instruction.OpBRegTag]; // Get the value of operand B from the Physical Register File
                }
            }
            else {
                decoded_instruction.OpBIsReady = true; // Operand B is ready if it is an immediate value
                decoded_instruction.OpBValue = static_cast<uint64_t>(static_cast<int64_t>(parsed_instruction.opB)); // Get the immediate value for operand B sign extended

            }

            // NOW update the Register Map Table to map the logical destination register to the new physical register
            next_state.RegisterMapTable[parsed_instruction.dest] = decoded_instruction.DestRegister; 
            next_state.BusyBitTable[decoded_instruction.DestRegister] = true; // Mark the destination physical register as busy

            next_state.IntegerQueue.push_back(decoded_instruction); // Add the decoded instruction to the Integer Queue in the next state
            
            // Updating the Active List 
            ActiveListEntry active_list_entry;
            active_list_entry.PC = decoded_instruction.PC;
            active_list_entry.LogicalDestination = parsed_instruction.dest;
            active_list_entry.OldDestination = old_dest; // Store the old physical register mapping for the destination logical register in the Active List entry (respecting intra-cycle dependencies)
            next_state.ActiveList.push_back(active_list_entry);

        }
    }

}


void issue(SystemState& next_state) {
    if (next_state.Exception) return;
    // Implementation for issue stage
    next_state.ReadyInstructions.clear();
    
    updateIntegerQueue(next_state);
    std::vector<IntegerQueueEntry> ready_instructions;
    for (auto& instruction : next_state.IntegerQueue) {
        if (instruction.OpAIsReady && instruction.OpBIsReady) {
            ready_instructions.push_back(instruction);
        }
    }

    // Sort ready instructions by PC (crescent/ascending order -> oldest instructions first)
    std::sort(ready_instructions.begin(), ready_instructions.end(), 
        [](const IntegerQueueEntry& a, const IntegerQueueEntry& b) {
            return a.PC < b.PC;
        });

    int instructionsToIssue = std::min(4, static_cast<int>(ready_instructions.size()));
    for (int i = 0; i < instructionsToIssue; ++i) {
        IntegerQueueEntry instruction = ready_instructions[i];
        
        // Find the precise entry in IntegerQueue and remove it
        auto it = std::find_if(next_state.IntegerQueue.begin(), next_state.IntegerQueue.end(),
            [&instruction](const IntegerQueueEntry& entry) {
                return entry.PC == instruction.PC;
            });
        
        if (it != next_state.IntegerQueue.end()) {
            next_state.IntegerQueue.erase(it);
        }

        // Pass the instruction to the execute stage
        next_state.ReadyInstructions.push_back(instruction);
    }
}

void updateIntegerQueue(SystemState& next_state) {

    // Implementation for updating the Integer Queue (we are considering forwarding paths also here)
    for (auto& instruction : next_state.IntegerQueue) {
        if (!instruction.OpAIsReady) {
            if (!next_state.BusyBitTable[instruction.OpARegTag]) {
                instruction.OpAIsReady = true;
                instruction.OpAValue = next_state.PhysicalRegisterFile[instruction.OpARegTag];
            }
        }
        if (!instruction.OpBIsReady) {
            if (!next_state.BusyBitTable[instruction.OpBRegTag]) {
                instruction.OpBIsReady = true;
                instruction.OpBValue = next_state.PhysicalRegisterFile[instruction.OpBRegTag];
            }
        }
    }
}

void execute(SystemState& current_state, SystemState& next_state) {
    if (next_state.Exception) return;
    // Implementation for execute stage (has to take 2 clock cycles)
    
    // Clear next_state's ExecutionQueue because we are consuming its elements this cycle
    next_state.ExecutionQueue.clear();

    for (auto& instruction : current_state.ExecutionQueue) {
        bool caused_exception = false;

        // Calculate the result and write to Physical Register File
        if (instruction.OpCode == "add" || instruction.OpCode == "addi") {
            next_state.PhysicalRegisterFile[instruction.DestRegister] = instruction.OpAValue + instruction.OpBValue;
        } else if (instruction.OpCode == "sub") {
            next_state.PhysicalRegisterFile[instruction.DestRegister] = instruction.OpAValue - instruction.OpBValue;
        } else if (instruction.OpCode == "mulu") {
            next_state.PhysicalRegisterFile[instruction.DestRegister] = instruction.OpAValue * instruction.OpBValue;
        } else if (instruction.OpCode == "divu") {
            if (instruction.OpBValue != 0) {
                next_state.PhysicalRegisterFile[instruction.DestRegister] = instruction.OpAValue / instruction.OpBValue;
            }
            else {
                // Divide by zero: mark exception in Active List
                caused_exception = true;
                auto it = std::find_if(next_state.ActiveList.begin(), next_state.ActiveList.end(),
                    [&instruction](const ActiveListEntry& entry) {
                        return entry.PC == instruction.PC;
                    });
                if (it != next_state.ActiveList.end()) {
                    it->Exception = true;
                }
            }
        } else if (instruction.OpCode == "remu") {
            if (instruction.OpBValue != 0) { 
                next_state.PhysicalRegisterFile[instruction.DestRegister] = instruction.OpAValue % instruction.OpBValue;
            }
            else {
                // Remainder by zero: mark exception in Active List
                caused_exception = true;
                auto it = std::find_if(next_state.ActiveList.begin(), next_state.ActiveList.end(),
                    [&instruction](const ActiveListEntry& entry) {
                        return entry.PC == instruction.PC;
                    });
                if (it != next_state.ActiveList.end()) {
                    it->Exception = true;
                }
            }
        }
        
        // Only clear the busy bit if the instruction produced a valid result.
        // Exception-causing instructions leave the busy bit set (no valid result).
        if (!caused_exception) {
            next_state.BusyBitTable[instruction.DestRegister] = false;
        }

        // Mark the instruction as "Done" in the Active List (ROB) so it can commit later
        for (auto& al_entry : next_state.ActiveList) {
            if (al_entry.PC == instruction.PC) {
                al_entry.Done = true;
                break;
            }
        }
    }

    next_state.ExecutionQueue = current_state.ReadyInstructions; // To execute them at the next cycle
}
