/*
Silicon Devices Si4713 FM Transmitter Driver Library. Based on the
Adafruit Si4713 library (https://github.com/adafruit/Adafruit-Si4713-Library/),
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

#ifndef __SI4713_H
#define __SI4713_H

#define SI47XX_CHIP_VERSION 128
#define SI47XX_I2C_ADDR 0x63

#define SI47XX_PIN_RESET 9
#define SI47XX_BUF_SIZE 10

#define SI47XX_MAX_AWAIT 3000 // 3 sec
#define SI47XX_STEP_AWAIT 100 


#define min(a, b) ((a) < (b) ? (a) : (b))

class Si47xx {
  public:
    bool begin();
    void tune_fm(unsigned int freqKHz);
    void read_tune_status(void);
    void read_tune_measure(unsigned int freq);
    void set_tx_power(unsigned int pwr, unsigned int antcap = 0);
    void read_asq_status(void);

    void begin_rds(unsigned int programID = 0xADAF);
    void set_rds_station(const char *s);
    void set_rds_buffer(const char *s);

    unsigned int CurrFreq;
    unsigned int CurrdBuV;
    unsigned int CurrAntCap;
    unsigned int CurrNoiseLevel;
    unsigned int CurrASQ;
    int CurrInLevel;

    void set_gpio(unsigned int x);
    void set_gpio_ctl(unsigned int x);

  private:
    unsigned int _cmd_buff[SI47XX_BUF_SIZE]; // holds the command buffer

    void _send_command(unsigned int len, bool need_status = true);
    void _set_property(unsigned int p, unsigned int v);
    unsigned int _get_status(void);
};

#endif // __SI4713_H
