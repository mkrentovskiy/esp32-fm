#!/bin/bash
export $(grep -v '^#' .env | xargs -d '\n')
curl -X GET  http://esp32-$MAC_ADDR.local/wifi_reset | jq
