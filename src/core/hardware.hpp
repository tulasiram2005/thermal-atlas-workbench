#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace wb {
enum class DeviceState { disconnected, connecting, backoff, connected };
struct DeviceInfo { std::string id, kind; std::vector<std::string> capabilities; };
struct Telemetry { std::string device, metric, unit; double value; bool simulated = true; };
class Device {
public:
    virtual ~Device() = default;
    virtual const DeviceInfo& info() const = 0;
    virtual DeviceState state() const = 0;
    virtual void connect(std::uint64_t milliseconds) = 0;
    virtual void poll(std::uint64_t milliseconds) = 0;
    virtual void disconnect() = 0;
    virtual Telemetry telemetry() const = 0;
};
class MockDevice final : public Device {
public:
    explicit MockDevice(DeviceInfo info, bool fail_first = false);
    const DeviceInfo& info() const override { return info_; }
    DeviceState state() const override { return state_; }
    void connect(std::uint64_t milliseconds) override;
    void poll(std::uint64_t milliseconds) override;
    void disconnect() override;
    Telemetry telemetry() const override;
    const std::vector<std::string>& log() const { return log_; }
private:
    DeviceInfo info_;
    DeviceState state_ = DeviceState::disconnected;
    bool fail_first_;
    std::uint64_t last_ = 0, deadline_ = 0;
    std::vector<std::string> log_;
};
std::vector<DeviceInfo> discover_mock_devices();
}
