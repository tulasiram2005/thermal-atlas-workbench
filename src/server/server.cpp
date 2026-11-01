#include "server.hpp"
#include "core/numerics.hpp"
#include "core/batch.hpp"
#include "core/verification.hpp"
#include "core/hardware.hpp"
#include "core/parsers.hpp"
#include "json.hpp"
#include <chrono>
#include <cmath>
#include <random>
#include <sstream>
#include <fstream>
#include <cstring>

namespace wb {
namespace {
void respond(httplib::Response& res, const nlohmann::json& value) {
    res.set_content(value.dump(), "application/json");
}
void error(httplib::Response& res, int status, const char* code) {
    res.status = status;
    respond(res, {{"code",code},{"message","Request rejected"},
                  {"hint","Check session, request format and permitted input ranges"},{"details",nullptr}});
}
}
LocalServer::LocalServer(std::filesystem::path assets) {
    std::random_device random;
    static constexpr char hex[] = "0123456789abcdef";
    for (int i=0;i<64;++i) token_ += hex[random() & 15U];
    server_.set_payload_max_length(1024 * 1024);
    server_.new_task_queue = [] { return new httplib::ThreadPool(4); };
    server_.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Security-Policy", "default-src 'self'; frame-ancestors 'none'; object-src 'none'; base-uri 'none'");
        res.set_header("X-Content-Type-Options", "nosniff");
        res.set_header("Referrer-Policy", "no-referrer");
        res.set_header("Cache-Control", "no-store");
        const auto host = "127.0.0.1:" + std::to_string(port_);
        if (req.get_header_value("Host") != host ||
            (req.has_header("Origin") && req.get_header_value("Origin") != "http://" + host)) {
            error(res,403,"ORIGIN_REJECTED");
            return httplib::Server::HandlerResponse::Handled;
        }
        if (req.method=="GET" && req.path=="/session" && req.has_param("token") &&
            req.get_param_value("token")==token_) {
            res.set_header("Set-Cookie","wb_session="+token_+"; HttpOnly; SameSite=Strict; Path=/");
            res.set_redirect("/",303);
            return httplib::Server::HandlerResponse::Handled;
        }
        bool cookie_ok=false;
        std::istringstream cookies(req.get_header_value("Cookie"));
        std::string cookie;
        while(std::getline(cookies,cookie,';')) {
            const auto first=cookie.find_first_not_of(" ");
            if(first!=std::string::npos && cookie.substr(first)=="wb_session="+token_) cookie_ok=true;
        }
        if (!cookie_ok && req.get_header_value("X-Session-Token") != token_) {
            error(res,401,"SESSION_REQUIRED");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
    server_.set_exception_handler([](const auto&, auto& res, std::exception_ptr) {
        error(res,400,"INVALID_INPUT");
    });
    server_.set_error_handler([](const auto&, auto& res) {
        if (res.body.empty()) error(res,res.status,"REQUEST_REJECTED");
    });
    if(!assets.empty() && std::filesystem::is_directory(assets)) {
        const auto root=std::filesystem::canonical(assets);
        server_.Get(R"(/(?:assets/[A-Za-z0-9_.-]+)?)", [root](const auto& req,auto& res) {
            const auto relative=req.path=="/"?std::string("index.html"):req.path.substr(1);
            const auto file=std::filesystem::weakly_canonical(root/relative);
            const auto rel=file.lexically_relative(root);
            if(rel.empty() || *rel.begin()==".." || !std::filesystem::is_regular_file(file)) {
                error(res,404,"ASSET_NOT_FOUND");return;
            }
            const auto extension=file.extension().string();
            const char* mime=extension==".html"?"text/html":extension==".js"?"text/javascript":extension==".css"?"text/css":extension==".svg"?"image/svg+xml":nullptr;
            if(!mime) {error(res,404,"ASSET_NOT_FOUND");return;}
            std::ifstream stream(file,std::ios::binary);
            const std::string body{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
            res.set_content(body,mime);
        });
    }
    server_.Post("/api/jobs/batch", [this](const auto& req, auto& res) {
        if(req.body!="{}") {error(res,400,"INVALID_INPUT");return;}
        try {
            auto id=jobs_.submit([](const auto& cancel,auto& progress) {
                const auto cases=generate_batch(128,8000,42);
                const auto result=solve_batch(cases,2,cancel,&progress);
                return nlohmann::json({{"cases",result.size()},{"seed",42},{"elements",8000}}).dump();
            });
            res.status=202;respond(res,{{"id",id}});
        } catch(...) {error(res,429,"JOB_CAPACITY");}
    });
    server_.Get(R"(/api/jobs/(\d+))", [this](const auto& req,auto& res) {
        try {
            const auto s=jobs_.get(std::stoull(req.matches[1].str()));
            respond(res,{{"id",s.id},{"state",s.state},{"progress",s.progress},{"result",s.result}});
        } catch(...) {error(res,404,"JOB_NOT_FOUND");}
    });
    server_.Get(R"(/api/jobs/(\d+)/events)", [this](const auto& req,auto& res) {
        std::uint64_t id=0;
        try {id=std::stoull(req.matches[1].str());(void)jobs_.get(id);}
        catch(...) {error(res,404,"JOB_NOT_FOUND");return;}
        if(streams_.fetch_add(1)>=2) {
            streams_.fetch_sub(1);error(res,429,"STREAM_CAPACITY");return;
        }
        // A shared lifetime guard releases capacity even when a client disconnects.
        auto guard=std::shared_ptr<int>(new int(0),[this](int* p){delete p;streams_.fetch_sub(1);});
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        res.set_chunked_content_provider("text/event-stream",
            [this,id,deadline,guard](std::size_t offset,httplib::DataSink& sink) {
                (void)guard;
                if(offset) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                try {
                    const auto s=jobs_.get(id);
                    const auto event="event: progress\ndata: "+nlohmann::json({{"id",s.id},
                        {"state",s.state},{"progress",s.progress},{"result",s.result}}).dump()+"\n\n";
                    if(!sink.write(event.data(),event.size())) return false;
                    if((s.state!="queued" && s.state!="running") || std::chrono::steady_clock::now()>=deadline)
                        sink.done();
                } catch(...) {
                    const std::string event="event: expired\ndata: {}\n\n";
                    if(!sink.write(event.data(),event.size())) return false;
                    sink.done();
                }
                return true;
            });
    });
    server_.Post(R"(/api/jobs/(\d+)/cancel)", [this](const auto& req,auto& res) {
        try {jobs_.cancel(std::stoull(req.matches[1].str()));respond(res,{{"status","cancellation_requested"}});}
        catch(...) {error(res,404,"JOB_NOT_FOUND");}
    });
    server_.Post("/api/jobs/verification", [this](const auto& req,auto& res) {
        if(req.body!="{}") {error(res,400,"INVALID_INPUT");return;}
        try {
            const auto id=jobs_.submit([](const auto& cancelled,auto& progress) {
                if(cancelled.load())throw Cancelled();
                nlohmann::json checks=nlohmann::json::array();
                for(const auto& check:verify_modules()) {
                    if(cancelled.load())throw Cancelled();
                    checks.push_back({{"name",check.name},{"passed",check.passed},{"measured",check.measured},{"units",check.units}});
                    progress.fetch_add(1);
                }
                return nlohmann::json({{"checks",checks},{"full_specification_complete",false},
                    {"scope","Implemented core modules; browser and Windows acceptance pending"}}).dump();
            });res.status=202;respond(res,{{"id",id}});
        }catch(...){error(res,429,"JOB_CAPACITY");}
    });
    server_.Get("/api/health", [](const auto&, auto& res) {
        respond(res,{{"status","ready"},{"mode","offline"}});
    });
    server_.Post("/api/fea", [](const auto& req, auto& res) {
        if (req.body.size()>4096) { error(res,413,"INPUT_TOO_LARGE"); return; }
        int depth = 0;
        const auto j = nlohmann::json::parse(req.body,[&](int d, auto, auto&) {
            depth = std::max(depth,d); return d<=4;
        });
        if (depth>4 || !j.is_object()) throw std::invalid_argument("Invalid JSON shape");
        Rod rod;
        for (auto it=j.begin();it!=j.end();++it) {
            if (!it.value().is_number()) throw std::invalid_argument("Numeric fields required");
            const auto& key=it.key();
            if (key=="elements") {
                if (!it.value().is_number_integer()) throw std::invalid_argument("Integer mesh required");
                rod.elements=it.value().template get<int>();
            } else if(key=="length") rod.length=it.value().template get<double>();
            else if(key=="area") rod.area=it.value().template get<double>();
            else if(key=="conductivity") rod.conductivity=it.value().template get<double>();
            else if(key=="source") rod.source=it.value().template get<double>();
            else if(key=="left") rod.left=it.value().template get<double>();
            else if(key=="right") rod.right=it.value().template get<double>();
            else throw std::invalid_argument("Unknown field");
        }
        if (rod.elements>2000) throw std::invalid_argument("Interactive mesh limit");
        const auto s=solve(rod);
        nlohmann::json comparison=nlohmann::json::array();
        double previous=0;
        for(int n:{10,20,40,80}) {
            Rod mesh=rod;mesh.elements=n;
            const auto solved=solve(mesh);
            nlohmann::json order=nullptr;
            if(previous>0 && solved.midpoint_error>0)
                order=std::log(previous/solved.midpoint_error)/std::log(2.0);
            comparison.push_back({{"elements",n},{"h",mesh.length/n},
                {"x",solved.x},{"temperature",solved.temperature},
                {"midpoint_error",solved.midpoint_error},{"order",order}});
            previous=solved.midpoint_error;
        }
        std::vector<double> exact_x,exact_temperature;
        for(int i=0;i<=200;++i) {
            const double x=rod.length*i/200.0;
            exact_x.push_back(x);exact_temperature.push_back(exact(rod,x));
        }
        respond(res,{{"x",s.x},{"temperature",s.temperature},{"midpoint_error",s.midpoint_error},
            {"nodal_error",s.nodal_error},{"comparison",comparison},
            {"exact_x",exact_x},{"exact_temperature",exact_temperature}});
    });
    server_.Get("/api/backprop", [](const auto&, auto& res) {
        Parameters p{.5,0,1,0};
        const auto initial=forward(p);
        nlohmann::json losses=nlohmann::json::array();
        for(int i=0;i<=101;++i) { losses.push_back(forward(p).loss); if(i<101) p=update(p); }
        respond(res,{{"initial_loss",initial.loss},{"gradients",initial.gradient},
                     {"gradient_errors",gradient_errors({.5,0,1,0})},{"losses",losses}});
    });
    server_.Get("/api/hardware", [](const auto&, auto& res) {
        nlohmann::json devices=nlohmann::json::array();
        for(const auto& info:discover_mock_devices()) {
            MockDevice device(info,true);
            device.connect(0); device.poll(100); device.poll(350); device.poll(450);
            const auto t=device.telemetry();
            devices.push_back({{"id",info.id},{"kind",info.kind},{"capabilities",info.capabilities},
                {"simulated",true},{"log",device.log()},{"metric",t.metric},{"value",t.value},{"unit",t.unit}});
        }
        respond(res,devices);
    });
    auto binary_response=[](httplib::Response& res,const std::vector<double>& values) {
        static_assert(sizeof(double)==8,"Binary format requires 64-bit double");
        std::string bytes;bytes.reserve(values.size()*8);
        for(const double value:values) {
            std::uint64_t bits=0;std::memcpy(&bits,&value,8);
            for(unsigned shift=0;shift<64;shift+=8) bytes.push_back(static_cast<char>((bits>>shift)&255));
        }
        res.set_content(bytes,"application/octet-stream");
    };
    server_.Post("/api/stl/binary", [binary_response](const auto& req,auto& res) {
        const auto mesh=parse_stl(req.body);
        std::vector<double> values;values.reserve(mesh.vertices.size()*3);
        for(const auto& point:mesh.vertices) for(double value:point) values.push_back(value);
        binary_response(res,values);
    });
    server_.Post("/api/csv/binary", [binary_response](const auto& req,auto& res) {
        const auto field=parse_csv(req.body);
        std::vector<double> values;values.reserve(field.nodes.size()*5);
        for(const auto& node:field.nodes) {
            values.push_back(static_cast<double>(node.id));
            for(double value:node.position) values.push_back(value);
            values.push_back(node.temperature);
        }
        binary_response(res,values);
    });
    server_.Post("/api/stl", [](const auto& req, auto& res) {
        const auto m=parse_stl(req.body);
        respond(res,{{"triangles",m.vertices.size()/3},{"low",m.low},{"high",m.high},{"degenerate",m.degenerate}});
    });
    server_.Post("/api/csv", [](const auto& req, auto& res) {
        const auto f=parse_csv(req.body);
        respond(res,{{"nodes",f.nodes.size()},{"minimum",f.minimum},{"maximum",f.maximum},{"mean",f.mean}});
    });
}
bool LocalServer::start() {
    if(thread_.joinable()) return false;
    port_=server_.bind_to_any_port("127.0.0.1");
    if(port_<=0) return false;
    thread_=std::thread([this]{server_.listen_after_bind();});
    for(int i=0;i<100 && !server_.is_running();++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return server_.is_running();
}
void LocalServer::stop() { server_.stop(); if(thread_.joinable()) thread_.join(); jobs_.shutdown(); }
LocalServer::~LocalServer() { stop(); }
bool scaffold_smoke(std::chrono::steady_clock::time_point start,double& ready_ms) {
    LocalServer server;
    if(!server.start()) return false;
    httplib::Client client("127.0.0.1",server.port());
    const httplib::Headers headers{{"X-Session-Token",server.token()}};
    const auto health=client.Get("/api/health",headers);
    if(!health || health->status!=200) return false;
    ready_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    const auto fea=client.Post("/api/fea",headers,"{}","application/json");
    return health && health->status==200 && fea && fea->status==200 &&
        std::abs(nlohmann::json::parse(fea->body)["midpoint_error"].get<double>()-1.25)<1e-8;
}
}
