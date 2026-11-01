#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "core/numerics.hpp"
#include <limits>

TEST_CASE("FE interpolation has known midpoint errors") {
    double previous=0;
    for(int n:{10,20,40,80}) {
        wb::Rod r; r.elements=n; const auto s=wb::solve(r);
        CHECK(s.midpoint_error==doctest::Approx(125.0/(n*n)).epsilon(1e-8));
        CHECK(s.nodal_error<1e-8);
        CHECK(s.temperature.front()==100); CHECK(s.temperature.back()==20);
        if(previous) CHECK(previous/s.midpoint_error==doctest::Approx(4).epsilon(1e-8));
        previous=s.midpoint_error;
    }
}
TEST_CASE("Independent dense reference agrees for changed boundary data") {
    wb::Rod r; r.elements=17; r.left=-25; r.right=70; r.source=-150; r.length=2;
    const auto a=wb::solve(r), b=wb::solve(r,true);
    for(std::size_t i=0;i<a.temperature.size();++i) CHECK(a.temperature[i]==doctest::Approx(b.temperature[i]).epsilon(1e-9));
}
TEST_CASE("Zero source gives the linear boundary solution") {
    wb::Rod r; r.source=0;
    CHECK(wb::solve(r).midpoint_error<1e-10);
}
TEST_CASE("Invalid and extreme inputs are rejected") {
    wb::Rod r; r.elements=0; CHECK_THROWS_AS(wb::solve(r),std::invalid_argument);
    r=wb::Rod{}; r.conductivity=0; CHECK_THROWS_AS(wb::solve(r),std::invalid_argument);
    r=wb::Rod{}; r.source=std::numeric_limits<double>::infinity(); CHECK_THROWS_AS(wb::solve(r),std::invalid_argument);
    CHECK_THROWS_AS(wb::update({0.5,0,1,0},0),std::invalid_argument);
    CHECK_THROWS_AS(wb::gradient_errors({0.5,0,1,0},0),std::invalid_argument);
}
TEST_CASE("Backprop golden values and simultaneous update") {
    wb::Parameters p{0.5,0,1,0}; const auto f=wb::forward(p);
    CHECK(f.h==doctest::Approx(0.46211715726).epsilon(1e-9));
    CHECK(f.loss==doctest::Approx(1.18254181900).epsilon(1e-9));
    CHECK(f.gradient[0]==doctest::Approx(-1.20946447524).epsilon(1e-9));
    CHECK(f.gradient[1]==doctest::Approx(-1.20946447524).epsilon(1e-9));
    CHECK(f.gradient[2]==doctest::Approx(-0.71068204749).epsilon(1e-9));
    CHECK(f.gradient[3]==doctest::Approx(-1.53788284274).epsilon(1e-9));
    const auto next=wb::update(p);
    const wb::Parameters expected{0.62094644752,0.12094644752,1.07106820475,0.15378828427};
    for(int i=0;i<4;++i) CHECK(next[i]==doctest::Approx(expected[i]).epsilon(1e-9));
    for(double e:wb::gradient_errors(p)) CHECK(e<1e-5);
    for(int i=0;i<101;++i) p=wb::update(p);
    CHECK(wb::forward(p).loss<1e-18);
}
