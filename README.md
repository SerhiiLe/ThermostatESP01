# ThermostatESP01
Simple, stand-alone version of a thermal relay with web control, on are ESP-01(s) module.

## (RU) Очень простой автономный термостат с управление через web.

Построен вокруг библиотек от AlexGyver. Нет MQTT, полная автономия, максимальная простота.
Можно сделать закладку http://relay.local и не искать IP. Название можно поменять в настройках, при желании.
После включения пять минут работает точка доступа "AP relay", для простой настройки. Пароля нет.
При первом подключении рекомендую зайти на точку и выбрать свою сеть WiFi, тогда реле будет доступно всегда.

### Сборка (компиляция)

Для сборки надо установить библиотеки от [AlexGyver](https://github.com/gyverlibs):
- Settings (со всеми зависимостями)
- GyverDS18
- GyverRelay
- GyverHTTP желательно не не обязательно.

В менеджере плат выбрать "Generic ESP8266 Module".
Все настройки можно оставить по умолчанию, но если модуль не с 1Мб flash, то подобрать под свою плату.
При большом желании можно собрать под разные esp32, но это как забивать микроскопом гвозди,
по цене самого простого esp32 можно купить два модуля esp01s + реле. 

### Сборка (железо)

У модулей ESP01 две проблемы:
- щелкает реле при включении
- иногда при срабатывании реле модуль подвисает

Метод борьбы простой - припаять два конденсатора, один по питанию 3.3В, второй на ноги оптопары
[![Sample1](https://github.com/SerhiiLe/ThermostatESP01/blob/main/esp01s-relay1.jpg)](https://github.com/SerhiiLe/ThermostatESP01/blob/main/esp01s-relay1.jpg)
[![Sample2](https://github.com/SerhiiLe/ThermostatESP01/blob/main/esp01s-relay2.png)](https://github.com/SerhiiLe/ThermostatESP01/blob/main/esp01s-relay2.png)

Я поставил оба по 470mF, но пишут что емкость может меньше, надо пробовать.

Это избавляет только дребезга при включении, если нажать копку сброса на плате, то дребезг будет.

Реле работает по низкому уровню, то есть для срабатывания нужен 0, а 1 выключает реле. Если у Вас другое реле, то поменть логику можно
в файле defaults.h

Не забудьте поставить резистор 4.7к между питанием 3.3В и информационным проводом DS18B20.

## (EN) Very simple stand-alone thermostat with web control.

Built around libraries from AlexGyver. No MQTT, full autonomy, maximum simplicity.
You can bookmark http://relay.local and not look for IP. The name can be changed in the settings, if desired.
After power on, the access point "AP relay" works for five minutes, for simple setup. There is no password.
When connecting for the first time, I recommend going to the point and choosing your WiFi network, then the relay will always be available.

The interface is in Russian, but can be easily translated into any language, everything that is displayed on the screen is in the "build" function,
you just need to replace the phrases with your language.

### Assembly (compilation)

To build, you need to install libraries from [AlexGyver](https://github.com/gyverlibs):
- Settings (with all dependencies)
- GyverDS18
- GyverRelay
- GyverHTTP (desirable but not required)

In the board manager, select "Generic ESP8266 Module".
All settings can be left by default, but if the module does not have 1MB flash, then select for your board.
If you really want, you can assemble it for different esp32, but it's like hammering nails with a microscope,
for the price of the simplest esp32, you can buy two esp01s modules + a relay.

### Assembly (hardware)

These modules have a bounce when turned on. To avoid this, you need to put a capacitor, as in the pictures above.
It is also advisable to put a capacitor on the 3.3V power supply.

The relay operates at a low level, that is, 0 is needed to trigger, and 1 turns off the relay. If you have a different relay, you can change the logic
in the defaults.h file.

Don't forget to put a 4.7k resistor between the 3.3V supply and the DS18B20 data wire.

## P.S.
![Screenshot1](https://github.com/SerhiiLe/ThermostatESP01/blob/main/Screenshot_1.jpg)]
![Screenshot2](https://github.com/SerhiiLe/ThermostatESP01/blob/main/Screenshot_2.jpg)]
![Screenshot3](https://github.com/SerhiiLe/ThermostatESP01/blob/main/Screenshot_3.jpg)]

