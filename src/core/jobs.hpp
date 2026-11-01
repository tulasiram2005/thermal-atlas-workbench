#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace wb {
struct JobSnapshot { std::uint64_t id; std::string state, result; std::size_t progress; };
class Jobs {
public:
    using Task = std::function<std::string(const std::atomic<bool>&, std::atomic<std::size_t>&)>;
    explicit Jobs(unsigned workers = 2, std::size_t capacity = 16);
    ~Jobs();
    std::uint64_t submit(Task task);
    JobSnapshot get(std::uint64_t id);
    void cancel(std::uint64_t id);
    void shutdown();
private:
    struct Job {
        std::uint64_t id;
        Task task;
        std::atomic<bool> cancelled{false};
        std::atomic<std::size_t> progress{0};
        std::string state="queued", result;
    };
    void work();
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::shared_ptr<Job>> queue_;
    std::map<std::uint64_t,std::shared_ptr<Job>> jobs_;
    std::vector<std::thread> workers_;
    std::size_t capacity_;
    std::uint64_t next_=1;
    bool stopping_=false;
};
}
