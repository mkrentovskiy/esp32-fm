/*
Silicon Devices Si47xx FM Transmitter Driver Library. Based on the
Adafruit Si47xx library (https://github.com/adafruit/Adafruit-Si47xx-Library/),
but significantly simplified and cleaned up, and with support for some
additional command modes.

Copyright (C) 2021  Robert Ussery

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <Wire.h>

#include "si47xx.h"

#define TAG "si47xx"

constexpr unsigned int SI4710_STATUS_CTS PROGMEM = 0x80;

constexpr unsigned int CMD_POWER_UP PROGMEM = 0x01;
constexpr unsigned int CMD_GET_REV PROGMEM = 0x10;
constexpr unsigned int CMD_POWER_DOWN PROGMEM = 0x11;
constexpr unsigned int CMD_SET_PROPERTY PROGMEM = 0x12;
constexpr unsigned int CMD_GET_PROPERTY PROGMEM = 0x13;
constexpr unsigned int CMD_GET_INT_STATUS PROGMEM = 0x14;
constexpr unsigned int CMD_PATCH_ARGS PROGMEM = 0x15;
constexpr unsigned int CMD_PATCH_DATA PROGMEM = 0x16;
constexpr unsigned int CMD_TX_TUNE_FREQ PROGMEM = 0x30;
constexpr unsigned int CMD_TX_TUNE_POWER PROGMEM = 0x31;
constexpr unsigned int CMD_TX_TUNE_MEASURE PROGMEM = 0x32;
constexpr unsigned int CMD_TX_TUNE_STATUS PROGMEM = 0x33;
constexpr unsigned int CMD_TX_ASQ_STATUS PROGMEM = 0x34;
constexpr unsigned int CMD_TX_RDS_BUFF PROGMEM = 0x35;
constexpr unsigned int CMD_TX_RDS_PS PROGMEM = 0x36;
constexpr unsigned int CMD_GPO_CTL PROGMEM = 0x80;
constexpr unsigned int CMD_GPO_SET PROGMEM = 0x81;

constexpr unsigned int PROP_GPO_IEN PROGMEM = 0x0001;
constexpr unsigned int PROP_DIGITAL_INPUT_FORMAT PROGMEM = 0x0101;
constexpr unsigned int PROP_DIGITAL_INPUT_SAMPLE_RATE PROGMEM = 0x0103;
constexpr unsigned int PROP_REFCLK_FREQ PROGMEM = 0x0201;
constexpr unsigned int PROP_REFCLK_PRESCALE PROGMEM = 0x0202;
constexpr unsigned int PROP_TX_COMPONENT_ENABLE PROGMEM = 0x2100;
constexpr unsigned int PROP_TX_AUDIO_DEVIATION PROGMEM = 0x2101;
constexpr unsigned int PROP_TX_PILOT_DEVIATION PROGMEM = 0x2102;
constexpr unsigned int PROP_TX_RDS_DEVIATION PROGMEM = 0x2103;
constexpr unsigned int PROP_TX_LINE_LEVEL_INPUT_LEVEL PROGMEM = 0x2104;
constexpr unsigned int PROP_TX_LINE_INPUT_MUTE PROGMEM = 0x2105;
constexpr unsigned int PROP_TX_PREEMPHASIS PROGMEM = 0x2106;
constexpr unsigned int PROP_TX_PILOT_FREQUENCY PROGMEM = 0x2107;
constexpr unsigned int PROP_TX_ACOMP_ENABLE PROGMEM = 0x2200;
constexpr unsigned int PROP_TX_ACOMP_THRESHOLD PROGMEM = 0x2201;
constexpr unsigned int PROP_TX_ATTACK_TIME PROGMEM = 0x2202;
constexpr unsigned int PROP_TX_RELEASE_TIME PROGMEM = 0x2203;
constexpr unsigned int PROP_TX_ACOMP_GAIN PROGMEM = 0x2204;
constexpr unsigned int PROP_TX_LIMITER_RELEASE_TIME PROGMEM = 0x2205;
constexpr unsigned int PROP_TX_ASQ_INTERRUPT_SOURCE PROGMEM = 0x2300;
constexpr unsigned int PROP_TX_ASQ_LEVEL_LOW PROGMEM = 0x2301;
constexpr unsigned int PROP_TX_ASQ_DURATION_LOW PROGMEM = 0x2302;
constexpr unsigned int PROP_TX_AQS_LEVEL_HIGH PROGMEM = 0x2303;
constexpr unsigned int PROP_TX_AQS_DURATION_HIGH PROGMEM = 0x2304;
constexpr unsigned int PROP_TX_RDS_INTERRUPT_SOURCE PROGMEM = 0x2C00;
constexpr unsigned int PROP_TX_RDS_PI PROGMEM = 0x2C01;
constexpr unsigned int PROP_TX_RDS_PS_MIX PROGMEM = 0x2C02;
constexpr unsigned int PROP_TX_RDS_PS_MISC PROGMEM = 0x2C03;
constexpr unsigned int PROP_TX_RDS_PS_REPEAT_COUNT PROGMEM = 0x2C04;
constexpr unsigned int PROP_TX_RDS_MESSAGE_COUNT PROGMEM = 0x2C05;
constexpr unsigned int PROP_TX_RDS_PS_AF PROGMEM = 0x2C06;
constexpr unsigned int PROP_TX_RDS_FIFO_SIZE PROGMEM = 0x2C07;


void _request_bytes(uint8_t num_bytes) {
  uint8_t res = Wire.requestFrom(SI47XX_I2C_ADDR, num_bytes);
  // ESP_LOGI(TAG, "Request %d bytes with result %d", num_bytes, res);
  if(res == num_bytes) 
    return;

  int k = 0;
  while((Wire.available() < num_bytes) && (k < (SI47XX_MAX_AWAIT / SI47XX_STEP_AWAIT))) {
    // ESP_LOGI(TAG, "Wait for %d - availible %d", num_bytes, Wire.available());
    delay(SI47XX_STEP_AWAIT);
    k++;
  }
}

void Si47xx::_send_command(unsigned int len, bool need_status) {
  // ESP_LOGI(TAG, "Send cmd %x with common len %d", _cmd_buff[0], len);
  
  Wire.beginTransmission(SI47XX_I2C_ADDR);
  for (uint8_t i = 0; i < min(len, SI47XX_BUF_SIZE); i++) {
    Wire.write(_cmd_buff[i]);
  }
  Wire.endTransmission();
  
  if(need_status) {
    // Wait for status CTS bit, indicating command is complete:
    uint8_t status = 0;
    int k = 0;

    while (!(status & SI4710_STATUS_CTS) && (k < (SI47XX_MAX_AWAIT / SI47XX_STEP_AWAIT))) {
      if(Wire.requestFrom(SI47XX_I2C_ADDR, 1) == 1) {
        Wire.readBytes(&status, 1);
        // ESP_LOGI(TAG, "[%d] Cmd status - %x", k, status); 
      }
      delay(SI47XX_STEP_AWAIT);
      k++;
    }
  }
}

void Si47xx::_set_property(unsigned int property, unsigned int value) {
  _cmd_buff[0] = CMD_SET_PROPERTY;
  _cmd_buff[1] = 0;
  _cmd_buff[2] = property >> 8;
  _cmd_buff[3] = property & 0xFF;
  _cmd_buff[4] = value >> 8;
  _cmd_buff[5] = value & 0xFF;
  
  _send_command(6);
}

unsigned int Si47xx::_get_status(void) {
  Wire.beginTransmission(SI47XX_I2C_ADDR);
  Wire.write(CMD_GET_INT_STATUS);
  Wire.endTransmission();

  _request_bytes(1);
  return Wire.read();
}


bool Si47xx::begin() {
  pinMode(SI47XX_PIN_RESET, OUTPUT);
  digitalWrite(SI47XX_PIN_RESET, HIGH);
  delay(10);
  digitalWrite(SI47XX_PIN_RESET, LOW);
  delay(10);
  digitalWrite(SI47XX_PIN_RESET, HIGH);
  delay(100);

  Wire.begin();
  delay(100);
  
  _cmd_buff[0] = CMD_POWER_UP;
  // CTS interrupt disabled, GPO2 output disabled, boot normally, transmit mode:
  _cmd_buff[1] = 0x12; // Crystal osc enabled
  _cmd_buff[2] = 0x50; // Analog input mode
  _send_command(3);

  // Check communications with Si47xx:
  _cmd_buff[0] = CMD_GET_REV;
  _cmd_buff[1] = 0;
  _send_command(1);
  _request_bytes(8);
  
  uint8_t part_num =  Wire.read();
  uint8_t fw_major =  Wire.read();
  uint8_t fw_minor =  Wire.read();
  uint8_t patch_h =  Wire.read();
  uint8_t patch_l =  Wire.read();
  uint8_t cmp_major =  Wire.read();
  uint8_t cmp_minor =  Wire.read();
  uint8_t chip_rev =  Wire.read();
  
  ESP_LOGI(TAG, "Info: PN %d, FW %d.%d, PATCH %d, CMP %d.%d, Chip REV %d", 
    part_num, fw_major, fw_minor, (uint16_t) (patch_h << 8 && patch_l), 
    cmp_major, cmp_minor, chip_rev);
  
  if(part_num != SI47XX_CHIP_VERSION) {
    ESP_LOGI(TAG, "DETECTED WRONG CHIP VERSION: %d", part_num);
    return false; // Wrong chip version detected, bail out
  }

  _set_property(PROP_TX_PREEMPHASIS, 1); // 75µS pre-emph (USA std)
  _set_property(PROP_TX_ACOMP_ENABLE, 0x0003); // Turn on limiter and Audio Dynamic Range Control

  return true;
}

void Si47xx::tune_fm(unsigned int freq_kHz) {
  freq_kHz /= 10; // Convert to 10kHz
  
  // Force freq to be a multiple of 50kHz:
  if (freq_kHz % 5 != 0)
    freq_kHz -= (freq_kHz % 5);

  _cmd_buff[0] = CMD_TX_TUNE_FREQ;
  _cmd_buff[1] = 0;
  _cmd_buff[2] = freq_kHz >> 8;
  _cmd_buff[3] = freq_kHz;
  _send_command(4);

  // Wait for Seek/Tune Complete (STC) bit to be set:
  while ((_get_status() & 0x81) != 0x81)
    yield();
}

void Si47xx::set_tx_power(unsigned int pwr, unsigned int antcap) {
  _cmd_buff[0] = CMD_TX_TUNE_POWER;
  _cmd_buff[1] = 0;
  _cmd_buff[2] = 0;
  _cmd_buff[3] = pwr;
  _cmd_buff[4] = antcap;
  _send_command(5);

  // Wait for Seek/Tune Complete (STC) bit to be set:
  while ((_get_status() & 0x81) != 0x81)
    yield();
}

void Si47xx::read_asq_status(void) {
  _cmd_buff[0] = CMD_TX_ASQ_STATUS;
  _cmd_buff[1] = 0x1;
  _send_command(2);

  _request_bytes(5);
  Wire.read();
  CurrASQ = Wire.read();
  Wire.read();
  Wire.read();
  CurrInLevel = (int8_t) Wire.read();
}

void Si47xx::read_tune_status(void) {
  _cmd_buff[0] = CMD_TX_TUNE_STATUS;
  _cmd_buff[1] = 0x1;
  _send_command(2);

  _request_bytes(8);

  Wire.read();
  Wire.read();
  CurrFreq = Wire.read();
  CurrFreq <<= 8;
  CurrFreq |= Wire.read();
  Wire.read();
  CurrdBuV = Wire.read();
  CurrAntCap = Wire.read();
  CurrNoiseLevel = Wire.read();
}

void Si47xx::read_tune_measure(unsigned int freq_kHz) {
  freq_kHz /= 10; // Convert to 10kHz
  // Force freq to be a multiple of 50kHz:
  if (freq_kHz % 5 != 0)
    freq_kHz -= (freq_kHz % 5);

  _cmd_buff[0] = CMD_TX_TUNE_MEASURE;
  _cmd_buff[1] = 0;
  _cmd_buff[2] = freq_kHz >> 8;
  _cmd_buff[3] = freq_kHz;
  _cmd_buff[4] = 0;
  _send_command(5);

  while (_get_status() != 0x81)
    yield();
}

void Si47xx::begin_rds(unsigned int programID) {
  _set_property(PROP_TX_AUDIO_DEVIATION, 6625); // 66.25KHz (default is 68.25)
  _set_property(PROP_TX_RDS_DEVIATION, 200);    // 2KHz (default)
  _set_property(PROP_TX_RDS_INTERRUPT_SOURCE, 0x0001); // RDS IRQ
  _set_property(PROP_TX_RDS_PI, programID);
  _set_property(PROP_TX_RDS_PS_MIX, 0x03);       // 50% mix (default)
  _set_property(PROP_TX_RDS_PS_MISC, 0x1AC8);    // RDSD0 & RDSMS (default)
  _set_property(PROP_TX_RDS_PS_REPEAT_COUNT, 3); // 3 repeats (default)
  _set_property(PROP_TX_RDS_MESSAGE_COUNT, 1);
  _set_property(PROP_TX_RDS_PS_AF, 0xE0E0); // no AF
  _set_property(PROP_TX_RDS_FIFO_SIZE, 0);
  _set_property(PROP_TX_COMPONENT_ENABLE, 0x0007); // Enable RDS
}

void Si47xx::set_rds_station(const char *s) {
  unsigned int slots = (strlen(s) + 3) / 4;

  for (unsigned int i = 0; i < slots; i++) {
    memset(_cmd_buff, ' ', 6);
    memcpy(_cmd_buff + 2, s, min(4, (int)strlen(s)));
    s += 4;
    _cmd_buff[6] = 0;
    _cmd_buff[0] = CMD_TX_RDS_PS;
    _cmd_buff[1] = i; // slot number
    _send_command(6);
  }
}

void Si47xx::set_rds_buffer(const char *s) {
  unsigned int slots = (strlen(s) + 3) / 4;

  for (unsigned int i = 0; i < slots; i++) {
    memset(_cmd_buff, ' ', 8);
    memcpy(_cmd_buff + 4, s, min(4, (int)strlen(s)));
    s += 4;
    _cmd_buff[8] = 0;
    _cmd_buff[0] = CMD_TX_RDS_BUFF;
    if (i == 0)
      _cmd_buff[1] = 0x06;
    else
      _cmd_buff[1] = 0x04;

    _cmd_buff[2] = 0x20;
    _cmd_buff[3] = i;
    _send_command(8);
  }
  // _set_property(PROP_TX_COMPONENT_ENABLE, 0x0007); // stereo, pilot+rds
}
