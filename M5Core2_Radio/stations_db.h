#ifndef _STATIONS_DB_H_
#define _STATIONS_DB_H_

#include <Arduino.h>

struct StationItem {
    const char* name;
    const char* state;
    const char* language;
    const char* url;
};

#define TOTAL_ALL_STATIONS 51
#define TOTAL_LANGUAGES 3
#define TOTAL_STATES 4

const StationItem ALL_STATIONS[TOTAL_ALL_STATIONS] PROGMEM = {
    { "Akashvani Thrissur", "Kerala", "Malayalam", "https://radio.wavespb.com/live/f70fdeca437dc326/f70fdeca437dc326.m3u8" },
    { "AIR Thrissur Relay", "Kerala", "Malayalam", "https://airrelay.onrender.com/thrissur.mp3" },
    { "FM Rainbow Kochi", "Kerala", "Malayalam", "https://radio.wavespb.com/live/7df6f2a8c3c4d33b/7df6f2a8c3c4d33b.m3u8" },
    { "Akashvani Kochi 102.3", "Kerala", "Malayalam", "https://radio.wavespb.com/live/70400e7510e87cdf/70400e7510e87cdf.m3u8" },
    { "Akashvani Kozhikode (Calicut)", "Kerala", "Malayalam", "https://radio.wavespb.com/live/8321393de70015fc/8321393de70015fc.m3u8" },
    { "Akashvani Kozhikode Real FM", "Kerala", "Malayalam", "https://radio.wavespb.com/live/b69c296065db7627/b69c296065db7627.m3u8" },
    { "Akashvani Kannur", "Kerala", "Malayalam", "https://radio.wavespb.com/live/b82c91a395fc4a7d/b82c91a395fc4a7d.m3u8" },
    { "Akashvani Devikulam", "Kerala", "Malayalam", "https://radio.wavespb.com/live/e97acc829da9bf2a/e97acc829da9bf2a.m3u8" },
    { "Akashvani Manjeri", "Kerala", "Malayalam", "https://radio.wavespb.com/live/58390a2ed33cea4a/58390a2ed33cea4a.m3u8" },
    { "Akashvani Thiruvananthapuram", "Kerala", "Malayalam", "https://air.pc.cdn.bitgravity.com/air/live/pbaudio001/playlist.m3u8" },
    { "Ananthapuri FM 101.9", "Kerala", "Malayalam", "https://air.pc.cdn.bitgravity.com/air/live/pbaudio229/playlist.m3u8" },
    { "AIR Alappuzha", "Kerala", "Malayalam", "https://air.pc.cdn.bitgravity.com/air/live/pbaudio230/playlist.m3u8" },
    { "VB Malayalam (Vividh Bharati)", "Kerala", "Malayalam", "https://radio.wavespb.com/live/ad3a8436a329e2d6/ad3a8436a329e2d6.m3u8" },
    { "Akashvani Kerala State", "Kerala", "Malayalam", "https://radio.wavespb.com/live/6ff13de7ea9b53d7/6ff13de7ea9b53d7.m3u8" },
    { "AIR Malayalam National", "National", "Malayalam", "http://air.pc.cdn.bitgravity.com/air/live/pbaudio230/playlist.m3u8" },
    { "Akashvani Kavaratti", "Kerala", "Malayalam", "https://radio.wavespb.com/live/ffb3825f86c4b9e3/ffb3825f86c4b9e3.m3u8" },
    { "Raagam AIR 24x7 Carnatic", "National", "Malayalam", "https://airhlspush.pc.cdn.bitgravity.com/httppush/hlspbaudioragam/hlspbaudioragam_Auto.m3u8" },
    { "Radio Suno 91.7 FM", "Kerala", "Malayalam", "http://playerservices.streamtheworld.com/api/livestream-redirect/SUNO917_SC" },
    { "Radio Suno Malayalam 91.7", "Kerala", "Malayalam", "https://playerservices.streamtheworld.com/api/livestream-redirect/SUNO917_SC" },
    { "Radio Keralam", "Kerala", "Malayalam", "https://ice31.securenetsystems.net/RADIOKERAL" },
    { "Radio Digital Malayali", "Kerala", "Malayalam", "https://radio.digitalmalayali.in/listen/stream/radio.mp3" },
    { "Radio Macfast 90.4 FM", "Kerala", "Malayalam", "https://icecast.octosignals.com/radiomacfast" },
    { "Radio Mattoli 90.4 FM", "Kerala", "Malayalam", "https://cast1.my-control-panel.com/proxy/radiomattoli/stream" },
    { "Ahalia FM 90.4 Palakkad", "Kerala", "Malayalam", "https://cast1.my-control-panel.com/proxy/ahaliafm/stream" },
    { "Radio Tirur", "Kerala", "Malayalam", "https://sonic01.instainternet.com/8002/stream" },
    { "London Malayalam Radio", "International", "Malayalam", "https://ais-edge105-live365-dal02.cdnstream.com/a50671" },
    { "Aaha Radio Malayalam", "Kerala", "Malayalam", "https://s2.radio.co/s3801784f1/listen" },
    { "Kaathoram Live Malayalam", "Kerala", "Malayalam", "https://c30.radioboss.fm/stream/560" },
    { "Kancheeravam Radio", "Kerala", "Malayalam", "https://radiosavre.com:8020/radio.mp3" },
    { "Recast FM Malayalam", "Kerala", "Malayalam", "https://usa8.fastcast4u.com/proxy/auralradio?mp=/1/;stream.mp3" },
    { "Live FM Malayalam", "Kerala", "Malayalam", "https://stream.aiir.com/dbv0rxpwp6ytv" },
    { "Ente Radio Malayalam", "Kerala", "Malayalam", "https://cast1.my-control-panel.com/proxy/enteradio/stream" },
    { "Shahimsha Online Radio", "Kerala", "Malayalam", "https://radio.shahimsha.com/listen/shahimsha/radio.mp3" },
    { "Peace Radio Malayalam", "Kerala", "Malayalam", "https://peaceradio.out.airtime.pro/peaceradio_a" },
    { "Peace Radio Quran", "Kerala", "Malayalam", "http://stream.peaceradio.com:8000/quran/high" },
    { "Shaiva Lahari Malayalam", "Kerala", "Malayalam", "https://radio.shaivam.org/listen/shaiva-lahari/radio.mp3" },
    { "DVN Radio Malayalam", "Kerala", "Malayalam", "https://ice31.securenetsystems.net/DVN" },
    { "Amen Radio Malayalam", "Kerala", "Malayalam", "https://ice7.securenetsystems.net/AMENFM" },
    { "Luminous Radio Malayalam", "Kerala", "Malayalam", "https://patmos.cdnstream.com/proxy/luminous/?mp=/stream" },
    { "Rafa Radio Malayalam", "Kerala", "Malayalam", "https://gains.reviveradio.net/proxy/rafaradio?mp=/stream" },
    { "Revive Radio Malayalam", "Kerala", "Malayalam", "https://gains.reviveradio.net/proxy/revivemalayalam?mp=/stream" },
    { "Jesus Reigns Radio", "Kerala", "Malayalam", "https://gains.reviveradio.net/proxy/jesusreigns?mp=/stream" },
    { "BBC World Service News", "International", "English", "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service" },
    { "BBC World Service East Asia", "International", "English", "https://stream.live.vc.bbcmedia.co.uk/bbc_world_service_east_asia" },
    { "BBC Radio 1 UK", "International", "English", "http://a.files.bbci.co.uk/ms6/live/3441A116-B12E-4D2F-ACA8-C1984642FA4B/audio/simulcast/hls/nonuk/pc_hd_abr_v2/ak/bbc_radio_one.m3u8" },
    { "BBC Radio 2 UK", "International", "English", "http://as-hls-ww-live.akamaized.net/pool_74208725/live/ww/bbc_radio_two/bbc_radio_two.isml/bbc_radio_two-audio%3d128000.norewind.m3u8" },
    { "BBC Radio 4 FM", "International", "English", "http://as-hls-ww-live.akamaized.net/pool_55057080/live/ww/bbc_radio_fourfm/bbc_radio_fourfm.isml/bbc_radio_fourfm-audio%3d128000.norewind.m3u8" },
    { "BBC Radio 5 Live", "International", "English", "http://as-hls-ww-live.akamaized.net/pool_89021708/live/ww/bbc_radio_five_live/bbc_radio_five_live.isml/bbc_radio_five_live-audio%3d128000.norewind.m3u8" },
    { "BBC Radio 6 Music", "International", "English", "http://as-hls-ww-live.akamaized.net/pool_81827798/live/ww/bbc_6music/bbc_6music.isml/bbc_6music-audio%3d128000.norewind.m3u8" },
    { "NPR 24 Hour News USA", "International", "English", "https://npr-ice.streamguys1.com/live.mp3" },
    { "Radio France International", "International", "English", "https://rfienanglais64k.ice.infomaniak.ch/rfienanglais-64.mp3" },
};

const char* const FILTER_LANGUAGES[TOTAL_LANGUAGES] = {
    "All Languages",
    "Malayalam",
    "English",
};

const char* const FILTER_STATES[TOTAL_STATES] = {
    "All Regions",
    "Kerala",
    "National",
    "International",
};

#endif // _STATIONS_DB_H_
