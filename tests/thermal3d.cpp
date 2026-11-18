#include "doctest.h"
#include "core/thermal3d.hpp"
#include <cmath>
TEST_CASE("3D cube reproduces analytical linear temperature in all methods"){
    auto grid=wb::thermal_grid(6,false,0);
    for(auto method:{wb::ThermalMethod::Jacobi,wb::ThermalMethod::GaussSeidel,wb::ThermalMethod::PCG}){
        auto run=wb::thermal_solve(grid,method,1e-10);
        REQUIRE(run.converged);
        for(std::size_t i=0;i<grid.cells.size();++i)
            CHECK(std::abs(run.temperature[i]-(100-80*(grid.cells[i][0]+0.5)*grid.h))<1e-6);
    }
}
TEST_CASE("Constricted 3D domain conserves heat and solvers agree"){
    auto g=wb::thermal_grid(6,true);auto reference=wb::thermal_solve(g,wb::ThermalMethod::PCG,1e-11);
    REQUIRE(reference.converged);CHECK(g.cells.size()<216);
    for(auto method:{wb::ThermalMethod::Jacobi,wb::ThermalMethod::GaussSeidel}){
        auto run=wb::thermal_solve(g,method,1e-10);REQUIRE(run.converged);
        CHECK(std::abs(run.generated_heat-run.outward_heat)/run.generated_heat<1e-7);
        for(std::size_t i=0;i<g.cells.size();++i)CHECK(std::abs(run.temperature[i]-reference.temperature[i])<1e-4);
    }
}
TEST_CASE("Nonconvergence and invalid controls remain explicit"){
    auto g=wb::thermal_grid(6,true);
    CHECK_FALSE(wb::thermal_solve(g,wb::ThermalMethod::Jacobi,1e-12,1).converged);
    CHECK_THROWS(wb::thermal_grid(0,false));
    CHECK_THROWS(wb::thermal_solve(g,wb::ThermalMethod::PCG,0));
}
TEST_CASE("Cube mesh refinement converges quadratically against continuum solution"){
    double previous=0;
    for(int n:{3,6,12}){
        auto g=wb::thermal_grid(n,false);auto r=wb::thermal_solve(g,wb::ThermalMethod::PCG,1e-12);
        REQUIRE(r.converged);double error=0;
        for(std::size_t i=0;i<g.cells.size();++i){
            double x=(g.cells[i][0]+0.5)*g.h;
            error=std::max(error,std::abs(r.temperature[i]-(100-80*x+500*x*(1-x))));
        }
        CHECK(error==doctest::Approx(125.0/(n*n)).epsilon(1e-8));
        if(previous>0)CHECK(previous/error==doctest::Approx(4).epsilon(1e-8));
        previous=error;
    }
}
TEST_CASE("Sparse thermal solve agrees with independent dense Gaussian elimination"){
    auto g=wb::thermal_grid(3,true);
    const auto n=g.cells.size();std::vector<std::vector<double>> a(n,std::vector<double>(n+1));
    for(std::size_t i=0;i<n;++i){a[i][i]=g.diagonal[i];a[i][n]=g.rhs[i];
        for(int j:g.neighbors[i])if(j>=0)a[i][static_cast<std::size_t>(j)]-=g.conductance;}
    for(std::size_t k=0;k<n;++k){std::size_t pivot=k;
        for(std::size_t i=k+1;i<n;++i)if(std::abs(a[i][k])>std::abs(a[pivot][k]))pivot=i;
        std::swap(a[k],a[pivot]);REQUIRE(std::abs(a[k][k])>1e-12);
        for(std::size_t i=k+1;i<n;++i){double factor=a[i][k]/a[k][k];
            for(std::size_t j=k;j<=n;++j)a[i][j]-=factor*a[k][j];}}
    std::vector<double> exact(n);
    for(std::size_t ii=n;ii-->0;){double v=a[ii][n];for(std::size_t j=ii+1;j<n;++j)v-=a[ii][j]*exact[j];exact[ii]=v/a[ii][ii];}
    for(auto method:{wb::ThermalMethod::Jacobi,wb::ThermalMethod::GaussSeidel,wb::ThermalMethod::PCG}){
        auto r=wb::thermal_solve(g,method,1e-11);REQUIRE(r.converged);
        for(std::size_t i=0;i<n;++i)CHECK(std::abs(exact[i]-r.temperature[i])<1e-6);
    }
}
