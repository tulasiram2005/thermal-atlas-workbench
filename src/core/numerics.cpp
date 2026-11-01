#include "core/numerics.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wb {
double exact(const Rod& r, double x) {
    return r.left + (r.right-r.left)*x/r.length + r.source*x*(r.length-x)/(2*r.conductivity);
}
Solution solve(const Rod& r, bool dense) {
    if (r.elements < 2 || r.elements > 100000 || !std::isfinite(r.length) || r.length <= 0 ||
        !std::isfinite(r.area) || r.area <= 0 || !std::isfinite(r.conductivity) || r.conductivity <= 0 ||
        !std::isfinite(r.source) || !std::isfinite(r.left) || !std::isfinite(r.right))
        throw std::invalid_argument("Invalid rod parameters");
    const int n = r.elements-1;
    if (dense && n > 200) throw std::invalid_argument("Dense reference restricted to 200 unknowns");
    const double h = r.length/r.elements, stiffness = r.conductivity*r.area/h;
    if (!std::isfinite(stiffness) || stiffness <= 0) throw std::invalid_argument("Unrepresentable stiffness");
    std::vector<double> a(n, -stiffness), d(n, 2*stiffness), c(n, -stiffness);
    std::vector<double> rhs(n, r.source*r.area*h), t(n);
    // Eliminate both prescribed endpoint temperatures from interior equations.
    rhs.front() += stiffness*r.left;
    rhs.back() += stiffness*r.right;
    if (dense) {
        std::vector<std::vector<double>> matrix(n, std::vector<double>(n));
        for (int i=0;i<n;++i) {
            matrix[i][i]=d[i];
            if(i) matrix[i][i-1]=a[i];
            if(i+1<n) matrix[i][i+1]=c[i];
        }
        for(int k=0;k<n;++k) for(int i=k+1;i<n;++i) {
            const double factor=matrix[i][k]/matrix[k][k];
            for(int j=k;j<n;++j) matrix[i][j]-=factor*matrix[k][j];
            rhs[i]-=factor*rhs[k];
        }
        for(int i=n-1;i>=0;--i) {
            double sum=rhs[i];
            for(int j=i+1;j<n;++j) sum-=matrix[i][j]*t[j];
            t[i]=sum/matrix[i][i];
        }
    } else {
        for(int i=1;i<n;++i) {
            const double factor=a[i]/d[i-1];
            d[i]-=factor*c[i-1]; rhs[i]-=factor*rhs[i-1];
        }
        t.back()=rhs.back()/d.back();
        for(int i=n-2;i>=0;--i) t[i]=(rhs[i]-c[i]*t[i+1])/d[i];
    }
    Solution out;
    out.temperature.push_back(r.left);
    out.temperature.insert(out.temperature.end(),t.begin(),t.end());
    out.temperature.push_back(r.right);
    for(int i=0;i<=r.elements;++i) {
        const double x=i*h;
        if(!std::isfinite(out.temperature[i])) throw std::invalid_argument("Non-finite solution");
        out.x.push_back(x);
        out.nodal_error=std::max(out.nodal_error,std::abs(out.temperature[i]-exact(r,x)));
        if(i<r.elements) out.midpoint_error=std::max(out.midpoint_error,
            std::abs((out.temperature[i]+out.temperature[i+1])/2-exact(r,x+h/2)));
    }
    return out;
}
Network forward(const Parameters& p, double x, double y) {
    for(double v:p) if(!std::isfinite(v)) throw std::invalid_argument("Non-finite parameter");
    if(!std::isfinite(x)||!std::isfinite(y)) throw std::invalid_argument("Non-finite sample");
    const double z=p[0]*x+p[1], h=std::tanh(z), prediction=p[2]*h+p[3], e=prediction-y;
    const double dz=e*p[2]*(1-h*h);
    return {z,h,prediction,e,e*e/2,{dz*x,dz,e*h,e}};
}
Parameters update(const Parameters& p,double rate) {
    if(!std::isfinite(rate)||rate<=0||rate>1) throw std::invalid_argument("Learning rate must be in (0,1]");
    auto result=p; const auto g=forward(p).gradient;
    for(int i=0;i<4;++i) result[i]-=rate*g[i];
    return result;
}
Parameters gradient_errors(const Parameters& p,double epsilon) {
    if(!std::isfinite(epsilon)||epsilon<=0) throw std::invalid_argument("Invalid epsilon");
    Parameters error{}; const auto g=forward(p).gradient;
    for(int i=0;i<4;++i) {
        auto plus=p,minus=p; plus[i]+=epsilon; minus[i]-=epsilon;
        error[i]=std::abs((forward(plus).loss-forward(minus).loss)/(2*epsilon)-g[i]);
    }
    return error;
}
}
