#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace wb {
using Point = std::array<double,3>;
struct Mesh { std::vector<Point> vertices; Point low{}, high{}; unsigned degenerate=0; };
struct Node { std::int64_t id; Point position; double temperature; };
struct Field { std::vector<Node> nodes; double minimum=0, maximum=0, mean=0; };
Mesh parse_stl(std::string bytes);
Field parse_csv(std::string bytes);
}
