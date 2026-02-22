#include <Arduino.h>
#include <TimeLib.h>
#ifndef ESP32
#include <TZ.h>
#include <coredecls.h>
#endif

#include "ntp_clock.h"
#include "debug.h"


static bool timeWasSet = false;

void ntp_clock_init() {
#ifdef ESP32
    configTime(0, 0, "pool.ntp.org");
#else
    configTime(TZ_Etc_UTC, "pool.ntp.org");

    settimeofday_cb([] {
        timeWasSet = true;
        setTime(time(nullptr));
    });

    MIE_LOG("Syncing NTP time");
    int timeout = 4;
    int tick = 0;
    while (!timeWasSet && tick++ < timeout) {
        delay(500);
    }
    if (!timeWasSet) {
        MIE_LOG("NTP timed out");
    }
#endif

    setSyncProvider([] { return time(nullptr); });
}
