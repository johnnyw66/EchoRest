#ifndef _BEEPER_H
#define _BEEPER_H

#include <driver/i2s.h>
//#include <Arduino.h>

#define SPEAK_I2S_NUMBER I2S_NUM_0
size_t playBeep(int __freq = 2000, int __timems = 200, int __maxval = 10000);

#endif
