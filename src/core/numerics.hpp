#pragma once
#include <array>
#include <vector>

namespace wb {
struct Rod {
    int elements = 10;
    double length = 1, area = 1e-4, conductivity = 10, source = 10000;
    double left = 100, right = 20;
};
struct Solution {
    std::vector<double> x, temperature;
    double nodal_error = 0, midpoint_error = 0;
};
double exact(const Rod& rod, double x);
Solution solve(const Rod& rod, bool dense_reference = false);
using Parameters = std::array<double, 4>; // w, b, v, c
struct Network {
    double z, h, prediction, error, loss;
    Parameters gradient;
};
Network forward(const Parameters& p, double x = 1, double y = 2);
Parameters update(const Parameters& p, double rate = 0.1);
Parameters gradient_errors(const Parameters& p, double epsilon = 1e-6);
}
