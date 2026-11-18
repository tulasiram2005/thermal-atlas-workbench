#include "core/thermal3d.hpp"
#include "json.hpp"
#include <algorithm>
#include <iostream>
#include <chrono>
int main(){
    nlohmann::json rows=nlohmann::json::array();
    for(bool neck:{false,true})for(int n:{6,12,18}){
        const auto begin=std::chrono::steady_clock::now();const auto grid=wb::thermal_grid(n,neck);
        const double assembly=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        for(auto method:{wb::ThermalMethod::Jacobi,wb::ThermalMethod::GaussSeidel,wb::ThermalMethod::PCG}){
            (void)wb::thermal_solve(grid,method); // untimed warm-up
            std::vector<double> times;wb::ThermalRun run;
            for(int rep=0;rep<5;++rep){run=wb::thermal_solve(grid,method);times.push_back(run.elapsed_ms);}
            const auto raw=times;std::sort(times.begin(),times.end());
            const char* name=method==wb::ThermalMethod::Jacobi?"Jacobi":method==wb::ThermalMethod::GaussSeidel?"Gauss-Seidel":"Diagonal PCG";
            rows.push_back({{"geometry",neck?"constricted":"cube"},{"n",n},{"cells",grid.cells.size()},
                {"method",name},{"assembly_ms",assembly},{"solve_ms",raw},{"median_ms",times[2]},
                {"iterations",run.iterations},{"converged",run.converged},{"relative_residual",run.relative_residual},
                {"outward_heat_W",run.outward_heat},{"generated_heat_W",run.generated_heat},
                {"temperature",run.temperature},{"cells_xyz",grid.cells},{"residual_history",run.residual_history}});
        }
    }
    std::cout<<nlohmann::json({{"scope","development Mac; single-thread solvers; warm-up plus five repetitions"},
        {"discretization","cell-centered finite volume; not tetrahedral FEM"},{"tolerance",1e-8},{"rows",rows}}).dump(2)<<'\n';
}
