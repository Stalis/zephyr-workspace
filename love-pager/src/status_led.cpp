#include "status_led.h"

StatusLED::StatusLED(gpio_dt_spec& led) 
    : _led(led)
{}

int StatusLED::init() {
    return 0;
}

void StatusLED::setState(State newState) {
    getPattern
}

