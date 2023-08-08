#ifndef __CON_H
#define __CON_H

#include <string.h>

#define WIFI_RECONNECT_INTERVAL_MS 60000 
#define DELAY_AFTER_FILE_OP_MS 3000

#define WIFI_STEP_DELAY 500
#define WIFI_MAX_COUNT 300

#define CONFIG_FILE "/config.txt"

#define CON_STATE_UNDEFINED 0
#define CON_STATE_AP 1
#define CON_STATE_CLIENT 2

typedef unsigned int con_state_t;

typedef struct 
{
    con_state_t state = CON_STATE_UNDEFINED;

    String host_id = "";
    String ssid = "";
    String key = "";

} wifi_state;

/*
    iface for main
*/

void con_init();

con_state_t con_state();

void con_reconnect_handle();
void con_handle();

/*
    iface for web
*/

String get_availible_nets_json();
String get_mdns_name();

bool save_config(String, String);
void do_restart();

/*
    iface for device
*/

void con_reset();

#endif