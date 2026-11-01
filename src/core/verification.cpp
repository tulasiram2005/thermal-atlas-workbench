#include "verification.hpp"
#include "numerics.hpp"
#include "batch.hpp"
#include "hardware.hpp"
#include "parsers.hpp"
#include <algorithm>
#include <cmath>
namespace wb {
std::vector<Check> verify_modules() {
    std::vector<Check> checks;
    for(int n:{10,20,40,80}) {
        Rod rod;rod.elements=n;const auto s=solve(rod);
        checks.push_back({"FE midpoint N="+std::to_string(n),std::abs(s.midpoint_error-125.0/(n*n))<1e-8,s.midpoint_error,"degC"});
    }
    const auto errors=gradient_errors({.5,0,1,0});
    const auto error=*std::max_element(errors.begin(),errors.end());
    checks.push_back({"Backprop finite difference",error<=1e-5,error,"absolute gradient error"});
    auto parameters=Parameters{.5,0,1,0};
    for(int i=0;i<101;++i) parameters=update(parameters);
    const auto loss=forward(parameters).loss;
    checks.push_back({"Backprop training",loss<1e-18,loss,"loss"});
    std::atomic<bool> cancelled{false};const auto cases=generate_batch(8,80,42);
    const double difference=batch_difference(solve_batch(cases,1,cancelled),solve_batch(cases,2,cancelled));
    checks.push_back({"Serial parallel agreement",difference<=1e-8,difference,"degC"});
    const auto mesh=parse_stl("solid synthetic\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid synthetic\n");
    checks.push_back({"ASCII STL fixture",mesh.vertices.size()==3,static_cast<double>(mesh.vertices.size()/3),"triangles"});
    const auto field=parse_csv("node_id,x_m,y_m,z_m,temperature_C\n1,0,0,0,100\n2,1,0,0,20\n");
    checks.push_back({"Thermal CSV fixture",field.nodes.size()==2 && field.mean==60,field.mean,"degC mean"});
    bool rejected=false;try{(void)parse_csv("invalid");}catch(const std::exception&){rejected=true;}
    checks.push_back({"Malformed CSV rejection",rejected,rejected?1.0:0.0,"rejected"});
    unsigned recovered=0;
    for(const auto& info:discover_mock_devices()) {
        MockDevice device(info,true);device.connect(0);device.poll(100);device.poll(350);device.poll(450);
        if(device.state()==DeviceState::connected && device.telemetry().simulated)++recovered;
    }
    checks.push_back({"SIMULATED hardware recovery",recovered==3,static_cast<double>(recovered),"devices"});
    return checks;
}
}
