#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Ticker.h>
#include <Wire.h>

#include "accessory.h"
#include "debug.h"
#include "env_sensor.h"
#include "heatpump_client.h"
#include "mqtt.h"
#include "settings.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ====== CONFIG ======
static const char* NAME_PREFIX = "GVH5100_";   // change if yours differs
static constexpr uint32_t SCAN_EVERY_MS    = 60000; // once a minute
static constexpr uint32_t SCAN_DURATION_MS = 3000;  // 2–3s usually catches a beacon

// Shared state updated by callback
static volatile float    g_tempC = NAN;
static volatile float    g_hum   = NAN;
static volatile int      g_batt  = -1;
static volatile uint32_t g_lastSeenMs = 0;
static char g_name[32];

static BLEScan* pScan = nullptr;

#define SAMPLE_INTERVAL 10

static Adafruit_BME280 bme;

static DHT_Unified dht(D4, DHT22);
static DHT_Unified::Temperature dhtTemperature = dht.temperature();
static DHT_Unified::Humidity dhtHumidity = dht.humidity();

static Adafruit_Sensor *temperatureSensor = nullptr;
static Adafruit_Sensor *humiditySensor = nullptr;

static Ticker ticker;

char env_sensor_status[30] = {0};

class GoveeAdvertisedCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice d) override {
        // Filter by name prefix (note: name may not be present in every packet)
        std::string name = d.getName();
        if (name.empty()) return;
        if (name.rfind(NAME_PREFIX, 0) != 0) return;

        if (!d.haveManufacturerData()) return;

        std::string mfg = d.getManufacturerData();
        const uint8_t* b = (const uint8_t*)mfg.data();
        const size_t n = mfg.size();
    
        if (n < 9) return;
        if (b[0] != 0x88 || b[1] != 0xEC) return;
    
        // ---- Decode packed temp/humidity ----
        uint32_t raw24 =
            ((uint32_t)b[5] << 16) |
            ((uint32_t)b[6] << 8)  |
            (uint32_t)b[7];
    
        float tempC = (raw24 / 1000) / 10.0f;
        float hum   = (raw24 % 1000) / 10.0f;
        int batt    = (int)b[8];
    
        g_tempC = tempC;
        g_hum   = hum;
        g_batt  = batt;
        g_lastSeenMs = millis();
    
        strncpy(g_name, name.c_str(), sizeof(g_name) - 1);
        g_name[sizeof(g_name) - 1] = '\0';
    }
    };
  
static void startScanBurst() {
    // Short scan burst; BLEScan::start() is blocking for the duration,
    // so keep duration small to avoid impacting responsiveness.
    pScan->start(SCAN_DURATION_MS / 1000, /*is_continue=*/false);
    //pScan->stop();
    pScan->clearResults();
}
  
static double dew_point(double t, double r) {
    // Magnus-Tetens approximation
    double a = 17.27;
    double b = 237.7;
    double alpha = ((a * t) / (b + t)) + log(r / 100);
    return (b * alpha) / (a - alpha);
}

static void env_sensor_update() {
    float temperature = 0;
    float humidity = 0;
    float dewPoint = 0;
    if( pScan) {
        startScanBurst();
        temperature = g_tempC;
        humidity = g_hum;
        dewPoint = dew_point(temperature, humidity);
        snprintf(env_sensor_status, sizeof(env_sensor_status), "%s %.2fºC %.2f%%RH", g_name, temperature, humidity);
    } else {
        sensors_event_t temperatureEvent;
        temperatureSensor->getEvent(&temperatureEvent);
        sensors_event_t humidityEvent;
        humiditySensor->getEvent(&humidityEvent);

        temperature = temperatureEvent.temperature;
        humidity = humidityEvent.relative_humidity;
        dewPoint = dew_point(temperature, humidity);

        sensor_t sensor;
        temperatureSensor->getSensor(&sensor);
        snprintf(env_sensor_status, sizeof(env_sensor_status), "%s %.2fºC %.2f%%RH", sensor.name, temperature, humidity);
    } 

    if (mqtt_connect()) {
        char str[6];
        if (strlen(settings.mqtt_temp) && std::isnormal(temperature)) {
            snprintf(str, sizeof(str), "%.2f", temperature);
            mqtt.publish(settings.mqtt_temp, str);
        }
        if (strlen(settings.mqtt_humidity) && std::isnormal(humidity)) {
            snprintf(str, sizeof(str), "%.2f", humidity);
            mqtt.publish(settings.mqtt_humidity, str);
        }
        if (strlen(settings.mqtt_dew_point) && std::isnormal(dewPoint)) {
            snprintf(str, sizeof(str), "%.2f", dewPoint);
            mqtt.publish(settings.mqtt_dew_point, str);
        }
    }

    accessory_set_float(&ch_dehumidifier_relative_humidity, humidity, true);
    accessory_set_float(&ch_dew_point, dewPoint, true);
    if (!heatpump.isConnected()) {
        accessory_set_float(&ch_thermostat_current_temperature, temperature, true);
    }
}

void env_sensor_init() {
    sensors_event_t temperatureEvent;
    sensors_event_t humidityEvent;

    if (bme.begin(BME280_ADDRESS_ALTERNATE)) {
        bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                Adafruit_BME280::SAMPLING_X1,
                Adafruit_BME280::SAMPLING_NONE,
                Adafruit_BME280::SAMPLING_X1,
                Adafruit_BME280::FILTER_OFF,
                Adafruit_BME280::STANDBY_MS_1000);

        temperatureSensor = bme.getTemperatureSensor();
        humiditySensor = bme.getHumiditySensor();

        temperatureSensor->getEvent(&temperatureEvent);
        humiditySensor->getEvent(&humidityEvent);

        if (!isnan(temperatureEvent.temperature) && !isnan(humidityEvent.relative_humidity)) {
            strlcpy(env_sensor_status, "BME280", sizeof(env_sensor_status));
            MIE_LOG("Found BME280 sensor: %.1fºC %.1f%% RH",
                    temperatureEvent.temperature,
                    humidityEvent.relative_humidity);
        } else {
            temperatureSensor = nullptr;
            humiditySensor = nullptr;
        }
    } else {
        dht.begin();
        sensor_t sensor;
        dhtTemperature.getSensor(&sensor);
        delay(sensor.min_delay / 1000);

        dhtTemperature.getEvent(&temperatureEvent);
        dhtHumidity.getEvent(&humidityEvent);

        if (!isnan(temperatureEvent.temperature) && !isnan(humidityEvent.relative_humidity)) {
            strlcpy(env_sensor_status, "DHT22", sizeof(env_sensor_status));
            MIE_LOG("Found DHT22 sensor: %.1fºC %.1f%% RH",
                    temperatureEvent.temperature,
                    humidityEvent.relative_humidity);

            temperatureSensor = &dhtTemperature;
            humiditySensor = &dhtHumidity;
        }
    }

    if( !temperatureSensor && ! humiditySensor) {
        BLEDevice::init("");

        pScan = BLEDevice::getScan();
        pScan->setAdvertisedDeviceCallbacks(new GoveeAdvertisedCallbacks(), /*wantDuplicates=*/true);
      
        // Passive scan reduces RF chatter and Wi-Fi impact
        pScan->setActiveScan(false);
      
        // Gentle scan parameters (ms) to reduce contention on single-core + Wi-Fi
        pScan->setInterval(1200);
        pScan->setWindow(120); // ~10% duty during scan bursts
    }

    if (pScan || (temperatureSensor && humiditySensor)) {
#ifdef ESP32
        ticker.attach(SAMPLE_INTERVAL, env_sensor_update);
#else
        ticker.attach_scheduled(SAMPLE_INTERVAL, env_sensor_update);
#endif
    } else {
        MIE_LOG("No temperature and humidity sensors found");
    }
}
