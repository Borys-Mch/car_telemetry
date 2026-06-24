#include "modem.h"

TinyGsm modem(SerialAT);
TinyGsmClient gsmClient(modem);
SSLClient sslClient(&gsmClient);
PubSubClient mqttClient(sslClient);

String callStatus = "Idle";
bool callInProgress = false;

bool initModem()
{
  SerialAT.begin(GSM_BAUD, SERIAL_8N1, GSM_RX, GSM_TX);
  delay(3000);

  Serial.println("Запуск...");

  // Рестарт модему з затримками
  modem.restart();
  delay(3000); // Даємо час на ініціалізацію

  // Отримуємо інформацію з таймаутом
  unsigned long start = millis();
  String modemInfo = "";

  while (millis() - start < 5000)
  { // 5 секунд таймаут
    modemInfo = modem.getModemInfo();
    if (modemInfo.length() > 0)
    {
      break;
    }
    delay(500);
    yield(); // Важливо для Watchdog
  }

  if (modemInfo.length() > 0)
  {
    Serial.println("Модем: " + modemInfo);
    return true;
  }

  Serial.println("Помилка: модем не відповідає!");
  return false;
}

bool connectToGPRS()
{
  Serial.print("Підключення до мережі ");
  Serial.print(APN);

  // Спроба підключення з таймаутом
  unsigned long start = millis();
  while (millis() - start < 30000)
  { // 30 секунд
    if (modem.gprsConnect(APN))
    {
      Serial.println(" OK");
      Serial.print("IP: ");
      Serial.println(modem.getLocalIP());
      return true;
    }
    delay(1000);
    yield();
    Serial.print(".");
  }

  Serial.println(" Помилка!");
  return false;
}

int getSignalQuality()
{
  return modem.getSignalQuality();
}

String getIncomingNumber()
{

  if (callInProgress || callStatus == "Calling")
  {
    return "";
  }

  SerialAT.println("AT+CLCC");

  unsigned long t = millis();
  String response = "";
  while (millis() - t < 500)
  {
    while (SerialAT.available())
    {
      char c = SerialAT.read();
      response += c;
    }
    yield(); // годуємо WDT поки чекаємо
  }

  int idx = response.indexOf("+CLCC:");
  if (idx != -1)
  {
    int comma1 = response.indexOf(",", idx);
    int comma2 = response.indexOf(",", comma1 + 1);
    String direction = response.substring(comma1 + 1, comma2);
    direction.trim();

    // Якщо вихідний дзвінок - ігноруємо
    if (direction == "1")
    {
      return "";
    }

    int start = response.indexOf("\"", idx);
    int end = response.indexOf("\"", start + 1);
    if (start != -1 && end != -1)
      return response.substring(start + 1, end);
  }
  return "";
}

bool callBarrier()
{
  if (callInProgress || callStatus == "Calling")
  {
    Serial.println("Дзвінок вже виконується!");
    return false;
  }

  callInProgress = true;
  callStatus = "Calling";
  Serial.println("Дзвінок на шлагбаум: " + String(BARRIER_NUMBER));

  // Здійснюємо дзвінок
  SerialAT.println("ATD" + String(BARRIER_NUMBER) + ";");
  delay(500);

  // Читаємо відповідь
  String response = "";
  unsigned long start = millis();
  while (millis() - start < 3000)
  {
    if (SerialAT.available())
    {
      response += (char)SerialAT.read();
      if (response.indexOf("OK") != -1 || response.indexOf("CONNECT") != -1)
      {
        break;
      }
    }
    yield();
  }

  Serial.println("Відповідь: " + response);

  // Чекаємо 3 секунди для з'єднання
  delay(3000);

  // Завершуємо дзвінок
  SerialAT.println("ATH");
  delay(500);

  // Очищаємо буфер від відповідей
  while (SerialAT.available())
  {
    SerialAT.read();
  }

  callInProgress = false;
  callStatus = "Idle";
  Serial.println("Дзвінок завершено");

  return true;
}

void setCallStatus(const String &status)
{
  callStatus = status;
}

String getCallStatus()
{
  return callStatus;
}