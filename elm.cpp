#include "elm.h"
#include "mqtt.h"

#include <stdlib.h>

HardwareSerial ELM(1);

bool elmReady = false;

static unsigned long lastElmPoll = 0;
static unsigned long lastElmInitAttempt = 0;
static uint32_t elmBaud = 0;
static const unsigned long ELM_POLL_INTERVAL_MS = 5000;
static const unsigned long ELM_RECONNECT_INTERVAL_MS = 30000;

static void elmFlush()
{
  while (ELM.available())
    ELM.read();
}

static size_t elmCountIdleBytes(uint32_t duration)
{
  size_t count = 0;
  unsigned long start = millis();

  while (millis() - start < duration)
  {
    while (ELM.available())
    {
      ELM.read();
      count++;
    }
  }

  return count;
}

static String elmReadResponse(uint32_t timeout)
{
  String resp;
  unsigned long start = millis();

  while (millis() - start < timeout)
  {
    while (ELM.available())
    {
      char c = ELM.read();
      if (c == '>')
        return resp;
      resp += c;
    }
  }

  return resp;
}

static String printableResponse(const String &resp)
{
  String out;
  const size_t maxLen = 120;

  for (size_t i = 0; i < resp.length() && out.length() < maxLen; i++)
  {
    char c = resp[i];
    if (c >= 32 && c <= 126)
      out += c;
    else if (c == '\r' || c == '\n' || c == '\t')
      out += ' ';
    else
      out += '.';
  }

  if (resp.length() > maxLen)
    out += "...";

  out.trim();
  return out;
}

static String elmCommand(const char *cmd, uint32_t timeout = 1200)
{
  elmFlush();
  Serial.print("[ELM] >> ");
  Serial.println(cmd);
  ELM.print(cmd);
  ELM.print('\r');

  String resp = elmReadResponse(timeout);
  resp.replace("\r", " ");
  resp.replace("\n", " ");
  resp.trim();

  Serial.print("[ELM] << ");
  Serial.println(printableResponse(resp));
  return resp;
}

static void elmBegin(uint32_t baud)
{
  ELM.end();
  delay(100);
  ELM.begin(baud, SERIAL_8N1, ELM_RX, ELM_TX);
  elmBaud = baud;
  delay(300);
  elmFlush();
}

static bool elmWaitForAT(uint32_t totalTimeout)
{
  unsigned long start = millis();

  while (millis() - start < totalTimeout)
  {
    String resp = elmCommand("AT", 1000);
    if (resp.indexOf("OK") != -1 || resp.indexOf("ELM327") != -1)
      return true;

    resp = elmCommand("ATZ", 1500);
    if (resp.indexOf("OK") != -1 || resp.indexOf("ELM327") != -1)
      return true;

    delay(500);
  }

  return false;
}

static bool elmDetectBaud()
{
  const uint32_t baudRates[] = {
      ELM_BAUD,
      38400,
      9600,
      115200,
      57600,
      19200};

  for (size_t i = 0; i < sizeof(baudRates) / sizeof(baudRates[0]); i++)
  {
    bool alreadyTried = false;
    for (size_t j = 0; j < i; j++)
    {
      if (baudRates[j] == baudRates[i])
      {
        alreadyTried = true;
        break;
      }
    }
    if (alreadyTried)
      continue;

    Serial.print("[ELM] Trying baud ");
    Serial.println(baudRates[i]);
    elmBegin(baudRates[i]);

    size_t idleBytes = elmCountIdleBytes(300);
    if (idleBytes > 0)
    {
      Serial.print("[ELM] RX noise before command: ");
      Serial.print(idleBytes);
      Serial.println(" bytes");
    }

    if (elmWaitForAT(2500))
    {
      Serial.print("[ELM] Baud detected: ");
      Serial.println(baudRates[i]);
      return true;
    }
  }

  return false;
}

static bool parsePidBytes(const String &resp, const char *pid, uint8_t *bytes, size_t needed)
{
  String clean = resp;
  clean.toUpperCase();
  clean.replace("SEARCHING...", "");
  clean.replace("STOPPED", "");
  clean.replace("NO DATA", "");
  clean.replace("UNABLE TO CONNECT", "");
  clean.replace("?", "");
  clean.replace(" ", "");

  String marker = String("41") + pid;
  int pos = clean.indexOf(marker);
  if (pos == -1)
    return false;

  pos += marker.length();
  if ((int)clean.length() < pos + (int)(needed * 2))
    return false;

  for (size_t i = 0; i < needed; i++)
  {
    String hexByte = clean.substring(pos + i * 2, pos + i * 2 + 2);
    char *end = nullptr;
    long value = strtol(hexByte.c_str(), &end, 16);
    if (end == hexByte.c_str() || *end != '\0')
      return false;
    bytes[i] = (uint8_t)value;
  }

  return true;
}

static bool readRpm(float &rpm)
{
  uint8_t b[2];
  if (!parsePidBytes(elmCommand("010C"), "0C", b, 2))
    return false;

  rpm = ((b[0] * 256.0f) + b[1]) / 4.0f;
  return true;
}

static bool readSpeed(int &speed)
{
  uint8_t b[1];
  if (!parsePidBytes(elmCommand("010D"), "0D", b, 1))
    return false;

  speed = b[0];
  return true;
}

static bool readCoolant(int &coolant)
{
  uint8_t b[1];
  if (!parsePidBytes(elmCommand("0105"), "05", b, 1))
    return false;

  coolant = (int)b[0] - 40;
  return true;
}

static bool readVoltage(float &voltage)
{
  String resp = elmCommand("ATRV");
  int vPos = resp.indexOf('V');
  if (vPos == -1)
    return false;

  String value = resp.substring(0, vPos);
  value.trim();
  voltage = value.toFloat();
  return voltage > 0.0f;
}

bool elmInit()
{
  elmReady = false;
  lastElmInitAttempt = millis();

  Serial.println("[ELM] Initializing...");

  if (!elmDetectBaud())
  {
    Serial.println("[ELM] No valid AT response on known baud rates");
    return false;
  }

  elmCommand("ATZ", 2000);
  elmCommand("ATE0");
  elmCommand("ATL0");
  elmCommand("ATS0");
  elmCommand("ATH0");
  elmCommand("ATSP0", 2000);

  elmReady = true;
  Serial.print("[ELM] Ready at ");
  Serial.print(elmBaud);
  Serial.println(" baud");
  return true;
}

void elmPoll()
{
  if (!elmReady)
  {
    if (millis() - lastElmInitAttempt > ELM_RECONNECT_INTERVAL_MS)
      elmInit();
    return;
  }

  unsigned long now = millis();
  if (now - lastElmPoll < ELM_POLL_INTERVAL_MS)
    return;
  lastElmPoll = now;

  float rpm = 0.0f;
  int speed = 0;
  int coolant = 0;
  float voltage = 0.0f;

  bool hasRpm = readRpm(rpm);
  bool hasSpeed = readSpeed(speed);
  bool hasCoolant = readCoolant(coolant);
  bool hasVoltage = readVoltage(voltage);

  mqttSendObd(rpm, hasRpm, speed, hasSpeed, coolant, hasCoolant, voltage, hasVoltage);
}
