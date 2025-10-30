#ifndef _TIME_CLIENT_H
#define _TIME_CLIENT_H 1

#include <time.h>

class TimeClient {
    unsigned long update_interval;
public:
    TimeClient(long upd=15L*60000L) {
        update_interval = upd;
    }
    void begin(const char *TZ);
    void loop();
    bool hasTime();
    bool getTime(int &hh, int &mm, int &ss);
    bool getDate(int &yy, int &mm, int &dd);
protected:
    void syncSystemClock();
};

#endif // _TIME_CLIENT_H