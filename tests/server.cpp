#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "server/server.hpp"
#include "json.hpp"
#include <fstream>
#include <cstring>
#include <random>
TEST_CASE("Loopback API validates session origin input and routes") {
    wb::LocalServer server;
    REQUIRE(server.start());
    httplib::Client client("127.0.0.1",server.port());
    httplib::Headers headers{{"X-Session-Token",server.token()}};
    auto r=client.Get("/api/health"); REQUIRE(r); CHECK(r->status==401);
    for(const auto* route:{"/api/health","/api/backprop","/api/hardware"}) {
        r=client.Get(route,headers); REQUIRE(r); CHECK(r->status==200);
        CHECK(r->get_header_value("X-Content-Type-Options")=="nosniff");
    }
    auto hostile=headers; hostile.emplace("Origin","https://example.invalid");
    r=client.Get("/api/health",hostile); REQUIRE(r); CHECK(r->status==403);
    hostile=headers; hostile.emplace("Host","evil.invalid");
    r=client.Get("/api/health",hostile); REQUIRE(r); CHECK(r->status==403);
    r=client.Post("/api/fea",headers,"{}","application/json"); REQUIRE(r); CHECK(r->status==200);
    for(const auto* input:{"{", "[]", "{\"elements\":2001}", "{\"elements\":2.5}", "{\"source\":\"bad\"}","{\"extra\":1}"}) {
        r=client.Post("/api/fea",headers,input,"application/json"); REQUIRE(r); CHECK(r->status==400);
    }
    r=client.Post("/api/stl",headers,"bad","text/plain"); REQUIRE(r); CHECK(r->status==400);
    r=client.Post("/api/csv",headers,"bad","text/plain"); REQUIRE(r); CHECK(r->status==400);
    r=client.Get("/../docs/MASTER_SPEC.md",headers); REQUIRE(r); CHECK(r->status!=200);
    r=client.Post("/api/fea",headers,std::string(5000,'x'),"application/json"); REQUIRE(r); CHECK(r->status==413);
}

TEST_CASE("Background jobs expose bounded submission polling and cancellation") {
    wb::LocalServer server; REQUIRE(server.start());
    httplib::Client client("127.0.0.1",server.port());
    httplib::Headers headers{{"X-Session-Token",server.token()}};
    auto r=client.Post("/api/jobs/batch",headers,"{}","application/json");
    REQUIRE(r); REQUIRE(r->status==202);
    auto id=nlohmann::json::parse(r->body)["id"].get<unsigned long long>();
    auto path="/api/jobs/"+std::to_string(id);
    r=client.Get(path,headers);REQUIRE(r);CHECK(r->status==200);
    r=client.Post(path+"/cancel",headers,"{}","application/json");REQUIRE(r);CHECK(r->status==200);
    r=client.Get("/api/jobs/999999",headers);REQUIRE(r);CHECK(r->status==404);
    r=client.Post("/api/jobs/batch",headers,"bad","application/json");REQUIRE(r);CHECK(r->status==400);
}
TEST_CASE("Browser bootstrap exchanges token for HttpOnly cookie and clean URL") {
    wb::LocalServer server;REQUIRE(server.start());
    httplib::Client client("127.0.0.1",server.port());
    auto r=client.Get("/session?token="+server.token());
    REQUIRE(r);CHECK(r->status==303);CHECK(r->get_header_value("Location")=="/");
    const auto cookie=r->get_header_value("Set-Cookie");
    CHECK(cookie.find("HttpOnly")!=std::string::npos);
    CHECK(cookie.find("SameSite=Strict")!=std::string::npos);
    r=client.Get("/api/health",{{"Cookie",cookie.substr(0,cookie.find(';'))}});
    REQUIRE(r);CHECK(r->status==200);
    r=client.Get("/api/health",{{"Cookie","wb_session=wrong"}});REQUIRE(r);CHECK(r->status==401);
    r=client.Get("/session?token=wrong");REQUIRE(r);CHECK(r->status==401);
}
TEST_CASE("Static shell is authenticated and restricted to asset paths") {
    const auto root=std::filesystem::temp_directory_path()/ ("wb-assets-"+std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root/"assets");
    struct Cleanup {std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);}} cleanup{root};
    {std::ofstream file(root/"index.html");file<<"<!doctype html><title>test</title>";}
    {std::ofstream file(root/"assets"/"app.js");file<<"console.log('local')";}
    {std::ofstream file(root/"assets"/"icon.svg");file<<"<svg xmlns=\"http://www.w3.org/2000/svg\"/>";}
    wb::LocalServer server(root);REQUIRE(server.start());
    httplib::Client client("127.0.0.1",server.port());
    httplib::Headers h{{"X-Session-Token",server.token()}};
    auto r=client.Get("/",h);REQUIRE(r);CHECK(r->status==200);
    r=client.Get("/assets/app.js",h);REQUIRE(r);CHECK(r->status==200);
    r=client.Get("/assets/icon.svg",h);REQUIRE(r);CHECK(r->status==200);CHECK(r->get_header_value("Content-Type")=="image/svg+xml");
    r=client.Get("/assets/icon.svg");REQUIRE(r);CHECK(r->status==401);
    r=client.Get("/assets/../index.html",h);REQUIRE(r);CHECK(r->status!=200);
    r=client.Get("/");REQUIRE(r);CHECK(r->status==401);
}

TEST_CASE("Job event stream reports terminal result and rejects unknown jobs") {
    wb::LocalServer server;REQUIRE(server.start());
    httplib::Client client("127.0.0.1",server.port());
    client.set_read_timeout(7,0);
    httplib::Headers headers{{"X-Session-Token",server.token()}};
    auto r=client.Post("/api/jobs/batch",headers,"{}","application/json");REQUIRE(r);REQUIRE(r->status==202);
    const auto id=nlohmann::json::parse(r->body)["id"].get<unsigned long long>();
    r=client.Get("/api/jobs/"+std::to_string(id)+"/events",headers);
    REQUIRE(r);CHECK(r->status==200);CHECK(r->get_header_value("Content-Type")=="text/event-stream");
    CHECK(r->body.find("event: progress")!=std::string::npos);
    CHECK(r->body.find("complete")!=std::string::npos);
    r=client.Get("/api/jobs/999999/events",headers);REQUIRE(r);CHECK(r->status==404);
}

TEST_CASE("Thermal comparison reports exact reference and second order convergence") {
    wb::LocalServer server;REQUIRE(server.start());
    httplib::Client client("127.0.0.1",server.port());
    const auto r=client.Post("/api/fea",{{"X-Session-Token",server.token()}},"{}","application/json");
    REQUIRE(r);REQUIRE(r->status==200);
    const auto data=nlohmann::json::parse(r->body);
    REQUIRE(data["comparison"].size()==4);
    CHECK(data["exact_x"].size()==201);
    CHECK(data["exact_temperature"][0].get<double>()==100);
    for(std::size_t i=0;i<4;++i) {
        const auto& row=data["comparison"][i];
        const int n=row["elements"].get<int>();
        CHECK(row["midpoint_error"].get<double>()==doctest::Approx(125.0/(n*n)).epsilon(1e-8));
        if(i) CHECK(row["order"].get<double>()==doctest::Approx(2).epsilon(1e-8));
    }
}
TEST_CASE("Binary CSV transfer uses documented little endian float64 layout") {
    wb::LocalServer server;REQUIRE(server.start());
    httplib::Client client("127.0.0.1",server.port());
    const auto r=client.Post("/api/csv/binary",{{"X-Session-Token",server.token()}},
        "node_id,x_m,y_m,z_m,temperature_C\n7,1,2,3,25\n","text/plain");
    REQUIRE(r);REQUIRE(r->status==200);REQUIRE(r->body.size()==40);
    const double expected[]={7,1,2,3,25};
    for(std::size_t i=0;i<5;++i){
        std::uint64_t bits=0;
        for(unsigned j=0;j<8;++j)bits|=static_cast<std::uint64_t>(static_cast<unsigned char>(r->body[i*8+j]))<<(j*8);
        double value=0;std::memcpy(&value,&bits,8);CHECK(value==expected[i]);
    }
    const auto bad=client.Post("/api/stl/binary",{{"X-Session-Token",server.token()}},"broken","text/plain");
    REQUIRE(bad);CHECK(bad->status==400);
}
TEST_CASE("Verification job reports measured core checks without claiming full acceptance") {
    wb::LocalServer server;REQUIRE(server.start());
    httplib::Client client("127.0.0.1",server.port());
    httplib::Headers headers{{"X-Session-Token",server.token()}};
    auto r=client.Post("/api/jobs/verification",headers,"{}","application/json");
    REQUIRE(r);REQUIRE(r->status==202);
    const auto id=nlohmann::json::parse(r->body)["id"].get<unsigned long long>();
    nlohmann::json job;
    for(int i=0;i<100;++i){
        r=client.Get("/api/jobs/"+std::to_string(id),headers);REQUIRE(r);
        job=nlohmann::json::parse(r->body);
        if(job["state"]=="complete")break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(job["state"]=="complete");
    const auto result=nlohmann::json::parse(job["result"].get<std::string>());
    CHECK(result["full_specification_complete"]==false);
    REQUIRE(result["checks"].size()==11);
    for(const auto& check:result["checks"])CHECK(check["passed"]==true);
}
