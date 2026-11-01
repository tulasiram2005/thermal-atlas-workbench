#pragma once
#include <atomic>
#include <filesystem>
#include <string_view>

namespace wb {
// Fault injection is internal test support, never an HTTP option.
enum class WriteFault { none, after_write, before_publish };
void atomic_write(const std::filesystem::path& destination, std::string_view bytes,
                  const std::atomic<bool>& cancelled, WriteFault fault = WriteFault::none);
}
