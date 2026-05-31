/*
 * Car Telemetry - MQTT + FreeRTOS tasks
 * ESP32-S3 + A7670E (UART1 GPIO18/17) + ELM327 (UART0 GPIO44/43)
 *
 * Бібліотеки: ArduinoJson
 * Arduino IDE → Tools → USB CDC On Boot: Enabled
 */

#include <ArduinoJson.h>
#include "esp_task_wdt.h"
#include "secrets.h"

// ===== ПІНИ =================================================
#define GSM_RX 18
#define GSM_TX 17
#define GSM_BAUD 115200
#define PWK_PIN 4
#define BTN_PIN 5

// ===== UART =================================================
HardwareSerial GSM(2);

// ===== БУФЕР ДЛЯ ПАРСИНГУ ===================================
String line = "";

// ===== ТАЙМЕР ===============================================
unsigned long lastSend = 0;
unsigned long lastGPS = 0;
unsigned long lastMQTT = 0;
unsigned long lastMQTTsend = 0;

bool mqttBusy = false;

void sendAT(const char *cmd)
{
  GSM.println(cmd);
}

// ===== MQTT =================================================

void mqttConnect()
{
  GSM.println("AT+CMQTTDISC=0,120");
  delay(500);

  GSM.println("AT+CMQTTREL=0");
  delay(500);

  GSM.println("AT+CMQTTSTOP");
  delay(1000);

  sendAT("AT+CMQTTSTART");
  delay(1000);

  sendAT("AT+CSSLCFG=\"sslversion\",0,4");
  delay(200);

  sendAT("AT+CSSLCFG=\"authmode\",0,0"); // без перевірки
  delay(200);

  GSM.printf("AT+CMQTTACCQ=0,\"%s\",1\r\n", MQTT_CLIENT_ID);
  delay(500);

  sendAT("AT+CMQTTSSLCFG=0,0");
  delay(200);

  GSM.printf(
      "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,1,\"%s\",\"%s\"\r\n",
      MQTT_HOST,
      MQTT_PORT,
      MQTT_USER,
      MQTT_PASS);

  delay(2000);
}

void mqttSend(String topic, String payload, bool retain = false)
{
  if (millis() - lastMQTTsend < 1500)
  {
    delay(1500 - (millis() - lastMQTTsend));
  }

  lastMQTTsend = millis();

  GSM.printf("AT+CMQTTTOPIC=0,%d\r\n", topic.length());
  delay(300);

  GSM.print(topic);
  delay(300);

  GSM.printf("AT+CMQTTPAYLOAD=0,%d\r\n", payload.length());
  delay(300);

  GSM.print(payload);
  delay(300);

  if (retain)
    GSM.println("AT+CMQTTPUB=0,1,60,1");
  else
    GSM.println("AT+CMQTTPUB=0,1,60");

  delay(500);
}

// ===== MOBILE SIGNAL ========================================

void sendSignal(int rssi)
{
  int dbm = -113 + (rssi * 2);

  String payload = "{\"rssi\":" + String(rssi) + ",\"dbm\":" + String(dbm) + "}";
  mqttSend("home/car/telemetry", payload, false);
}

void requestSignal()
{
  sendAT("AT+CSQ");
}

void sendSignalDiscovery()
{
  String payload = R"({
    "name": "Modem Signal",
    "state_topic": "home/car/telemetry",
    "unit_of_measurement": "dBm",
    "value_template": "{{ value_json.dbm }}",
    "unique_id": "modem_signal",
    "device": {
      "identifiers": ["car_info"],
      "name": "Car Info",
      "model": "ESP32-S3 + A7670E + ELM327",
      "manufacturer": "DEREN"
    }
  })";

  mqttSend("homeassistant/sensor/car_signal/config", payload, false);
}

// ===== ШЛАГБАУМ =============================================

void sendButtonDiscovery()
{
  String payload = R"({
    "name": "Gate Open",
    "command_topic": "home/car/cmd",
    "payload_press": "gate",
    "unique_id": "car_gate_btn",
    "device": {
      "identifiers": ["car_info"],
      "name": "Car Info"
    }
  })";

  mqttSend("homeassistant/button/car_gate/config", payload, true);
}

void mqttSubscribe()
{
  String topic = "home/car/cmd";

  GSM.printf("AT+CMQTTSUB=0,%d,1\r\n", topic.length());
  delay(100);
  GSM.print(topic);
}

void sendCallStatusDiscovery()
{
  String payload = R"({
    "name": "Car Call Status",
    "state_topic": "home/car/status",
    "value_template": "{{ value_json.call }}",
    "unique_id": "car_call_status",
    "icon": "mdi:phone",
    "device": {
      "identifiers": ["car_info"],
      "name": "Car Info"
    }
  })";

  mqttSend("homeassistant/sensor/car_call_status/config", payload, true);
}

void sendCallStatus(String status)
{
  String payload = "{\"call\":\"" + status + "\"}";

  mqttSend("home/car/status", payload, true);
}

// ===== ВХІДНІ ДЗВІНКИ =======================================

void sendIncomingDiscovery()
{
  String payload = R"({
    "name": "Incoming Call",
    "state_topic": "home/car/incoming",
    "value_template": "{{ value_json.number }}",
    "json_attributes_topic": "home/car/incoming",
    "unique_id": "car_incoming_call",
    "icon": "mdi:phone-incoming",
    "device": {
      "identifiers": ["car_info"],
      "name": "Car Info"
    }
  })";

  mqttSend("homeassistant/sensor/car_incoming_call/config", payload, true);
}

void sendIncomingCall(String number)
{
  String payload = "{";
  payload += "\"number\":\"" + number + "\",";
  payload += "\"state\":\"ringing\"";
  payload += "}";

  mqttSend("home/car/incoming", payload, true);
}

void clearIncomingCall()
{
  String payload = "{";
  payload += "\"number\":\"\",";
  payload += "\"state\":\"idle\"";
  payload += "}";

  mqttSend("home/car/incoming", payload, true);
}

// ===== ЗАВЕРШЕННЯ ДЗВІНКА ================================

void sendEndCallDiscovery()
{
  String payload = R"({
    "name": "End Call",
    "command_topic": "home/car/cmd",
    "payload_press": "hangup",
    "unique_id": "car_call_end_btn",
    "icon": "mdi:phone-hangup",
    "device": {
      "identifiers": ["car_info"],
      "name": "Car Info"
    }
  })";

  mqttSend("homeassistant/button/car_call_end/config", payload, true);
}

// ===== GPS ==================================================

void sendGPS(String lat, String lon, String sats)
{
  if (sats.length() == 0)
    sats = "0";

  String payload = "{";
  payload += "\"latitude\":" + lat + ",";
  payload += "\"longitude\":" + lon + ",";
  payload += "\"sat\":" + sats;
  payload += "}";

  mqttSend("home/car/gps", payload, true);
}

void sendGPSDiscovery()
{
  String payload = R"({
    "name": "Car GPS",
    "state_topic": "home/car/gps",
    "json_attributes_topic": "home/car/gps",
    "unique_id": "car_gps_tracker",
    "source_type": "gps",
    "device": {
      "identifiers": ["car_info"],
      "name": "Car Info"
    }
  })";

  mqttSend("homeassistant/device_tracker/car_gps/config", payload, true);
}

// ===== SETUP ================================================

void setup()
{
  Serial.begin(115200);
  GSM.begin(GSM_BAUD, SERIAL_8N1, GSM_RX, GSM_TX);
  GSM.println("AT+CLIP=1");
  delay(3000);

  GSM.println("AT+CGNSSPWR=1");
  delay(1000);
  GSM.println("AT+CGNSSINFO=0");
  delay(500);

  sendAT("AT");
  delay(1000);

  mqttConnect();
  delay(2000);

  sendSignalDiscovery(); // сигнал
  delay(500);
  sendCallStatusDiscovery(); // статус дзвінка
  delay(500);
  sendButtonDiscovery(); // кнопка
  delay(500);
  sendIncomingDiscovery(); // номер вхідного дзвінка
  delay(500);
  sendEndCallDiscovery(); // завершення дзвінка
  delay(500);
  sendGPSDiscovery(); // GPS
  delay(500);

  mqttSubscribe(); // підписка на команди
}

// ===== LOOP (основний таск — OBD + кнопка + виконання команд)

void loop()
{
  while (GSM.available())
  {
    char c = GSM.read();
    Serial.write(c);

    if (c == '\n')
    {
      line.trim();

      // сигнал
      if (line.startsWith("+CSQ:"))
      {
        int comma = line.indexOf(',');
        int rssi = line.substring(6, comma).toInt();

        if (rssi != 99)
        {
          sendSignal(rssi);
        }
      }

      if (line.indexOf("BUSY") != -1)
      {
        sendCallStatus("busy");
      }

      // вхідний дзвінок
      if (line.startsWith("+CLIP:"))
      {
        int firstQuote = line.indexOf('"');
        int secondQuote = line.indexOf('"', firstQuote + 1);

        String number = line.substring(firstQuote + 1, secondQuote);

        Serial.println("Incoming: " + number);

        sendIncomingCall(number);
      }

      // завершення дзвінка
      if (line.indexOf("hangup") != -1)
      {
        Serial.println("END CALL");

        GSM.println("AT+CHUP");
      }

      // статус дзвінка
      if (line.indexOf("NO CARRIER") != -1)
      {
        sendCallStatus("idle");
        clearIncomingCall();
      }

      // дані GPS
      if (line.startsWith("+CGNSSINFO:"))
      {
        // розбиваємо
        int idx[20];
        int count = 0;

        for (int i = 0; i < line.length(); i++)
        {
          if (line[i] == ',')
          {
            idx[count++] = i;
          }
        }

        String lat = line.substring(idx[4] + 1, idx[5]);
        String latDir = line.substring(idx[5] + 1, idx[6]);

        String lon = line.substring(idx[6] + 1, idx[7]);
        String lonDir = line.substring(idx[7] + 1, idx[8]);

        String sats = line.substring(idx[count - 1] + 1);

        // напрямки
        if (latDir == "S")
          lat = "-" + lat;
        if (lonDir == "W")
          lon = "-" + lon;

        Serial.println("GPS: " + lat + ", " + lon);

        sendGPS(lat, lon, sats);
      }

      if (millis() - lastGPS > 10000)
      {
        lastGPS = millis();
        GSM.println("AT+CGNSSINFO");
      }

      // команда з HA
      if (line.indexOf("gate") != -1)
      {
        Serial.println("OPEN GATE");

        sendCallStatus("calling");
        GSM.printf("ATD%s;\r\n", BARRIER_NUMBER);
      }

      line = "";
    }
    else
    {
      line += c;
    }
  }

  // кожні 10 сек
  if (millis() - lastSend > 10000)
  {
    lastSend = millis();
    GSM.println("AT+CSQ");
  }
}
