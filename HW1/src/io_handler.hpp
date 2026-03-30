#pragma once
#include <vector>
#include <string>
#include "nlohmann/json.hpp"
#include "state.hpp"

using json = nlohmann::json;

struct ParsedInstruction {
    std::string opcode;
    int dest;
    int opA;
    int opB;
    bool is_addi;
};

std::vector<std::string> loadProgram(const std::string& filepath);
ParsedInstruction decodeInstructionString(std::string inst_str);
json dumpStateToJson(const SystemState& state);
void saveSimulationLog(const std::vector<json>& simulationLog, const std::string& filepath);