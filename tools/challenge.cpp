#include "core/numerics.hpp"
#include "core/batch.hpp"
#include "core/parsers.hpp"
#include "json.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    const auto rod=wb::solve(wb::Rod{});
    wb::Parameters initial{.5,0,1,0};
    auto sequential=initial;
    for(std::size_t i=0;i<sequential.size();++i)
        sequential[i]-=.1*wb::forward(sequential).gradient[i];
    const auto cases=wb::generate_batch(32,1000,42);
    std::atomic<bool> stop{false};
    nlohmann::json timing;
    for(const unsigned workers:{1U,2U}) {
        const auto start=std::chrono::steady_clock::now();
        std::ifstream csv(argv[1]); if(!csv) return 2;
        const std::string body{std::istreambuf_iterator<char>(csv),{}};
        const auto field=wb::parse_csv(body);
        const auto compute=std::chrono::steady_clock::now();
        const auto output=wb::solve_batch(cases,workers,stop);
        const auto end=std::chrono::steady_clock::now();
        if(output.empty() || field.nodes.empty()) return 1;
        timing.push_back({{"workers",workers},{"including_read_parse_ms",std::chrono::duration<double,std::milli>(end-start).count()},{"solve_ms",std::chrono::duration<double,std::milli>(end-compute).count()}});
    }
    std::cout<<nlohmann::json({{"nodal_error",rod.nodal_error},{"midpoint_error",rod.midpoint_error},{"sequential",sequential},{"simultaneous",wb::update(initial)},{"illustrative_timings",timing}}).dump(2)<<'\n';
}
