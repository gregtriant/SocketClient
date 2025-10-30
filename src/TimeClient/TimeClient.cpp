#include <WiFiUdp.h>
#include <NTPClient.h>

#include "TimeClient.h"

WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org", 0, 60000);

void TimeClient::begin(const char *TZ){
  ntpClient.begin();
  ntpClient.forceUpdate();
  syncSystemClock();

  // 🌍 Apply timezone & DST rules
  setenv("TZ", TZ, 1);
  tzset();
} 

void TimeClient::syncSystemClock() {
  ntpClient.update();
  unsigned long epoch = ntpClient.getEpochTime();

  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);  // 🔥 updates system time
  //- Serial.printf("Synced system time to %lu (UTC)\n", epoch);
}

void TimeClient::loop(){
  static unsigned long last_sync = 0;
  if(millis()-last_sync>update_interval){
    syncSystemClock();
    last_sync = millis();
  }
}

bool TimeClient::hasTime(){
  return ntpClient.isTimeSet();
}

bool TimeClient::getTime(int &hh, int &mm, int &ss){
    if(!hasTime()){
        hh = mm = ss = 0; //- sanity defaults...
        return false;
    }
    time_t epochTime = time(nullptr);
    struct tm *timeinfo = localtime(&epochTime);
    hh = timeinfo->tm_hour;
    mm = timeinfo->tm_min;
    ss = timeinfo->tm_sec;
    return true;
}

bool TimeClient::getDate(int &yy, int &mm, int &dd){
    if(!hasTime()) { 
        yy = 1900;
        mm = dd = 1;
        return false;
    }
    time_t epochTime = time(nullptr);
    struct tm *timeinfo = localtime(&epochTime);
    yy = timeinfo->tm_year+1900;
    mm = timeinfo->tm_mon+1;
    dd = timeinfo->tm_mday;
    return true;
}

// === Helper: compute nth weekday of a month (e.g. last Sunday in March) ===
int nthWeekdayOfMonth(int year, int month, int weekday, int nth) {
  // weekday: 0=Sunday..6=Saturday
  struct tm t = {0};
  t.tm_year = year - 1900;
  t.tm_mon  = month - 1;
  t.tm_mday = 1;
  mktime(&t);

  int firstWday = t.tm_wday;
  int day = 1 + ((7 + weekday - firstWday) % 7) + (nth - 1) * 7;

  // If nth == 5 → last occurrence
  struct tm test = t;
  test.tm_mday = day;
  mktime(&test);
  if (nth == 5) {
    while (true) {
      test.tm_mday += 7;
      mktime(&test);
      if (test.tm_mon != month - 1) {
        test.tm_mday -= 7;
        break;
      }
    }
    day = test.tm_mday;
  }
  return day;
}

// === Parse rule like "M3.5.0/03" ===
bool parseRule(const char* rule, int& month, int& nth, int& weekday, int& hour) {
  return sscanf(rule, "M%d.%d.%d/%d", &month, &nth, &weekday, &hour) == 4;
}

// === Parse base offset and DST offset from TZ string ===
void parseOffsets(const char* tz, int& baseOffset, int& dstOffset) {
  // Format example: "EET-2EEST"
  // meaning: standard offset = -2 hours → UTC+2
  char stdName[10], dstName[10];
  float stdHours = 0;
  //-int consumed = 0;

  // Try to read: NAMEoffsetNAME
  if (sscanf(tz, "%[A-Za-z]%f%[A-Za-z]", stdName, &stdHours, dstName) >= 2) {
    baseOffset = (int)(-stdHours * 3600);  // POSIX sign inversion
    if (strlen(dstName) > 0)
      dstOffset = baseOffset + 3600; // default DST +1h
    else
      dstOffset = baseOffset;
  } else {
    baseOffset = 0;
    dstOffset = 0;
  }
}

// === Return current UTC offset (in seconds) for a TZ string ===
long getTZOffset(const char* tzInfo) {
  int baseOffset = 0, dstOffset = 0;
  parseOffsets(tzInfo, baseOffset, dstOffset);

  const char* comma1 = strchr(tzInfo, ',');
  if (!comma1) return baseOffset; // no DST rules

  const char* comma2 = strchr(comma1 + 1, ',');
  if (!comma2) return baseOffset;

  char startRule[20], endRule[20];
  strncpy(startRule, comma1 + 1, comma2 - comma1 - 1);
  startRule[comma2 - comma1 - 1] = '\0';
  strcpy(endRule, comma2 + 1);

  int sm, sn, sw, sh;
  int em, en, ew, eh;
  if (!parseRule(startRule, sm, sn, sw, sh)) return baseOffset;
  if (!parseRule(endRule, em, en, ew, eh)) return baseOffset;

  time_t now = time(nullptr);
  struct tm *utc = gmtime(&now);
  int year = utc->tm_year + 1900;

  int startDay = nthWeekdayOfMonth(year, sm, sw, sn);
  int endDay   = nthWeekdayOfMonth(year, em, ew, en);

  struct tm start = {0}, end = {0};
  start.tm_year = end.tm_year = year - 1900;
  start.tm_mon  = sm - 1;
  start.tm_mday = startDay;
  start.tm_hour = sh;
  end.tm_mon    = em - 1;
  end.tm_mday   = endDay;
  end.tm_hour   = eh;

  time_t start_t = mktime(&start);
  time_t end_t   = mktime(&end);

  // handle wrap-around (southern hemisphere)
  bool dstActive = false;
  if (end_t < start_t) {
    dstActive = (now >= start_t || now < end_t);
  } else {
    dstActive = (now >= start_t && now < end_t);
  }

  return dstActive ? dstOffset : baseOffset;
}