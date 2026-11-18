#include "core/thermal3d.hpp"
#include "json.hpp"
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif
extern "C" EXPORT const char* thermal_compare(int n, int neck) {
    static std::string output;
    try {
        if ((n!=3 && n!=6 && n!=9 && n!=12) || (neck!=0 && neck!=1))
            throw std::invalid_argument("Choose resolution 3, 6, 9 or 12 and a valid geometry");
        const auto grid=wb::thermal_grid(n,neck!=0);
        nlohmann::json runs=nlohmann::json::array();
        for(auto method:{wb::ThermalMethod::Jacobi,wb::ThermalMethod::GaussSeidel,wb::ThermalMethod::PCG}) {
            const auto r=wb::thermal_solve(grid,method);
            const char* name=method==wb::ThermalMethod::Jacobi?"Jacobi":method==wb::ThermalMethod::GaussSeidel?"Gauss-Seidel":"Diagonal PCG";
            runs.push_back({{"method",name},{"temperature",r.temperature},{"iterations",r.iterations},
                {"converged",r.converged},{"relative_residual",r.relative_residual},{"elapsed_ms",r.elapsed_ms},
                {"outward_heat",r.outward_heat},{"generated_heat",r.generated_heat},{"residual_history",r.residual_history}});
        }
        output=nlohmann::json({{"n",n},{"neck",neck!=0},{"cells",grid.cells},{"runs",runs}}).dump();
    } catch(const std::exception& e) {output=nlohmann::json({{"error",e.what()}}).dump();}
    return output.c_str();
}
#ifdef THERMAL_NATIVE_BRIDGE
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    std::cout<<thermal_compare(std::stoi(argv[1]),std::stoi(argv[2]))<<'\n';
}
#endif
