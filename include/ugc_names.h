#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace UgcNames {
struct Names {std::map<std::string,std::string> values;bool warning=false;};
Names parse(const std::vector<uint8_t>& bytes);
}
