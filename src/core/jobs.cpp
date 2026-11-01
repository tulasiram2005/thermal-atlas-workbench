#include "jobs.hpp"
#include "batch.hpp"
#include <stdexcept>
namespace wb {
Jobs::Jobs(unsigned workers,std::size_t capacity):capacity_(capacity) {
    if(workers<1 || workers>4 || capacity<1 || capacity>64)
        throw std::invalid_argument("Invalid job pool limits");
    try { for(unsigned i=0;i<workers;++i) workers_.emplace_back([this]{work();}); }
    catch(...) { shutdown(); throw; }
}
Jobs::~Jobs() { shutdown(); }
void Jobs::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_=true;
        for(auto& entry:jobs_) entry.second->cancelled.store(true);
    }
    ready_.notify_all();
    for(auto& worker:workers_) if(worker.joinable()) worker.join();
}
std::uint64_t Jobs::submit(Task task) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_ || !task) throw std::runtime_error("Job pool unavailable");
    if(jobs_.size()>=capacity_) {
        for(auto it=jobs_.begin();it!=jobs_.end();++it) {
            const auto& s=it->second->state;
            if(s=="complete" || s=="failed" || s=="cancelled") {jobs_.erase(it);break;}
        }
    }
    if(jobs_.size()>=capacity_) throw std::runtime_error("Job capacity reached");
    auto job=std::make_shared<Job>(); job->id=next_++; job->task=std::move(task);
    jobs_[job->id]=job; queue_.push_back(job); ready_.notify_one(); return job->id;
}
JobSnapshot Jobs::get(std::uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto job=jobs_.at(id);
    return {id,job->state,job->result,job->progress.load()};
}
void Jobs::cancel(std::uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_.at(id)->cancelled.store(true);
}
void Jobs::work() {
    for(;;) {
        std::shared_ptr<Job> job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock,[this]{return stopping_ || !queue_.empty();});
            if(queue_.empty()) return;
            job=queue_.front(); queue_.pop_front();
            if(job->cancelled.load()) {job->state="cancelled";continue;}
            job->state="running";
        }
        std::string result, state="complete";
        try {
            result=job->task(job->cancelled,job->progress);
            if(result.size()>4*1024*1024) throw std::runtime_error("Result too large");
            if(job->cancelled.load()) {state="cancelled";result.clear();}
        } catch(const Cancelled&) {state="cancelled";result.clear();}
        catch(...) {state="failed";result.clear();}
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(job->cancelled.load()) {state="cancelled";result.clear();}
            job->state=state;job->result=std::move(result);job->task={};
        }
    }
}
}
