#include "doctest.h"
#include "core/parsers.hpp"
#include <stdexcept>
namespace {
const std::string header="node_id,x_m,y_m,z_m,temperature_C\n";
const std::string triangle="solid sample\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1e0 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid sample\n";
}
TEST_CASE("ASCII STL parses bounds and exponents") {
    auto m=wb::parse_stl(triangle); CHECK(m.vertices.size()==3); CHECK(m.high[0]==1); CHECK(m.high[1]==1); CHECK(m.degenerate==0);
    CHECK(wb::parse_stl("\xef\xbb\xbf"+triangle).vertices.size()==3);
    std::string crlf; for(char c:triangle) { if(c=='\n') crlf+='\r'; crlf+=c; }
    CHECK(wb::parse_stl(crlf).vertices.size()==3);
}
TEST_CASE("STL malformed and binary input is rejected") {
    for(const auto& s:std::vector<std::string>{"", "solid x\nendsolid x",triangle.substr(0,65),triangle+"extra",std::string(84,'\0')}) CHECK_THROWS_AS(wb::parse_stl(s),std::invalid_argument);
    for(const auto& value:{"nan","inf","1e999"}) {
        auto s=triangle; s.replace(s.find("1e0"),3,value); CHECK_THROWS_AS(wb::parse_stl(s),std::invalid_argument);
    }
}
TEST_CASE("CSV statistics and finite data") {
    const auto f=wb::parse_csv(header+"1,0,0,0,100\n2,1,0,0,20\n");
    CHECK(f.nodes.size()==2); CHECK(f.minimum==20); CHECK(f.maximum==100); CHECK(f.mean==60);
}
TEST_CASE("CSV malformed rows are rejected") {
    for(const auto& row:{"1,0,0,0,nan", "1,0,0,0,inf", "1,0,0,0,1e999", "1,0,0,20", "1,0,0,0,20,", "1.5,0,0,0,20", "-1,0,0,0,20", "1,0,0,0,20x", "1,0,0,0,20\n1,0,0,0,30"})
        CHECK_THROWS_AS(wb::parse_csv(header+row),std::invalid_argument);
    CHECK_THROWS_AS(wb::parse_csv(header),std::invalid_argument);
    CHECK_THROWS_AS(wb::parse_csv("wrong\n"),std::invalid_argument);
    CHECK_THROWS_AS(wb::parse_csv(header+std::string(5000,'1')),std::invalid_argument);
    CHECK_THROWS_AS(wb::parse_csv(header+std::string(1,'\0')),std::invalid_argument);
}
