#include "core/numerics.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2) { std::cerr<<"Usage: generate_samples OUTPUT_DIRECTORY\n"; return 2; }
    try {
        const std::filesystem::path dir(argv[1]); std::filesystem::create_directories(dir);
        std::ofstream stl(dir/"triangle.stl"),csv(dir/"thermal_rod.csv");
        stl<<"solid synthetic_triangle\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid synthetic_triangle\n";
        csv<<std::setprecision(17)<<"node_id,x_m,y_m,z_m,temperature_C\n";
        for(int i=0;i<=100;++i) { const double x=i/100.0; csv<<i<<','<<x<<",0,0,"<<wb::exact(wb::Rod{},x)<<'\n'; }
        if(!stl||!csv) return 1;
        return 0;
    } catch(const std::exception&) { std::cerr<<"Sample generation failed: check output folder permissions.\n"; return 1; }
}
