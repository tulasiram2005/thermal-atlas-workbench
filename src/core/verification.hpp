#pragma once
#include <string>
#include <vector>
namespace wb {
struct Check { std::string name; bool passed; double measured; std::string units; };
std::vector<Check> verify_modules();
}
