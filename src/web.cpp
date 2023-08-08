#include <Arduino.h>
#include <Update.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <AsyncJson.h>
#include <ArduinoJson.h>

#include "config.h"
#include "con.h"
#include "web.h"
#include "devices.h"

#define ERROR_EXPLAIN(Explain) request->send(200, "application/json", "{ \"result\": \"error\", \"explain\": \"" Explain "\" }")


AsyncWebServer server(80);  

void not_found(AsyncWebServerRequest *request) {
    String method = request->methodToString();
    String url = request->url();
    unsigned int body_size = request->contentLength();

    ESP_LOGD(TAG, "Unknown request - %s %s (%d)", method.c_str(), url.c_str(), body_size);
    request->send(404, "application/json", "{ \"result\": \"error\", \"explain\": \"method_not_found\" }");
}

void web_init()
{
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{ \"result\": \"ok\", \"state\": \"online\" }");
    });

    server.on("/wifi_list", HTTP_GET, [](AsyncWebServerRequest *request) {
        String reply = get_availible_nets_json();    
        request->send(200,  "application/json", reply);
    });

    server.on("/wifi_reset", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{ \"result\": \"ok\" }");
        
        yield();
        delay(DELAY_AFTER_FILE_OP_MS);
        yield();

        con_reset(); 
    });

    server.addHandler(new AsyncCallbackJsonWebHandler("/wifi_config", [](AsyncWebServerRequest *request, JsonVariant &income) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        DynamicJsonDocument reply(JSON_MAX_SIZE);
        bool need_reboot = false;

        if(income.containsKey("ssid") && income.containsKey("key") 
            && income["ssid"].as<String>().length() > 0)
        {
            if(save_config(income["ssid"], income["key"])) {
                reply["result"] = "ok";
                reply["hostname"] = get_mdns_name();
                need_reboot = true;
            } else {
                reply["result"] = "error";
                reply["explain"] = "config_write_error";
            }
        } else {
            reply["result"] = "error";
            reply["explain"] = "params_error";
        }
        serializeJson(reply, *response);
        
        if(need_reboot) {
            response->addHeader("Connection", "close");
            request->send(response);
            do_restart();
        } else {
            request->send(response);
        }
    }));


    /*
        Wireless Firmware update
    */

    server.on("/ota", HTTP_POST, [&](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(
                200, 
                "application/json", 
                (Update.hasError()) ? "{ \"result\": \"fail\" }" : "{ \"result\": \"done\" }"
            );
        response->addHeader("Connection", "close");
        request->send(response);
       
        if(!Update.hasError()) 
            do_restart();
    }, [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        int cmd = (filename == "filesystem") ? U_SPIFFS : U_FLASH;

        if(!index) {
            if(!request->hasParam("MD5", true)) {
                return ERROR_EXPLAIN("ota_md5_parameter_missing");
            }

            if(!Update.setMD5(request->getParam("MD5", true)->value().c_str())) {
                return ERROR_EXPLAIN("ota_md5_parameter_invalid");
            }

            if(!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) { 
                return ERROR_EXPLAIN("ota_could_not_begin");
            }
        }

        if(len) {
            if(Update.write(data, len) != len) {
                return ERROR_EXPLAIN("ota_could_not_process");
            }
        }
            
        if(final) {
            if(!Update.end(true)) {
                return ERROR_EXPLAIN("ota_could_not_finished");
            }
        } 
    });

    devices_web_init(&server);

    server.onNotFound(not_found);
    server.begin();
}
