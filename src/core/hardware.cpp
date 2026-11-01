#include "hardware.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace wb {
std::vector<DeviceInfo> discover_mock_devices() {
    return {{"sim-mcu", "MCU", {"temperature", "gpio-simulation"}},
            {"sim-fpga", "FPGA", {"counter", "logic-simulation"}},
            {"sim-jtag", "JTAG", {"target-voltage", "debug-interface-only"}}};
}
MockDevice::MockDevice(DeviceInfo info, bool fail_first)
    : info_(std::move(info)), fail_first_(fail_first) {
    if (info_.kind != "MCU" && info_.kind != "FPGA" && info_.kind != "JTAG")
        throw std::invalid_argument("Unsupported simulated device kind");
}
void MockDevice::connect(std::uint64_t now) {
    if (state_ != DeviceState::disconnected) throw std::logic_error("Disconnect before connecting");
    if (now < last_ || now > std::numeric_limits<std::uint64_t>::max() - 1000)
        throw std::invalid_argument("Invalid monotonic time");
    last_ = now;
    deadline_ = now + 100;
    state_ = DeviceState::connecting;
    log_.push_back("SIMULATED: connecting");
}
void MockDevice::poll(std::uint64_t now) {
    if (now < last_ || now > std::numeric_limits<std::uint64_t>::max() - 1000)
        throw std::invalid_argument("Invalid monotonic time");
    last_ = now;
    if (now < deadline_) return;
    if (state_ == DeviceState::connecting) {
        if (fail_first_) {
            fail_first_ = false;
            state_ = DeviceState::backoff;
            deadline_ = now + 250;
            log_.push_back("SIMULATED: timeout; retry after 250 ms backoff");
        } else {
            state_ = DeviceState::connected;
            log_.push_back("SIMULATED: connected; telemetry available");
        }
    } else if (state_ == DeviceState::backoff) {
        state_ = DeviceState::connecting;
        deadline_ = now + 100;
        log_.push_back("SIMULATED: retry connecting");
    }
}
void MockDevice::disconnect() {
    if (state_ == DeviceState::disconnected) throw std::logic_error("Already disconnected");
    state_ = DeviceState::disconnected;
    log_.push_back("SIMULATED: disconnected");
}
Telemetry MockDevice::telemetry() const {
    if (state_ != DeviceState::connected) throw std::logic_error("Connect before requesting telemetry");
    if (info_.kind == "MCU") return {info_.id, "temperature", "degC", 25.0, true};
    if (info_.kind == "FPGA") return {info_.id, "counter", "ticks", 1024.0, true};
    return {info_.id, "target-voltage", "V", 3.3, true};
}
}
