#include "core/batch.hpp"
#include "doctest.h"

TEST_CASE("Seeded varied batches agree across bounded worker counts") {
    const auto cases = wb::generate_batch(31, 80, 42);
    const auto repeated = wb::generate_batch(31, 80, 42);
    CHECK(cases[0].conductivity == repeated[0].conductivity);
    CHECK(cases[0].source == repeated[0].source);
    CHECK(cases[0].source != cases[1].source);
    std::atomic<bool> cancel{false};
    std::atomic<std::size_t> progress{0};
    const auto serial = wb::solve_batch(cases, 1, cancel);
    for (unsigned workers : {2U, 4U}) {
        const auto parallel = wb::solve_batch(cases, workers, cancel, &progress);
        CHECK(progress.load() == cases.size());
        CHECK(wb::batch_difference(serial, parallel) <= 1e-8);
    }
}
TEST_CASE("Batch cancellation, worker failure and resource limits are explicit") {
    auto cases = wb::generate_batch(8, 10, 0);
    std::atomic<bool> cancel{true};
    CHECK_THROWS_AS(wb::solve_batch(cases, 2, cancel), wb::Cancelled);
    cancel.store(false);
    cases[3].conductivity = 0;
    CHECK_THROWS_AS(wb::solve_batch(cases, 4, cancel), std::invalid_argument);
    CHECK_THROWS_AS(wb::solve_batch(cases, 0, cancel), std::invalid_argument);
    CHECK_THROWS_AS(wb::generate_batch(10000, 100000, 0), std::invalid_argument);
    CHECK_THROWS_AS(wb::generate_batch(0, 10, 0), std::invalid_argument);
    CHECK_THROWS_AS(wb::batch_difference({wb::Solution{}}, {}), std::invalid_argument);
}
