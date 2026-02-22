#include <Arduino.h>
#include <Ticker.h>

#include "debug.h"
#include "env_sensor.h"
#include "heatpump_client.h"
#include "homekit.h"
#ifndef XIAO_ESP32C3
#include "led_status_patterns.h"
#endif
#include "mqtt.h"
#include "ntp_clock.h"
#include "settings.h"
#include "web.h"
#include "wifi_manager.h"

#define NAME_PREFIX "Heat Pump "
#define HOSTNAME_PREFIX "heat-pump-"

char name[25];
char hostname[25];

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println( "serial started");
    //wifiManager.resetSettings();
#ifndef XIAO_ESP32C3
    led_status_init(LED_BUILTIN, false);
#endif

#ifdef ESP32
    sprintf(name, NAME_PREFIX "%06x", ESP.getEfuseMac());
    sprintf(hostname, HOSTNAME_PREFIX "%06x", ESP.getEfuseMac());
#else
    sprintf(name, NAME_PREFIX "%06x", ESP.getChipId());
    sprintf(hostname, HOSTNAME_PREFIX "%06x", ESP.getChipId());
#endif

    Serial.println( "initialize settings");
    settings_init();
    Serial.println( "initialize debug");
    wifi_init(name);
    debug_init(name);
    ntp_clock_init();
    web_init(hostname);
    mqtt_init(name);
    env_sensor_init();

    homekit_init(name, loop);

    if (!heatpump_init()) {
#ifndef XIAO_ESP32C3
        led_status_signal(&status_led_error);
#endif
    }

#ifndef XIAO_ESP32C3
    led_status_done();
#endif
}

void loop() {
    web_loop();
    homekit_loop();
    mqtt_loop();
    debug_loop();
}
