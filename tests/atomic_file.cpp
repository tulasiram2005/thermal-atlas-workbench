#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "platform/atomic_file.hpp"
#include "core/batch.hpp"
#include <fstream>
#include <random>

namespace {
struct Fixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("wb-test-" + std::to_string(std::random_device{}()));
    Fixture() { std::filesystem::create_directory(root); }
    ~Fixture() { std::error_code e; std::filesystem::remove_all(root, e); }
};
std::string read(const std::filesystem::path& p) {
    std::ifstream file(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
}
TEST_CASE("Exports replace whole files and support spaces and unicode paths") {
    Fixture f;
    const auto target = f.root / std::filesystem::u8path("result space \u03bb.csv");
    std::atomic<bool> cancelled{false};
    wb::atomic_write(target, "original", cancelled);
    wb::atomic_write(target, "replacement", cancelled);
    CHECK(read(target) == "replacement");
    CHECK(std::distance(std::filesystem::directory_iterator(f.root), std::filesystem::directory_iterator{}) == 1);
}
TEST_CASE("Failure and cancellation preserve previous results with no temporary debris") {
    Fixture f;
    const auto target = f.root / "result.csv";
    std::atomic<bool> cancelled{false};
    wb::atomic_write(target, "original", cancelled);
    for (auto fault : {wb::WriteFault::after_write, wb::WriteFault::before_publish}) {
        CHECK_THROWS(wb::atomic_write(target, std::string(100000, 'x'), cancelled, fault));
        CHECK(read(target) == "original");
        CHECK(std::distance(std::filesystem::directory_iterator(f.root), std::filesystem::directory_iterator{}) == 1);
    }
    cancelled.store(true);
    CHECK_THROWS_AS(wb::atomic_write(target, "new", cancelled), wb::Cancelled);
    CHECK(read(target) == "original");
    cancelled.store(false);
    CHECK_THROWS(wb::atomic_write(f.root / "missing" / "file.csv", "x", cancelled));
    CHECK_THROWS(wb::atomic_write(f.root / "new.csv", "partial", cancelled, wb::WriteFault::after_write));
    CHECK_FALSE(std::filesystem::exists(f.root / "new.csv"));
}
