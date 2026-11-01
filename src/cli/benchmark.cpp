#include "core/batch.hpp"
#include "json.hpp"
#include "platform/atomic_file.hpp"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("Usage: wb_bench OUTPUT_DIRECTORY");
        const auto cases = wb::generate_batch(128, 8000, 42);
        std::atomic<bool> cancel{false};
        const auto reference = wb::solve_batch(cases, 1, cancel);
        const unsigned logical = std::thread::hardware_concurrency();
        const unsigned cap = std::max(1U, std::min(4U, logical));
        std::vector<unsigned> counts{1};
        if (cap >= 2) counts.push_back(2);
        if (cap > 2) counts.push_back(cap);
        nlohmann::json report = {{"seed",42}, {"cases",128}, {"elements",8000},
            {"logical_cpus",logical}, {"cpu_model","not measured"},
            {"physical_cores","not measured"}, {"ram_bytes","not measured"},
            {"scope","development machine; not clean Windows acceptance"},
            {"protocol","warm-up then five repetitions; steady_clock; generation, comparison and file IO excluded"}};
#ifdef _MSC_VER
        report["compiler"] = "MSVC " + std::to_string(_MSC_VER);
#else
        report["compiler"] = __VERSION__;
#endif
#ifdef NDEBUG
        report["build_mode"] = "NDEBUG; exact optimization flags recorded in CMake build configuration";
#else
        report["build_mode"] = "debug; do not use for final performance claims";
#endif
        using Clock = std::chrono::steady_clock;
        unsigned batches = 1;
        for (;;) {
            const auto begin = Clock::now();
            for (unsigned i = 0; i < batches; ++i) (void)wb::solve_batch(cases, 1, cancel);
            if (Clock::now() - begin >= std::chrono::milliseconds(750) || batches >= 1024) break;
            batches *= 2;
        }
        report["batches_per_repetition"] = batches;
        report["cases_per_repetition"] = batches * cases.size();
#ifdef __APPLE__
        report["scope"] = "development Mac";
#elif defined(_WIN32)
        report["scope"] = "Windows benchmark host";
#endif
        double baseline = 0;
        for (const auto workers : counts) {
            (void)wb::solve_batch(cases, workers, cancel);
            std::vector<double> milliseconds;
            double error = 0;
            for (int repetition = 0; repetition < 5; ++repetition) {
                double compute_ms = 0;
                for (unsigned batch = 0; batch < batches; ++batch) {
                    const auto start = Clock::now();
                    const auto result = wb::solve_batch(cases, workers, cancel);
                    compute_ms += std::chrono::duration<double, std::milli>(Clock::now()-start).count();
                    error = std::max(error, wb::batch_difference(reference, result));
                }
                milliseconds.push_back(compute_ms);
            }
            if (error > 1e-8) throw std::runtime_error("Parallel agreement failed");
            auto ordered = milliseconds;
            std::sort(ordered.begin(), ordered.end());
            const double median = ordered[2];
            if (workers == 1) baseline = median;
            report["measurements"].push_back({{"workers",workers}, {"raw_ms",milliseconds},
                {"median_ms",median}, {"speedup",baseline/median},
                {"efficiency",baseline/median/workers}, {"max_difference_C",error}});
        }
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);
        std::ostringstream csv;
        csv.precision(17);
        csv << "workers,repetition,time_ms\n";
        for (const auto& row : report["measurements"]) {
            int repetition = 0;
            for (const auto& time : row["raw_ms"])
                csv << row["workers"] << ',' << ++repetition << ',' << time << '\n';
        }
        wb::atomic_write(directory/"perf.csv", csv.str(), cancel);
        wb::atomic_write(directory/"perf.json", report.dump(2)+"\n", cancel);
        std::cout << report.dump(2) << '\n';
        return 0;
    } catch (const std::exception&) {
        std::cerr << "Benchmark failed; check output directory and resource limits.\n";
        return 1;
    }
}
