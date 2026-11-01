#include "batch.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <mutex>
#include <random>
#include <thread>

namespace wb {
std::vector<Rod> generate_batch(std::size_t count, int elements, std::uint32_t seed) {
    if (count == 0 || count > 10000 || elements < 2 || elements > 100000 ||
        count > 2000000 / static_cast<std::size_t>(elements + 1))
        throw std::invalid_argument("Batch exceeds case, mesh or total-node limit");
    std::mt19937 random(seed);
    std::vector<Rod> cases(count);
    for (auto& rod : cases) {
        rod.elements = elements;
        // Fixed conversion avoids implementation-specific uniform_real_distribution.
        rod.conductivity = 1 + 49 * (static_cast<double>(random()) / 4294967295.0);
        rod.source = 100 + 19900 * (static_cast<double>(random()) / 4294967295.0);
    }
    return cases;
}

std::vector<Solution> solve_batch(const std::vector<Rod>& cases, unsigned workers,
                                 const std::atomic<bool>& cancelled,
                                 std::atomic<std::size_t>* completed) {
    if (workers < 1 || workers > 4 || cases.empty() || cases.size() > 10000)
        throw std::invalid_argument("Use 1 to 4 workers and a nonempty bounded batch");
    std::size_t nodes = 0;
    for (const auto& rod : cases) {
        if (rod.elements < 2 || rod.elements > 100000)
            throw std::invalid_argument("Invalid batch mesh");
        nodes += static_cast<std::size_t>(rod.elements + 1);
        if (nodes > 2000000) throw std::invalid_argument("Batch total-node limit exceeded");
    }
    if (completed) completed->store(0);
    if (cancelled.load()) throw Cancelled();
    std::vector<Solution> results(cases.size());
    std::atomic<std::size_t> next{0};
    std::atomic<bool> failed{false};
    std::exception_ptr error;
    std::mutex error_mutex;
    auto work = [&] {
        try {
            while (!cancelled.load() && !failed.load()) {
                const auto index = next.fetch_add(1);
                if (index >= cases.size()) break;
                results[index] = solve(cases[index]);
                if (completed) completed->fetch_add(1);
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock(error_mutex);
            if (!error) error = std::current_exception();
            failed.store(true);
        }
    };
    std::vector<std::thread> pool;
    try {
        for (unsigned i = 1; i < workers; ++i) pool.emplace_back(work);
        work();
    } catch (...) {
        failed.store(true);
        for (auto& thread : pool) thread.join();
        throw;
    }
    for (auto& thread : pool) thread.join();
    if (error) std::rethrow_exception(error);
    if (cancelled.load()) throw Cancelled();
    return results;
}

double batch_difference(const std::vector<Solution>& a, const std::vector<Solution>& b) {
    if (a.size() != b.size()) throw std::invalid_argument("Batch sizes differ");
    double maximum = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].temperature.size() != b[i].temperature.size())
            throw std::invalid_argument("Mesh sizes differ");
        for (std::size_t j = 0; j < a[i].temperature.size(); ++j) {
            const double delta = std::abs(a[i].temperature[j] - b[i].temperature[j]);
            if (!std::isfinite(delta)) throw std::invalid_argument("Nonfinite batch result");
            maximum = std::max(maximum, delta);
        }
    }
    return maximum;
}
}
