#pragma once
#include <array>
#include <vector>
namespace wb {
enum class ThermalMethod { Jacobi, GaussSeidel, PCG };
struct ThermalGrid {
    int n=0;
    double h=0, conductance=0, source=0;
    std::vector<std::array<int,3>> cells;
    std::vector<std::array<int,6>> neighbors;
    std::vector<double> diagonal, rhs;
};
struct ThermalRun {
    std::vector<double> temperature, residual_history;
    int iterations=0;
    bool converged=false;
    double relative_residual=0, elapsed_ms=0, outward_heat=0, generated_heat=0;
};
ThermalGrid thermal_grid(int n, bool neck, double source=10000);
ThermalRun thermal_solve(const ThermalGrid&, ThermalMethod, double tolerance=1e-8,
                         int max_iterations=20000);
}
