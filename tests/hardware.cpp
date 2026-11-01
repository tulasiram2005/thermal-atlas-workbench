#include <stdexcept>
#include "core/hardware.hpp"
#include "doctest.h"
#include <limits>

TEST_CASE("All mock devices expose simulated telemetry and capabilities") {
    const auto devices = wb::discover_mock_devices();
    REQUIRE(devices.size() == 3);
    for (const auto& info : devices) {
        CHECK_FALSE(info.capabilities.empty());
        wb::MockDevice device(info);
        CHECK_THROWS_AS(device.telemetry(), std::logic_error);
        device.connect(0);
        CHECK(device.state() == wb::DeviceState::connecting);
        CHECK_THROWS_AS(device.connect(1), std::logic_error);
        device.poll(99);
        CHECK(device.state() == wb::DeviceState::connecting);
        device.poll(100);
        CHECK(device.state() == wb::DeviceState::connected);
        CHECK(device.telemetry().simulated);
        CHECK(device.telemetry().device == info.id);
        CHECK_FALSE(device.telemetry().unit.empty());
        device.disconnect();
        CHECK(device.state() == wb::DeviceState::disconnected);
        CHECK_THROWS_AS(device.disconnect(), std::logic_error);
    }
}
TEST_CASE("Simulated timeout recovers only after bounded backoff") {
    wb::MockDevice device(wb::discover_mock_devices()[0], true);
    device.connect(0);
    device.poll(100);
    CHECK(device.state() == wb::DeviceState::backoff);
    CHECK_THROWS_AS(device.telemetry(), std::logic_error);
    device.poll(349);
    CHECK(device.state() == wb::DeviceState::backoff);
    device.poll(350);
    CHECK(device.state() == wb::DeviceState::connecting);
    device.poll(450);
    CHECK(device.state() == wb::DeviceState::connected);
    CHECK(device.log().size() == 4);
    CHECK_THROWS_AS(device.poll(400), std::invalid_argument);
    CHECK_THROWS_AS(device.poll(std::numeric_limits<std::uint64_t>::max()), std::invalid_argument);
}
