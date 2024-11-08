#ifndef defaults_h
#define defaults_h
/*
тут должны быть всякие разные параметры
*/

enum kk : size_t {
  wifi_ssid,
  wifi_pass,
  hostname,

  temp,
  hyst,
  gain,
  period,
  tcor,
  on,
};

// Значения по умолчанию

void db_init(GyverDBFile &db) {
  db.init(kk::wifi_ssid, "");
  db.init(kk::wifi_pass, "");
  db.init(kk::hostname, "relay");

  db.init(kk::temp, 24.0f);
  db.init(kk::hyst, 1.0f);
  db.init(kk::gain, 100.0f);
  db.init(kk::period, 5);
  db.init(kk::tcor, 0.0f);
  db.init(kk::on, 1);
}

//----------------------------------------------------


#define ON 0
#define OFF 1

#if defined(LOG)
#undef LOG
#endif

#ifdef DEBUG
	//#define LOG                   Serial
	#define LOG(func, ...) Serial.func(__VA_ARGS__)
#else
	#define LOG(func, ...) ;
#endif

#endif