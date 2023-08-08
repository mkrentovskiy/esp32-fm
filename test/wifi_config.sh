#!/bin/bash
export $(grep -v '^#' .env | xargs -d '\n')
curl -X POST http://192.168.4.1/wifi_config \
   -H 'Content-Type: application/json' \
   -d "{ \"ssid\": \"$WIFI_SSID\", \"key\": \"$WIFI_KEY\" }" | jq
