#!/bin/bash
export $(grep -v '^#' .env | xargs -d '\n')
curl -X POST http://esp32-$MAC_ADDD.local/ota \
  -F "MD5=$FW_MD5" \
  -F "file=@bin/firmware.bin" \
