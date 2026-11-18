#include "core/thermal3d.hpp"
#include "json.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <map>
int main(){
    using namespace wb;
    const ThermalMethod methods[]={ThermalMethod::Jacobi,ThermalMethod::GaussSeidel,ThermalMethod::PCG};
    const char* names[]={"Jacobi","Gauss-Seidel","Diagonal PCG"};
    nlohmann::json study=nlohmann::json::array(),mesh=nlohmann::json::array();
    for(bool neck:{false,true}){
        std::vector<double> previous;std::vector<std::array<int,3>> old_cells;
        for(int n:{6,12,24}){
            auto g=thermal_grid(n,neck);auto reference=thermal_solve(g,ThermalMethod::PCG,1e-12);
            if(!reference.converged)return 2;
            const auto& t=reference.temperature;
            nlohmann::json m={{"geometry",neck?"constricted":"cube"},{"n",n},{"cells",t.size()},
                {"peak_C",*std::max_element(t.begin(),t.end())},
                {"mean_C",std::accumulate(t.begin(),t.end(),0.0)/static_cast<double>(t.size())},
                {"heat_balance_relative",std::abs(reference.outward_heat-reference.generated_heat)/reference.generated_heat}};
            if(!previous.empty()){
                std::map<std::array<int,3>,std::pair<double,int>> averages;
                for(std::size_t i=0;i<t.size();++i){auto c=g.cells[i];for(auto& v:c)v/=2;auto& a=averages[c];a.first+=t[i];++a.second;}
                double square=0,max_error=0;
                for(std::size_t i=0;i<previous.size();++i){auto a=averages.at(old_cells[i]);if(a.second!=8)return 3;
                    double d=previous[i]-a.first/a.second;square+=d*d;max_error=std::max(max_error,std::abs(d));}
                m["coarse_to_restricted_fine_rms_C"]=std::sqrt(square/static_cast<double>(previous.size()));
                m["coarse_to_restricted_fine_max_C"]=max_error;
            }
            mesh.push_back(m);previous=t;old_cells=g.cells;
            std::vector<double> samples[3];int batches[3];ThermalRun runs[3];
            for(int k=0;k<3;++k){runs[k]=thermal_solve(g,methods[k]);
                batches[k]=std::clamp(static_cast<int>(std::ceil(50/std::max(.001,runs[k].elapsed_ms))),1,1000);}
            // Rotate order each repetition, use calibrated batches to reduce timer quantization.
            for(int rep=0;rep<5;++rep)for(int offset=0;offset<3;++offset){int k=(offset+rep)%3;
                const auto begin=std::chrono::steady_clock::now();
                for(int repeat=0;repeat<batches[k];++repeat)runs[k]=thermal_solve(g,methods[k]);
                double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
                samples[k].push_back(ms/batches[k]);}
            for(int k=0;k<3;++k){auto sorted=samples[k];std::sort(sorted.begin(),sorted.end());double diff=0;
                for(std::size_t i=0;i<t.size();++i)diff=std::max(diff,std::abs(t[i]-runs[k].temperature[i]));
                study.push_back({{"geometry",neck?"constricted":"cube"},{"n",n},{"cells",t.size()},
                    {"method",names[k]},{"repeats_per_batch",batches[k]},{"samples_ms",samples[k]},
                    {"median_ms",sorted[2]},{"min_ms",sorted.front()},{"max_ms",sorted.back()},
                    {"converged",runs[k].converged},{"iterations",runs[k].iterations},
                    {"relative_residual",runs[k].relative_residual},{"max_difference_reference_C",diff},
                    {"solution_sum_C",std::accumulate(runs[k].temperature.begin(),runs[k].temperature.end(),0.0)}});
            }
        }
    }
    std::cout<<nlohmann::json({{"benchmark_method","single thread; warmup/calibration; five batches; rotating solver order; total solve includes allocation and true-residual checks"},
        {"environment_control","not certified quiet; inspect external host metadata"},{"comparison_tolerance",1e-8},
        {"reference_tolerance",1e-12},{"mesh",mesh},{"study",study}}).dump(2)<<'\n';
}
