#include "core/parsers.hpp"
#include <algorithm>
#include <cmath>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>

namespace wb {
namespace {
void validate(std::string& s) {
    if(s.size()>16*1024*1024) throw std::invalid_argument("SIZE: maximum input is 16 MiB");
    if(s.compare(0,3,"\xef\xbb\xbf")==0) s.erase(0,3);
    if(s.empty()) throw std::invalid_argument("EMPTY: file has no content");
    for(unsigned char c:s) if(c==0 || c>127) throw std::invalid_argument("ENCODING: ASCII or UTF-8 BOM required");
}
double number(const std::string& s,std::size_t row,std::size_t col) {
    std::istringstream input(s); input.imbue(std::locale::classic()); double v;
    if(!(input>>v) || !std::isfinite(v) || std::abs(v)>1e12)
        throw std::invalid_argument("NUMBER: row "+std::to_string(row)+" column "+std::to_string(col));
    input>>std::ws;
    if(!input.eof()) throw std::invalid_argument("NUMBER: row "+std::to_string(row)+" column "+std::to_string(col));
    return v;
}
void word(std::istream& in,const std::string& expected) {
    std::string got;
    if(!(in>>got)||got!=expected) throw std::invalid_argument("STL_SYNTAX: expected "+expected);
}
Point point(std::istream& in) {
    Point p{};
    for(std::size_t i=0;i<3;++i) { std::string s; if(!(in>>s)) throw std::invalid_argument("STL_TRUNCATED: missing coordinate"); p[i]=number(s,0,i+1); }
    return p;
}
}
Mesh parse_stl(std::string bytes) {
    if(bytes.size()>=84) {
        std::uint32_t count=0;
        for(unsigned i=0;i<4;++i) count|=static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[80+i]))<<(i*8);
        if(84ULL+50ULL*count==bytes.size()) throw std::invalid_argument("BINARY_STL: export as ASCII STL");
    }
    validate(bytes);
    std::istringstream in(bytes); word(in,"solid"); std::string line; std::getline(in,line);
    Mesh m; std::string token; bool ended=false;
    while(in>>token) {
        if(token=="endsolid") { std::getline(in,line); ended=true; break; }
        if(token!="facet") throw std::invalid_argument("STL_SYNTAX: expected facet");
        word(in,"normal"); point(in); word(in,"outer"); word(in,"loop");
        std::array<Point,3> triangle;
        for(auto& p:triangle) { word(in,"vertex"); p=point(in); m.vertices.push_back(p); }
        word(in,"endloop"); word(in,"endfacet");
        Point a{},b{};
        for(int i=0;i<3;++i) { a[i]=triangle[1][i]-triangle[0][i]; b[i]=triangle[2][i]-triangle[0][i]; }
        const Point cross{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
        if(cross[0]==0 && cross[1]==0 && cross[2]==0) ++m.degenerate;
    }
    if(!ended) throw std::invalid_argument("STL_TRUNCATED: missing endsolid");
    if(in>>token) throw std::invalid_argument("STL_SYNTAX: trailing content");
    if(m.vertices.empty()) throw std::invalid_argument("EMPTY_MESH: no triangles");
    m.low=m.high=m.vertices.front();
    for(const auto& p:m.vertices) for(int i=0;i<3;++i) { m.low[i]=std::min(m.low[i],p[i]); m.high[i]=std::max(m.high[i],p[i]); }
    return m;
}
Field parse_csv(std::string bytes) {
    validate(bytes); std::istringstream in(bytes); std::string line; std::getline(in,line);
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line!="node_id,x_m,y_m,z_m,temperature_C") throw std::invalid_argument("CSV_HEADER: exact schema required");
    Field f; std::set<std::int64_t> ids; std::size_t row=1;
    while(std::getline(in,line)) {
        ++row;
        if(!line.empty()&&line.back()=='\r') line.pop_back();
        if(line.size()>4096) throw std::invalid_argument("LINE_LIMIT: row "+std::to_string(row));
        std::vector<std::string> cells; std::size_t start=0;
        for(std::size_t i=0;i<=line.size();++i) if(i==line.size()||line[i]==',') { cells.push_back(line.substr(start,i-start)); start=i+1; }
        if(cells.size()!=5) throw std::invalid_argument("CSV_COLUMNS: row "+std::to_string(row));
        const double id=number(cells[0],row,1);
        if(id<0 || std::floor(id)!=id) throw std::invalid_argument("NODE_ID: row "+std::to_string(row)+" column 1");
        Node node{static_cast<std::int64_t>(id),{},0};
        if(!ids.insert(node.id).second) throw std::invalid_argument("DUPLICATE_ID: row "+std::to_string(row)+" column 1");
        for(std::size_t i=0;i<3;++i) node.position[i]=number(cells[i+1],row,i+2);
        node.temperature=number(cells[4],row,5);
        if(f.nodes.empty()) f.minimum=f.maximum=node.temperature;
        f.minimum=std::min(f.minimum,node.temperature); f.maximum=std::max(f.maximum,node.temperature);
        f.nodes.push_back(node); f.mean+=(node.temperature-f.mean)/static_cast<double>(f.nodes.size());
    }
    if(f.nodes.empty()) throw std::invalid_argument("EMPTY_FIELD: header only; no nodes");
    return f;
}
}
