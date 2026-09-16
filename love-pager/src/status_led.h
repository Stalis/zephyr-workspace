#pragma once

#include <zephyr/drivers/gpio.h>
#include <array>
#include <span>

class StatusLED {
public:
    enum class State : uint8_t {
        Idle = 0,
        MessageReceived,
    };

    StatusLED(gpio_dt_spec&);
    int init();
    void setState(State);
private:
    gpio_dt_spec& _led;
    State _currentState;
    k_msgq _messageQueue;

    struct BlinkPattern {
        State state;
        std::span<const uint16_t> msDelays;
    };

    static constexpr std::array<uint16_t, 2> IdlePattern = {100, 2000};
    static constexpr std::array<uint16_t, 4> MessageReceivedPattern = {100, 100, 100, 500};

    inline static constexpr std::array<BlinkPattern, 2> PATTERNS = {
        BlinkPattern{ .state = State::Idle, .msDelays = IdlePattern},
        BlinkPattern{ .state = State::MessageReceived, .msDelays = MessageReceivedPattern}
    };

    static constexpr const BlinkPattern& getPattern(State state) {
        return PATTERNS[static_cast<uint8_t>(state)];
    };
};