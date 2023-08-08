#ifndef __DEVICE_H
#define __DEVICE_H

#include <Arduino.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <AsyncJson.h>
#include <ArduinoJson.h>

#include <FastLED.h>

void devices_web_init(AsyncWebServer *);

void devices_init_before();
void devices_init_after();
void devices_handle();

// led

#define LED_PIN 47
#define LED_BRIGHTNESS 128

#define LED_STATE_INIT CRGB::Red
#define LED_STATE_AP CRGB::Green
#define LED_STATE_STA CRGB::DarkBlue
#define LED_STATE_NONE CRGB::Black
#define LED_STATE_OTA CRGB::LightGoldenrodYellow

void set_led_state(CRGB);

// player

#define VS1053_CS 8
#define VS1053_DCS 6
#define VS1053_DREQ 7

#define VS1053_VOLUME 100

// transmitter
#define FM_FREQ 93200

#endif