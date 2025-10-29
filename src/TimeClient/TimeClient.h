#ifndef _TIME_CLIENT_H
#define _TIME_CLIENT_H 1

#include <time.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

extern NTPClient timeClient;
long getTZOffset(const char* tzInfo);

#endif // _TIME_CLIENT_H