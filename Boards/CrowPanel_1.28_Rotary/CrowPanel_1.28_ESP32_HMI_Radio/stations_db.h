#ifndef _STATIONS_DB_H_
#define _STATIONS_DB_H_

#include <Arduino.h>

struct StationInfo {
    const char* name;
    const char* state;
    const char* language;
    const char* url;
};

// Verified 100% Active Malayalam Stations (Direct Verified CDN Streams)
#define TOTAL_STARTER_STATIONS 10

const StationInfo STARTER_STATIONS[TOTAL_STARTER_STATIONS] = {
    { "AIR Thrissur",    "Kerala", "Malayalam", "https://d1cvqgmbcpg5yn.cloudfront.net/f70fdeca437dc326/f70fdeca437dc326.m3u8" },
    { "AIR Calicut",     "Kerala", "Malayalam", "https://d1cvqgmbcpg5yn.cloudfront.net/8321393de70015fc/8321393de70015fc.m3u8" },
    { "AIR Manjeri",     "Kerala", "Malayalam", "https://d3hrxqn1tritdh.cloudfront.net/58390a2ed33cea4a/58390a2ed33cea4a.m3u8" },
    { "AIR Real FM",     "Kerala", "Malayalam", "https://d1tmej9eu7kw5c.cloudfront.net/b69c296065db7627/b69c296065db7627.m3u8" },
    { "AIR Kochi",       "Kerala", "Malayalam", "https://d1tmej9eu7kw5c.cloudfront.net/70400e7510e87cdf/70400e7510e87cdf.m3u8" },
    { "Rainbow Kochi",   "Kerala", "Malayalam", "https://d3hrxqn1tritdh.cloudfront.net/7df6f2a8c3c4d33b/7df6f2a8c3c4d33b.m3u8" },
    { "VB Ananthapuri",  "Kerala", "Malayalam", "https://d3hrxqn1tritdh.cloudfront.net/ad3a8436a329e2d6/ad3a8436a329e2d6.m3u8" },
    { "AIR Kannur",      "Kerala", "Malayalam", "https://d1cvqgmbcpg5yn.cloudfront.net/b82c91a395fc4a7d/b82c91a395fc4a7d.m3u8" },
    { "Ahalia FM 90.4",  "Kerala", "Malayalam", "https://cast1.my-control-panel.com/proxy/ahaliafm/stream" },
    { "AIR Kerala",      "Kerala", "Malayalam", "https://d3hrxqn1tritdh.cloudfront.net/6ff13de7ea9b53d7/6ff13de7ea9b53d7.m3u8" }
};

#endif // _STATIONS_DB_H_
