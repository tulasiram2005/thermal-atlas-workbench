#pragma once
#include "numerics.hpp"
#include <atomic>
#include <cstdint>
#include <stdexcept>

namespace wb {
struct Cancelled : std::runtime_error {
    Cancelled() : std::runtime_error("Computation cancelled; no batch result published") {}
};
std::vector<Rod> generate_batch(std::size_t count, int elements, std::uint32_t seed);
std::vector<Solution> solve_batch(const std::vector<Rod>& cases, unsigned workers,
                                 const std::atomic<bool>& cancelled,
                                 std::atomic<std::size_t>* completed = nullptr);
double batch_difference(const std::vector<Solution>& a, const std::vector<Solution>& b);
}
