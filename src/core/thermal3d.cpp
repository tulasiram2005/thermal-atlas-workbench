#include "thermal3d.hpp"
#include <cmath>
#include <chrono>
#include <stdexcept>
#include <numeric>
namespace wb {
namespace {
double dot(const std::vector<double>& a,const std::vector<double>& b){
    return std::inner_product(a.begin(),a.end(),b.begin(),0.0);
}
std::vector<double> multiply(const ThermalGrid& g,const std::vector<double>& x){
    std::vector<double> y(x.size());
    for(std::size_t i=0;i<x.size();++i){
        y[i]=g.diagonal[i]*x[i];
        for(int j:g.neighbors[i]) if(j>=0)y[i]-=g.conductance*x[static_cast<std::size_t>(j)];
    }
    return y;
}
}
ThermalGrid thermal_grid(int n,bool neck,double source){
    if(n<3||n>32||!std::isfinite(source)||std::abs(source)>1e7)
        throw std::invalid_argument("Grid n must be 3..32; finite bounded source required");
    ThermalGrid g;g.n=n;g.h=1.0/n;g.conductance=10*g.h;g.source=source;
    std::vector<int> ids(static_cast<std::size_t>(n*n*n),-1);
    auto index=[n](int x,int y,int z){return static_cast<std::size_t>((z*n+y)*n+x);};
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        if(neck&&x>=n/3&&x<2*n/3&&!(y>=n/3&&y<2*n/3&&z>=n/3&&z<2*n/3))continue;
        ids[index(x,y,z)]=static_cast<int>(g.cells.size());g.cells.push_back({x,y,z});
    }
    const int dx[6]={-1,1,0,0,0,0},dy[6]={0,0,-1,1,0,0},dz[6]={0,0,0,0,-1,1};
    for(auto c:g.cells){
        std::array<int,6> links;links.fill(-1);double diagonal=0,rhs=source*g.h*g.h*g.h;
        for(int d=0;d<6;++d){
            int x=c[0]+dx[d],y=c[1]+dy[d],z=c[2]+dz[d];
            if(x>=0&&x<n&&y>=0&&y<n&&z>=0&&z<n){
                links[static_cast<std::size_t>(d)]=ids[index(x,y,z)];
                if(ids[index(x,y,z)]>=0)diagonal+=g.conductance;
            }else if(d<2){
                diagonal+=2*g.conductance;rhs+=2*g.conductance*(d==0?100:20);
            }
        }
        g.neighbors.push_back(links);g.diagonal.push_back(diagonal);g.rhs.push_back(rhs);
    }
    return g;
}
ThermalRun thermal_solve(const ThermalGrid& g,ThermalMethod method,double tol,int limit){
    if(g.cells.empty()||!std::isfinite(tol)||tol<=0||tol>=1||limit<1||limit>100000)
        throw std::invalid_argument("Invalid solver controls");
    ThermalRun out;out.temperature.assign(g.cells.size(),0);
    const auto start=std::chrono::steady_clock::now();
    const double norm=std::sqrt(dot(g.rhs,g.rhs));
    std::vector<double> r=g.rhs,z(r.size()),p(r.size());
    for(std::size_t i=0;i<r.size();++i)z[i]=r[i]/g.diagonal[i];
    p=z;double rz=dot(r,z);
    out.residual_history.push_back(1);
    for(int iteration=1;iteration<=limit;++iteration){
        if(method==ThermalMethod::PCG){
            const auto ap=multiply(g,p);const double pap=dot(p,ap);
            if(!(pap>0)||!std::isfinite(pap))break;
            const double alpha=rz/pap;
            for(std::size_t i=0;i<r.size();++i){out.temperature[i]+=alpha*p[i];r[i]-=alpha*ap[i];}
        }else{
            const auto old=out.temperature;
            for(std::size_t i=0;i<r.size();++i){
                double value=g.rhs[i];
                for(int j:g.neighbors[i])if(j>=0)value+=g.conductance*
                    (method==ThermalMethod::Jacobi?old:out.temperature)[static_cast<std::size_t>(j)];
                out.temperature[i]=value/g.diagonal[i];
            }
        }
        // True residual, independently recomputed for every method and every iteration.
        const auto ax=multiply(g,out.temperature);
        std::vector<double> true_r(r.size());
        for(std::size_t i=0;i<r.size();++i)true_r[i]=g.rhs[i]-ax[i];
        out.relative_residual=std::sqrt(dot(true_r,true_r))/norm;
        out.residual_history.push_back(out.relative_residual);out.iterations=iteration;
        if(out.relative_residual<=tol){out.converged=true;break;}
        if(method==ThermalMethod::PCG){
            for(std::size_t i=0;i<r.size();++i)z[i]=r[i]/g.diagonal[i];
            double next=dot(r,z),beta=next/rz;rz=next;
            for(std::size_t i=0;i<r.size();++i)p[i]=z[i]+beta*p[i];
        }
    }
    out.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    out.generated_heat=g.source*g.h*g.h*g.h*static_cast<double>(g.cells.size());
    for(std::size_t i=0;i<g.cells.size();++i){
        if(g.cells[i][0]==0)out.outward_heat+=2*g.conductance*(out.temperature[i]-100);
        if(g.cells[i][0]==g.n-1)out.outward_heat+=2*g.conductance*(out.temperature[i]-20);
    }
    return out;
}
}
