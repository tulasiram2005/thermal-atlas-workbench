#include <string>
#include "atomic_file.hpp"
#include "core/batch.hpp"
#include <cstdio>
#include <algorithm>
#include <random>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace wb {
void atomic_write(const std::filesystem::path& destination, std::string_view bytes,
                  const std::atomic<bool>& cancelled, WriteFault fault) {
    if (cancelled.load()) throw Cancelled();
    if (destination.filename().empty()) throw std::invalid_argument("Missing export name");
    const auto parent = destination.has_parent_path() ? destination.parent_path() : std::filesystem::path(".");
    // Exclusively create a sibling directory; this prevents concurrent writers from
    // sharing a temporary file and keeps the rename on the same filesystem.
    std::random_device random;
    std::filesystem::path staging;
    for (int attempt = 0; attempt < 32; ++attempt) {
        auto candidate = parent / (".wb-export-" + std::to_string(random()) + "-" + std::to_string(random()));
        if (std::filesystem::create_directory(candidate)) { staging = candidate; break; }
    }
    if (staging.empty()) throw std::runtime_error("Unable to create export staging area");
    const auto temporary = staging / "payload";
    FILE* stream = nullptr;
    try {
#ifdef _WIN32
        if (_wfopen_s(&stream, temporary.c_str(), L"wb") != 0) stream = nullptr;
#else
        stream = std::fopen(temporary.c_str(), "wb");
#endif
        if (!stream) throw std::runtime_error("Unable to create export");
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            if (cancelled.load()) throw Cancelled();
            const auto size = std::min<std::size_t>(65536, bytes.size() - offset);
            if (std::fwrite(bytes.data() + offset, 1, size, stream) != size)
                throw std::runtime_error("Unable to write export");
            offset += size;
            if (fault == WriteFault::after_write) throw std::runtime_error("Injected write failure");
        }
        if (std::fflush(stream) != 0) throw std::runtime_error("Unable to flush export");
#ifdef _WIN32
        if (_commit(_fileno(stream)) != 0) throw std::runtime_error("Unable to sync export");
#else
        if (fsync(fileno(stream)) != 0) throw std::runtime_error("Unable to sync export");
#endif
        const int closed = std::fclose(stream);
        stream = nullptr;
        if (closed != 0) throw std::runtime_error("Unable to close export");
        if (cancelled.load()) throw Cancelled();
        if (fault == WriteFault::before_publish) throw std::runtime_error("Injected publication failure");
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Unable to publish export");
#else
        std::filesystem::rename(temporary, destination);
#endif
        std::error_code ignored;
        std::filesystem::remove(staging, ignored);
    } catch (...) {
        if (stream) std::fclose(stream);
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
        throw;
    }
}
}
