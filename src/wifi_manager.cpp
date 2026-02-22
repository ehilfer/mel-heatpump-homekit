#include "wifi_manager.h"

#ifndef ESP32
#include <DoubleResetDetect.h>
#endif
#include <Ticker.h>

#include "debug.h"
#include "homekit.h"
#ifndef XIAO_ESP32C3
#include "led_status_patterns.h"
#endif

WiFiManager wifiManager;

#ifndef ESP32
#define DRD_TIMEOUT 2.0
#define DRD_ADDRESS 0x00
static DoubleResetDetect drd(DRD_TIMEOUT, DRD_ADDRESS);
#endif

static void wifi_did_enter_config_mode(WiFiManager *wifiManager) {
#ifndef XIAO_ESP32C3
    led_status_set(&status_led_waiting_wifi);
#endif
}

void wifi_init(const char* ssid) {
#ifdef ESP32
    WiFi.setSleep(false);
#else
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#endif

    wifiManager.setAPCallback(wifi_did_enter_config_mode);
    wifiManager.setConnectTimeout(30);
    wifiManager.setSaveConnectTimeout(30);
    wifiManager.setConfigPortalTimeout(120);

#ifndef ESP32
    if (drd.detect()) {
#ifndef XIAO_ESP32C3
        led_status_signal(&status_led_double_reset);
#endif
        while (!wifiManager.startConfigPortal(ssid)) {
            MIE_LOG("WiFi config portail timed out, restarting");
            delay(1000);
            ESP.restart();
        }
    } else {
#endif
        while (!wifiManager.autoConnect(ssid)) {
            MIE_LOG("WiFi connection failed, trying again");
        }
#ifndef ESP32
    }
#endif

    WiFi.mode(WIFI_STA);
    MIE_LOG("WiFi Connection successful");
    MIE_LOG("IP: %s", WiFi.localIP().toString().c_str());
}
