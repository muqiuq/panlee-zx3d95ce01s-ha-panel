#pragma once

#include <Arduino.h>

namespace hw {

// Built-in SHT20 temperature/humidity sensor on the touch I2C bus (I2C1, SDA 15 / SCL 6, addr 0x40).
// Shares the bus with the touch controller, so it must only be used from the main loop.
bool climateBegin(bool busReady);
void climateLoop();
bool climateAvailable();
// Raw readings without configured offsets; NAN until the first measurement.
float climateTemperature();
float climateHumidity();

}  // namespace hw
