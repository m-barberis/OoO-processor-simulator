#include "io_handler.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

// INPUT FUNCTIONS

// Extracts the list of instruction strings from the input JSON file
std::vector<std::string> loadProgram(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: unable to open " << filepath << std::endl;
        exit(1);
    }
    json j;
    file >> j;
    
    std::vector<std::string> instructions;
    for (const auto& item : j) {
        instructions.push_back(item.get<std::string>());
    }
    return instructions;
}

// Decodes a single instruction string (e.g., "addi x1, x2, 10") into a ParsedInstruction struct
ParsedInstruction decodeInstructionString(std::string inst_str) {
    ParsedInstruction inst;
    
    // Remove commas to facilitate reading with stringstream
    std::replace(inst_str.begin(), inst_str.end(), ',', ' ');
    
    std::stringstream ss(inst_str);
    std::string dest_str, opA_str, opB_str;
    
    ss >> inst.opcode >> dest_str >> opA_str >> opB_str;
    
    // Removes the x prefix from register names and converts to integer. For example, "x1" becomes 1.
    // std::stoi converts the substring starting from index 1 (after 'x') to an integer with substr(1)
    inst.dest = std::stoi(dest_str.substr(1));
    inst.opA = std::stoi(opA_str.substr(1));
    
    if (inst.opcode == "addi") {
        inst.opB = std::stoi(opB_str); // Immediate value, no need to remove 'x'
        inst.is_addi = true;
    } else {
        inst.opB = std::stoi(opB_str.substr(1)); // It's a register, remove the 'x'
        inst.is_addi = false;
    }
    
    return inst;
}


// OUTPUT FUNCTIONS

// Converts the SystemState in the JSON required for each cycle
json dumpStateToJson(const SystemState& state) {
    json j;
    
    j["PC"] = state.PC;
    j["PhysicalRegisterFile"] = state.PhysicalRegisterFile;
    j["DecodedPCs"] = state.DecodedPCs;
    j["Exception"] = state.Exception;
    j["ExceptionPC"] = state.ExceptionPC;
    j["RegisterMapTable"] = state.RegisterMapTable;
    j["FreeList"] = state.FreeList;
    j["BusyBitTable"] = state.BusyBitTable;
    
    // We manually construct the JSON array for the active list entries 
    json activeListJson = json::array();
    for (const auto& entry : state.ActiveList) {
        activeListJson.push_back({
            {"Done", entry.Done},
            {"Exception", entry.Exception},
            {"LogicalDestination", entry.LogicalDestination},
            {"OldDestination", entry.OldDestination},
            {"PC", entry.PC}
        });
    }
    j["ActiveList"] = activeListJson;
    
    // We manually construct the JSON array for the integer queue entries 
    json integerQueueJson = json::array();
    for (const auto& entry : state.IntegerQueue) {
        integerQueueJson.push_back({
            {"DestRegister", entry.DestRegister},
            {"OpAIsReady", entry.OpAIsReady},
            {"OpARegTag", entry.OpARegTag},
            {"OpAValue", entry.OpAValue},
            {"OpBIsReady", entry.OpBIsReady},
            {"OpBRegTag", entry.OpBRegTag},
            {"OpBValue", entry.OpBValue},
            {"OpCode", entry.OpCode},
            {"PC", entry.PC}
        });
    }
    j["IntegerQueue"] = integerQueueJson;
    
    return j;
}

// Saves the entire simulation log (vector of JSON states) to the output file in a pretty-printed format
void saveSimulationLog(const std::vector<json>& simulationLog, const std::string& filepath) {
    std::ofstream file(filepath);
    if (file.is_open()) {
        // Parameter 4 in dump() specifies the indentation level for pretty-printing the JSON output
        file << json(simulationLog).dump(4) << std::endl;
    } else {
        std::cerr << "Error: unable to save output to " << filepath << std::endl;
    }
}