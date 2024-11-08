/*
Термостат
Управляет температурой включая и выключая реле (котёл) в зависимовти от температуры в комнате.
*/

#define PIN_RELAY 0 // пин GPIO0/D3
#define PIN_DS 2   // пин GPIO2/D4
#define TIME_TO_AP 300000 // держать запущенную точку доступа после включения (в микросекундах)
#define USE_GYVER_HTTP // использовать врапер GyverHTTP поверх ESP8266WiFi (более стабильное первое подключение)
// #define DEBUG // включить отладку, надо освободить UART !!!!

#include "timerMini.hpp"
#include <GyverDBFile.h>
#include <LittleFS.h>
#ifdef USE_GYVER_HTTP
#include <SettingsGyver.h>
#else
#include <SettingsESP.h>
#endif
#include <GyverDS18.h>
#include <GyverRelay.h>
#include "defaults.h"
#ifdef ESP32
#include <ESPmDNS.h>
#else // ESP8266
#include <ESP8266mDNS.h>
#endif

GyverDS18Single ds(PIN_DS);
GyverRelay regulator(REVERSE);
// NORMAL - включаем нагрузку при переходе через значение снизу (пример: охлаждение)
// REVERSE - включаем нагрузку при переходе через значение сверху (пример: нагрев)

timerMini timerTemperature(2500); // опрашивать температуру каждые 2.5 секунд (как обновление web)
timerMini timerRegulator(60000); // действие каждую минуту

GyverDBFile db(&LittleFS, "/data.db");
#ifdef USE_GYVER_HTTP
SettingsGyver sett("Thermostat Relay", &db);
#else
SettingsESP sett("Thermostat Relay", &db);
#endif

float cur_temp = 0.0f; // температура во время последнего опроса
bool relay = OFF; // реальное состояние реле
bool cur_relay = OFF; // состояние, которое должно быть
bool fl_force = false; // флаг принудительного изменения состояния реле
uint8_t web_view = 0; // в интерфейсе есть обычная страничка и страничка сканироваия WiFi
int found_wifi = 0; // флаг - сканирование WiFi завершено (асинхронный режим)
bool need_reload = false; // надо обновить настройки
bool wifi_connected = false; // флаг того, что подключение к WiFi успешно
bool fl_mdns = false; // флаг того, что mDNS запущен
unsigned long temp_switch = 0; // время до которого работает временное принудительное переключение реле
bool fl_on = true; // флаг того, что реле вообще должно работать

/* Подключение в WiFi
  Всегда происходит в асинхронном режиме и нет никакого таймаута, когда появится точка доступа
  тогда и подключится. В примерах есть осчёт и таймаут, это костыль, который должен наглядно
  показать процесс подключения и вывести в консоль IP.
*/
void WifiConnect() {
  if (strlen(db[kk::wifi_ssid].str())) {
    LOG(printf_P, PSTR("ssid: %s, pass: %s\n"), db[kk::wifi_ssid].str(), db[kk::wifi_pass].str());
    WiFi.begin(db[kk::wifi_ssid].toString(), db[kk::wifi_pass].toString());
  }
}

// Перерисовать страничку подключения, чтобы отобразились найденные сети
void prinScanResult(int networksFound) {
  found_wifi = networksFound;
  sett.reload();
}

// Загрузить новые параметры в библиотеку GyverRelay
void regulatorReload() {
  regulator.setpoint = db[kk::temp].toFloat();
  regulator.hysteresis = db[kk::hyst].toFloat();
  regulator.k = db[kk::gain].toFloat();
  fl_on = db[kk::on].toBool();
  need_reload = false;
}

// Проверка заполненого в настройках, защита от дурака
void checkFloat(int id, float min, float max) {
  if( db[id].toFloat() > max ) db[id] = max;
  if( db[id].toFloat() < min ) db[id] = min;
  need_reload = true;
}
void checkInt(int id, int min, int max) {
  if( db[id].toInt() > max ) db[id] = max;
  if( db[id].toInt() < min ) db[id] = min;
  // need_reload = true;
}

// Конструктор веб-странички
void build(sets::Builder& b) {
  switch (web_view) {
    case 0: // главный вид
      b.Label("lbl_t"_h, "Температура (°C)");
      b.Label("lbl_cr"_h, "Состояние");
      // b.Label("lbl2"_h, "millis()", "", sets::Colors::Red);

      { sets::Group g(b, "Настройки");
        b.Input(kk::hostname, "Название");
        if( b.Input(kk::temp, "Нужная температура") ) checkFloat(kk::temp, 0.0f, 40.0f);
        if( b.Input(kk::hyst, "Гистерезис") ) checkFloat(kk::hyst, 0.1f, 20.0f);
        if( b.Input(kk::gain, "K-усиления") ) checkFloat(kk::gain, 0.0f, 1000.0f);
        if( b.Input(kk::period, "Задержка (сек.)") ) {
          checkInt(kk::period, 5, 1440);
          timerRegulator.setInterval(db[kk::period].toInt() * 1000);
        }
        if( b.Input(kk::tcor, "Коррекция (°C)") ) checkFloat(kk::tcor, -30.0f, 30.0f);
        if (b.beginMenu("WiFi")) {
          { sets::Group g(b, "WiFi");
            if (b.Button("scan"_h, "Поиск сети", sets::Colors::Yellow)) {
              web_view = 1;
              WiFi.scanNetworksAsync(prinScanResult);
            }

            b.Input(kk::wifi_ssid, "SSID");
            b.Pass(kk::wifi_pass, "Password", "***");
            b.LED("lbl_wifi"_h, "Подключено", &wifi_connected);
            b.Label("lbl_ip"_h, "IP");
            b.Label("lbl_rssi"_h, "RSSI");

            if (b.beginButtons()) {
              // if (b.Button("reboot"_h, "Reboot", sets::Colors::Red)) {
              //   db.update();
              //   #ifdef DEBUG
              //   Serial.flush();
              //   #endif
              //   #ifdef ESP32
              //   WiFi.getSleep();
              //   #else // ESP8266
              //   WiFi.forceSleepBegin(); //disable AP & station by calling "WiFi.mode(WIFI_OFF)" & put modem to sleep
              //   #endif
              //   LittleFS.end();
              //   delay(1000);
              //   ESP.restart();
              // }
              if (b.Button("connect"_h, "Connect")) {
                db.update();
                b.reload();
                WifiConnect();
              }
              // этот пункт вроде не нужен, но добавлен для однообразия со страничкой сканирования
              if (b.Button("back"_h, "Назад", sets::Colors::Blue)) {
                b.reload();
              }
              b.endButtons();  // завершить кнопки
            }
          }
          b.endMenu();  // не забываем завершить меню
        }
        if (b.Switch(kk::on, "Включено")) {
          fl_on = db[kk::on].toBool();
          if(!fl_on) {
            digitalWrite(PIN_RELAY, OFF);
            relay = OFF;
          }
          LOG(println, fl_on ? "On": "Off");
        }
        if (b.Switch("sw_test"_h, "Переключить на минуту", &fl_force)) {
          digitalWrite(PIN_RELAY, ! cur_relay);
          relay = ! cur_relay;
          temp_switch = fl_force ? millis(): 0;
        }
      }
      break;
    case 1: // режим сканирования
      { sets::Group g(b, "WiFi");
        if (b.Button("scan"_h, "Повторить поиск", sets::Colors::Orange)) {
          web_view = 1;
          WiFi.scanNetworksAsync(prinScanResult);
        }

        char ssid[50];
        for (int i=0; i<found_wifi; i++) {
          snprintf_P(ssid, sizeof(ssid), PSTR("%d: %s, Ch:%d (%ddBm) %s\n"),
            i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i),
            WiFi.encryptionType(i) == ENC_TYPE_NONE ? "open" : ""
          );
          if (b.Button(ssid, sets::Colors::Mint))
            db[kk::wifi_ssid] = WiFi.SSID(i).c_str();
        }

        b.Input(kk::wifi_ssid, "SSID");
        b.Pass(kk::wifi_pass, "Password", "***");

        if (b.beginButtons()) {
          if (b.Button("connect"_h, "Connect")) {
            db.update();
            web_view = 0;
            b.reload();
            WifiConnect();
            WiFi.scanDelete();
          }
          // а вот здесь кнопка обязательно, иначе из режима сканирования не выйти
          if (b.Button("connect"_h, "Назад", sets::Colors::Blue)) {
            web_view = 0;
            b.reload();
            WiFi.scanDelete();
          }
          b.endButtons();  // завершить кнопки
        }
      }
      break;
  }
}

// это апдейтер. Функция вызывается, когда вебморда запрашивает обновления
void update(sets::Updater& u) {
    // можно отправить значение по имени (хэшу) виджета
    u.update("lbl_t"_h, cur_temp);
    u.update("lbl_cr"_h, relay == ON ? "On": "Off");
    // u.update("lbl2"_h, millis());
    u.update("lbl_ip"_h, wifi_connected ? WiFi.localIP().toString().c_str(): "");
    u.update("lbl_rssi"_h, wifi_connected ? WiFi.RSSI(): 0);
    u.update("sw_test"_h, fl_force);

    // примечание: при ручных изменениях в базе данных отправлять новые значения не нужно!
    // библиотека сделает это сама =)
}

void setup() {
  // put your setup code here, to run once:
  #ifdef DEBUG
  Serial.begin(115200);
  /*
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
    Setting GPIO3 (RX) as an OUTPUT is NOT recommended if you can avoid it because it is
    easy to short out when next re-programming. Note carefully: If you are using GPIO3 (RX)
    as an output, add a small resistor say 330R in series to prevent accidentally shorting
    it out when you attach your programming leads.
  */
  Serial.println("Start!");
  #endif
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, OFF);
  ds.requestTemp();  // первый запрос на измерение

  // базу данных запускаем до подключения к точке
#ifdef ESP32
  LittleFS.begin(true);
#else
  LittleFS.begin();
#endif
  db.begin();
  db_init(db);

  regulatorReload();
  timerRegulator.setInterval(db[kk::period].toInt() * 1000);

  // ======== WIFI ========

  // STA
  WiFi.mode(WIFI_AP_STA);
  WifiConnect();

  // AP
  WiFi.softAP(String("AP ")+db[kk::hostname].toString());
  LOG(print, "AP: ");
  LOG(println, WiFi.softAPIP());

  // ======== SETTINGS ========
  sett.begin();
  sett.onBuild(build);
  sett.onUpdate(update);

}

// Получение температуры, взято из примера GyverDS18
void tempTick() {
  if(timerTemperature.isReady()) {
    if (ds.ready()) {         // измерения готовы по таймеру
      if (ds.readTemp()) {  // если чтение успешно
        cur_temp = ds.getTemp() + db[kk::tcor].toFloat();
      } else {
        cur_temp = -100.0f;
      }

      ds.requestTemp();  // запрос следующего измерения
    }
  }
}

// Запуск mDNS
void startMDNS() {
  // Включить mDNS сервер, для определения адресов вида http://hostname.local
  if( wifi_connected ) {
    #ifdef ESP32
    if(MDNS.begin(db[kk::hostname].str())) {
    #else // ESP8266
    if(MDNS.begin(db[kk::hostname].str(), WiFi.localIP())) {
    #endif
      MDNS.addService("http", "tcp", 80);
      fl_mdns = true;
      LOG(println, PSTR("MDNS responder started"));
    }
  }
}

/* Проверка подключился ли WiFi
  Если подключился, то включить mDNS
  По хорошему тут должно быть и обновление WEB, но я не знаю как себя ведёт библиотека Settings если WiFi пропал, а потом появился...
*/
bool checkWifi() {
  if( WiFi.status() == WL_CONNECTED ) {
    if( ! wifi_connected ) { // wifi подключился
      wifi_connected = true;
      startMDNS();
    } else {
      // HTTP.handleClient();
      #ifdef ESP8266
      if(fl_mdns) MDNS.update();
      #endif
    }
  } else {
    if( fl_mdns ) {
      #ifdef ESP32
      MDNS.disableWorkstation();
      #else // ESP8266
      MDNS.close();
      #endif
      fl_mdns = false;
      LOG(println, PSTR("MDNS responder stoped"));
    }
     wifi_connected = false;
  }
  return wifi_connected;
}

// А тут вся могучая логика :)
void loop() {
  // put your main code here, to run repeatedly:
  tempTick();
  sett.tick();
  if( checkWifi() ) {
    ;
  }
  // если в web поменялись настройки, то загрузить их в библиотеку
  if( need_reload ) regulatorReload();
  // если прошло время после включения, то погасить точку доступа
  if( millis() > TIME_TO_AP && WiFi.softAPgetStationNum() == 0 ) {
    WiFi.mode(WIFI_STA);
  }
  // логика работы термостата, взято из примера GyverRelay
  if( fl_on && timerRegulator.isReady() ) {
    regulator.input = cur_temp;   // сообщаем регулятору текущую температуру
    cur_relay = regulator.compute(db[kk::period].toInt()) ? ON: OFF;
    if( ! fl_force ) {
      relay = cur_relay;
      digitalWrite(PIN_RELAY, cur_relay);
      LOG(printf_P, PSTR("relay is %i\n"), cur_relay);
    }
  }
  // отключает временное переключение реле, через минуту
  if( temp_switch > 0 && temp_switch + 60000 < millis() ) {
    digitalWrite(PIN_RELAY, cur_relay);
    relay = cur_relay;
    temp_switch = 0;
    fl_force = false;
  }
}
