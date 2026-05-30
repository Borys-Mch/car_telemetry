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

// буфер для парсингу
String line = "";

// таймер
unsigned long lastSend = 0;

void sendAT(const char *cmd)
{
  GSM.println(cmd);
}

void mqttConnect()
{
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

void mqttPublish(int rssi)
{
  int dbm = -113 + (rssi * 2);

  String payload = "{\"rssi\":" + String(rssi) + ",\"dbm\":" + String(dbm) + "}";
  String topic = "home/car/telemetry";

  GSM.printf("AT+CMQTTTOPIC=0,%d\r\n", topic.length());
  delay(100);
  GSM.print(topic);
  delay(100);

  GSM.printf("AT+CMQTTPAYLOAD=0,%d\r\n", payload.length());
  delay(100);
  GSM.print(payload);
  delay(100);

  sendAT("AT+CMQTTPUB=0,1,60");
}

void requestSignal()
{
  sendAT("AT+CSQ");
}

void sendDiscovery()
{
  String topic = "homeassistant/sensor/car_signal/config";

  String payload = R"({
    "name": "Modem Signal",
    "state_topic": "home/car/telemetry",
    "unit_of_measurement": "dBm",
    "value_template": "{{ value_json.dbm }}",
    "unique_id": "modem_signal",
    "device": {
      "identifiers": ["car_info"],
      "name": "Car Info",
      "model": "ESP32 + A7670",
      "manufacturer": "DIY"
    }
  })";

  GSM.printf("AT+CMQTTTOPIC=0,%d\r\n", topic.length());
  delay(100);
  GSM.print(topic);
  delay(100);

  GSM.printf("AT+CMQTTPAYLOAD=0,%d\r\n", payload.length());
  delay(100);
  GSM.print(payload);
  delay(100);

  GSM.println("AT+CMQTTPUB=0,1,60");
}

void sendButtonDiscovery()
{
  String topic = "homeassistant/button/car_gate/config";

  String payload = R"({
    "name": "Gate Open",
    "command_topic": "home/car/cmd",
    "payload_press": "gate",
    "unique_id": "car_gate_btn",
    "device": {
      "identifiers": ["car_info"],
      "name": "Car Info",
      "model": "ESP32 + A7670",
      "manufacturer": "DIY"
    }
  })";

  GSM.printf("AT+CMQTTTOPIC=0,%d\r\n", topic.length());
  delay(100);
  GSM.print(topic);
  delay(100);

  GSM.printf("AT+CMQTTPAYLOAD=0,%d\r\n", payload.length());
  delay(100);
  GSM.print(payload);
  delay(100);

  GSM.println("AT+CMQTTPUB=0,1,60,1"); // retain
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
  String topic = "homeassistant/sensor/car_call_status/config";

  String payload = R"({
    "name": "Car Call Status",
    "state_topic": "home/car/status",
    "value_template": "{{ value_json.call }}",
    "unique_id": "car_call_status",
    "device": {
      "identifiers": ["car_info"],
      "name": "Car Info",
    }
  })";

  GSM.printf("AT+CMQTTTOPIC=0,%d\r\n", topic.length());
  delay(100);
  GSM.print(topic);
  delay(100);

  GSM.printf("AT+CMQTTPAYLOAD=0,%d\r\n", payload.length());
  delay(100);
  GSM.print(payload);
  delay(100);

  GSM.println("AT+CMQTTPUB=0,1,60,1");
}

void sendCallStatus(String status)
{
  String topic = "home/car/status";
  String payload = "{\"call\":\"" + status + "\"}";

  GSM.printf("AT+CMQTTTOPIC=0,%d\r\n", topic.length());
  delay(50);
  GSM.print(topic);
  delay(50);

  GSM.printf("AT+CMQTTPAYLOAD=0,%d\r\n", payload.length());
  delay(50);
  GSM.print(payload);
  delay(50);

  GSM.println("AT+CMQTTPUB=0,1,60");
}

void setup()
{
  Serial.begin(115200);
  GSM.begin(GSM_BAUD, SERIAL_8N1, GSM_RX, GSM_TX);

  delay(3000);

  sendAT("AT");
  delay(1000);

  mqttConnect();
  delay(2000);

  sendDiscovery();           // сигнал
  sendCallStatusDiscovery(); // статус дзвінка
  sendButtonDiscovery();     // кнопка
  delay(1000);

  mqttSubscribe(); // підписка на команди
}

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
          mqttPublish(rssi);
        }
      }

      // статус дзвінка
      if (line.indexOf("NO CARRIER") != -1)
      {
        sendCallStatus("idle");
      }

      if (line.indexOf("BUSY") != -1)
      {
        sendCallStatus("busy");
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