#include <Arduino.h>

#include "config.h"
#include "con.h"
#include "web.h"
#include "devices.h"


void setup()
{
    Serial.begin(DEBUG_SERIAL_SPEED);

    devices_init_before();

    con_init();
    web_init();

    devices_init_after();
}

void loop()
{  
    devices_handle();

    con_handle();
    if(con_state() == CON_STATE_UNDEFINED) {
        con_reconnect_handle();
    }  
}