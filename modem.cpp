#include "modem.h"
#include "secrets.h"

#define GSM_RX 18
#define GSM_TX 17
#define GSM_BAUD 115200

HardwareSerial GSM(2);

static void flushGSM()
{
  while (GSM.available())
  {
    Serial.write(GSM.read());
  }
}

bool waitForResponse(const char *ok, const char *err, uint32_t timeout)
{
  String resp = "";
  uint32_t start = millis();

  while (millis() - start < timeout)
  {
    while (GSM.available())
    {
      char c = GSM.read();
      resp += c;
      Serial.write(c);
    }
    if (resp.indexOf(ok) != -1)
      return true;
    if (err && resp.indexOf(err) != -1)
      return false;
    delay(1);
  }
  return false;
}

bool sendATWait(const char *cmd, const char *ok, uint32_t timeout)
{
  flushGSM();
  Serial.print(">> ");
  Serial.println(cmd);
  GSM.println(cmd);
  return waitForResponse(ok, "ERROR", timeout);
}

bool waitForAT(uint32_t totalTimeout)
{
  uint32_t start = millis();
  while (millis() - start < totalTimeout)
  {
    if (sendATWait("AT", "OK", 1000))
      return true;
    delay(1000);
  }
  return false;
}

bool waitForSIM(uint32_t totalTimeout)
{
  uint32_t start = millis();
  while (millis() - start < totalTimeout)
  {
    flushGSM();
    Serial.println(">> AT+CPIN?");
    GSM.println("AT+CPIN?");

    String resp = "";
    uint32_t t = millis();
    while (millis() - t < 1500)
    {
      while (GSM.available())
      {
        char c = GSM.read();
        resp += c;
        Serial.write(c);
      }
    }
    if (resp.indexOf("READY") != -1)
      return true;
    delay(1000);
  }
  return false;
}

bool waitForNetwork(uint32_t totalTimeout)
{
  uint32_t start = millis();
  while (millis() - start < totalTimeout)
  {
    flushGSM();
    Serial.println(">> AT+CREG?");
    GSM.println("AT+CREG?");

    String resp = "";
    uint32_t t = millis();
    while (millis() - t < 1500)
    {
      while (GSM.available())
      {
        char c = GSM.read();
        resp += c;
        Serial.write(c);
      }
    }
    if (resp.indexOf("+CREG: 0,1") != -1 ||
        resp.indexOf("+CREG: 0,5") != -1)
      return true;

    delay(1500);
  }
  return false;
}

bool modemInit()
{
  Serial.begin(GSM_BAUD);
  delay(300);
  Serial.println();
  Serial.println("=== Car Telemetry — MODEM init ===");

  GSM.begin(GSM_BAUD, SERIAL_8N1, GSM_RX, GSM_TX);
  delay(3000);

  if (!waitForAT())
  {
    Serial.println("ERROR: modem does not answer AT");
    return false;
  }
  Serial.println("MODEM OK");

  sendATWait("ATE0");
  sendATWait("AT+CMEE=2");

  if (!waitForSIM())
  {
    Serial.println("ERROR: SIM not ready");
    return false;
  }
  Serial.println("SIM READY");

  if (!waitForNetwork())
  {
    Serial.println("ERROR: network not registered");
    return false;
  }
  Serial.println("NETWORK READY");

  return true;
}