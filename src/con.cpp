#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

#include <time.h>

#include "config.h"
#include "con.h"
#include "devices.h"

wifi_state g_con;

static void con_ap_init() 
{
    g_con.state = CON_STATE_AP;
    
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP(g_con.host_id.c_str(), DEVICE_WIFI_KEY);

    set_led_state(LED_STATE_AP);
}

static void con_mdns_init()
{
    if(MDNS.begin(g_con.host_id.c_str()) != ESP_OK) {
        int s_num = MDNS.addService("http", "tcp", 80);
        if(s_num == 0) {
            ESP_LOGW(TAG, "Can't add mDNS service to responder");
        }      
    } else {
        ESP_LOGW(TAG, "Error setting up MDNS responder");
    }
}

static void con_sta_init()
{
    unsigned int cnt = 0;

    g_con.state = CON_STATE_CLIENT;
  
    ESP_LOGI(TAG, "Connecting to WiFi %s (%s)", g_con.ssid.c_str(), g_con.key.c_str());
  
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(g_con.host_id.c_str());
    WiFi.begin(g_con.ssid.c_str(), g_con.key.c_str());
  
    while(WiFi.status() != WL_CONNECTED && cnt < WIFI_MAX_COUNT) {
        delay(WIFI_STEP_DELAY);
        cnt++;
    }
  
    if(WiFi.status() != WL_CONNECTED) {
        ESP_LOGI(TAG, "Can't connect to %s - timeout. Starting AP", g_con.ssid.c_str());
        con_ap_init();
    } else {
        ESP_LOGI(TAG, "Connected to %s - %s [%d]", g_con.ssid.c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
        set_led_state(LED_STATE_STA);
    }
}


static bool con_config_init() 
{
    if(LOCALFS.exists(CONFIG_FILE)) {
        File f = LOCALFS.open(CONFIG_FILE, "r", false);

        if(!f) {
            ESP_LOGW(TAG, "Can't open config file");  
            return false;      
        }
        
        if(f.available()) {
            g_con.ssid = f.readStringUntil('\n');
            g_con.ssid.replace("\n", "");
            g_con.ssid.trim();

            ESP_LOGI(TAG, "Read SSID from config - %s", g_con.ssid.c_str());    
        } else {
            ESP_LOGW(TAG, "Can't read SSID from config - not available");    
            f.close();
            return false;      
        }

        if(f.available()) {
            g_con.key = f.readStringUntil('\n');
            g_con.key.replace("\n", "");
            g_con.key.trim();

            ESP_LOGI(TAG, "Read key from config - %s", g_con.key.c_str());    
        } else {
            ESP_LOGW(TAG, "Can't read key from config - not available");    
            f.close();
            return false;      
        }
        f.close();

        return (g_con.ssid.length() > 0);
    } else {
        ESP_LOGI(TAG, "Config file not found");
        return false;
    }
}

void do_restart()
{
    yield();
    delay(DELAY_AFTER_FILE_OP_MS);
    yield();
    set_led_state(LED_STATE_NONE);
    ESP.restart();
}

void con_reset()
{
    g_con.state = CON_STATE_UNDEFINED;
    WiFi.disconnect();

    LOCALFS.remove(CONFIG_FILE);    
    set_led_state(LED_STATE_NONE);
    do_restart();
}

/*
    main
*/

void con_init() 
{
    set_led_state(LED_STATE_INIT);

    String mac_addr = String(WiFi.macAddress());
    mac_addr.replace(":", "");
    g_con.host_id = DEVICE_PREFIX + mac_addr;

    #ifdef CONFIG_RESET_PIN
        pinMode(CONFIG_RESET_PIN, INPUT);
    #endif

    if(!LOCALFS.begin(FORMAT_FS_IF_FAILED)) {
        ESP_LOGE(TAG, "Can't access to FS");
    }

    if(con_config_init()) {
        con_sta_init();
    } else {
        con_ap_init();
    };
    con_mdns_init();
}

con_state_t con_state()
{
    return (g_con.state == CON_STATE_CLIENT && WiFi.status() != WL_CONNECTED) 
        ? CON_STATE_UNDEFINED
        : g_con.state;
}

void con_reconnect_handle()
{
    static unsigned long prev_ms = 0;
    unsigned long current_ms = millis();
  
    if((current_ms - prev_ms) >= WIFI_RECONNECT_INTERVAL_MS ) {
        ESP_LOGW(TAG, "Reconnecting to WiFi");
    
        set_led_state(LED_STATE_NONE);    
        WiFi.disconnect();   
        WiFi.reconnect();
        set_led_state(LED_STATE_STA);    
    
        prev_ms = current_ms;
    }
}

void con_handle()
{
    #ifdef CONFIG_RESET_PIN
        if(digitalRead(CONFIG_RESET_PIN) == HIGH) {
            con_reset();
        }
    #endif 
}

/*
    web
*/

String get_mdns_name() 
{
    return g_con.host_id + ".local";
}

String get_availible_nets_json()
{
    String json = "";
    int n = WiFi.scanComplete();

    if(n == -2) {
        WiFi.scanNetworks(true);
        json = "{ \"result\": \"await\", \"explain\": \"start_scan\" }";
    } else if(n == -1) {
        json = "{ \"result\": \"await\", \"explain\": \"still_scan\" }";
    } else if(n) {
        json = "{ \"result\": \"ok\", \"list\": [";   
        for(unsigned int i = 0; i < n; ++i) {
            if(i) json += ", ";
      
            json += "{";
            json += "\"rssi\":" + String(WiFi.RSSI(i));
            json += ", \"ssid\":\"" + WiFi.SSID(i) + "\"";
            json += ", \"bssid\":\"" + WiFi.BSSIDstr(i) + "\"";
            json += ", \"channel\":" + String(WiFi.channel(i));
            json += ", \"secure\":" + String(WiFi.encryptionType(i));
            json += "}";
        }
        WiFi.scanDelete();
        json += "], ";

        if(WiFi.scanComplete() == -1) {
            json += "\"scan_state\": \"working\"";   
        } else {
            json += "\"scan_state\": \"done\"";   
        }
        json += "}";
    }

    return json;
}

bool save_config(String ssid, String key)
{
    File f = LOCALFS.open(CONFIG_FILE, "w", true);

    if(!f) {
        ESP_LOGW(TAG, "Can't write to config - file not open");    
        return false;
    }
    
    f.println(ssid);
    f.println(key);
    f.close();

    return true;
}
