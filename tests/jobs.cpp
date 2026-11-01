#include "core/jobs.hpp"
#include "doctest.h"
#include <chrono>
#include <future>
namespace {
wb::JobSnapshot finish(wb::Jobs& jobs,std::uint64_t id) {
    for(int i=0;i<1000;++i) {
        auto s=jobs.get(id);
        if(s.state!="queued" && s.state!="running") return s;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("Job test timed out");
}
}
TEST_CASE("Jobs publish complete results and contain failures") {
    wb::Jobs jobs;
    auto id=jobs.submit([](const auto&,auto& progress){progress.store(3);return "result";});
    auto s=finish(jobs,id);CHECK(s.state=="complete");CHECK(s.result=="result");CHECK(s.progress==3);
    id=jobs.submit([](const auto&,auto&)->std::string{throw std::runtime_error("private path");});
    s=finish(jobs,id);CHECK(s.state=="failed");CHECK(s.result.empty());
    CHECK_THROWS(jobs.get(9999));
}
TEST_CASE("Running cancellation and queue caps do not publish partial results") {
    wb::Jobs jobs(1,1);
    std::promise<void> entered;
    auto started=entered.get_future();
    auto id=jobs.submit([&](const auto& cancel,auto& progress){
        entered.set_value();
        while(!cancel.load()) {progress.fetch_add(1);std::this_thread::yield();}
        return "partial";
    });
    REQUIRE(started.wait_for(std::chrono::seconds(1))==std::future_status::ready);
    CHECK_THROWS(jobs.submit([](const auto&,auto&){return "overflow";}));
    jobs.cancel(id);
    const auto s=finish(jobs,id);CHECK(s.state=="cancelled");CHECK(s.result.empty());
    auto next=jobs.submit([](const auto&,auto&){return "replacement";});
    CHECK(finish(jobs,next).result=="replacement");
    jobs.shutdown();
    CHECK_THROWS(jobs.submit([](const auto&,auto&){return "stopped";}));
}
