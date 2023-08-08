#include <Arduino.h>

#include <VS1053.h>
#include <ESP32_VS1053_Stream.h>

#include "si47xx.h"

#include "config.h"
#include "con.h"
#include "devices.h"


CRGB led[1];
ESP32_VS1053_Stream stream;
Si47xx mpx;

String stream_url = "http://nashe1.hostingradio.ru/nashe-256";
String station_ps = "HAIIIE";

/*
    inteface
*/

void devices_web_init(AsyncWebServer *server)
{

}

/*
    external functions
*/

void devices_init_before()
{
    // led
    FastLED.addLeds<WS2812B, LED_PIN>(led, 1);
    set_led_state(LED_STATE_NONE);
}

void devices_init_after()
{
    // player
    SPI.begin();
    stream.startDecoder(VS1053_CS, VS1053_DCS, VS1053_DREQ);
    stream.setVolume(VS1053_VOLUME);
    delay(1000);

    // transmitter
    if(mpx.begin()) {
        mpx.tune_fm(FM_FREQ);
        mpx.set_tx_power(120);
        mpx.begin_rds();
        mpx.set_rds_station(station_ps.c_str());
    } else {
        ESP_LOGE(TAG, "Can't start FM transmitter");
    }
}

void devices_handle()
{
    if(!stream.isRunning()) {
        if(con_state() == CON_STATE_CLIENT) {
            ESP_LOGI(TAG, "Starting stream %s", stream_url.c_str());
            stream.connecttohost(stream_url.c_str());
        } else {
            ESP_LOGW(TAG, "Can't start %s - not connected to AP", stream_url.c_str());
        }
    } else {
        stream.loop();
    };
}

/*
    led functions
*/

void set_led_state(CRGB color)
{
  led[0] = color;
  FastLED.setBrightness(LED_BRIGHTNESS);
  FastLED.show();
  delay(50);
}

/*
    player functions
*/

void audio_showstation(const char* info) {
    ESP_LOGW(TAG, "Station - %s", info);
}

void audio_showstreamtitle(const char* info) {
    ESP_LOGI(TAG, "Stream Title - %s", info);
}

void audio_eof_stream(const char* info) {
    ESP_LOGW(TAG, "EOF - %s", info);
}
