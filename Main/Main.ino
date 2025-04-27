#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <FastLED.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "WifiCredentials.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>
#include <cstring>

constexpr bool DEBUG = false;

#define DPRINT(...)                    \
    do                                 \
    {                                  \
        if (DEBUG)                     \
            Serial.print(__VA_ARGS__); \
    } while (0)
#define DPRINTLN(...)                    \
    do                                   \
    {                                    \
        if (DEBUG)                       \
            Serial.println(__VA_ARGS__); \
    } while (0)
#define DPRINTF(...)                    \
    do                                  \
    {                                   \
        if (DEBUG)                      \
            Serial.printf(__VA_ARGS__); \
    } while (0)
//=============================================================================
//                            CONFIGURATION
//=============================================================================

// Time configuration
constexpr long GMT_OFFSET_SEC = -7 * 3600;
constexpr int DAYLIGHT_OFFSET_SEC = 0;

// Network fetch timing
constexpr unsigned long FETCH_INTERVAL_MS = 5 * 60 * 1000UL;
constexpr unsigned long DISPLAY_UPDATE_INTERVAL_MS = 7UL;

// Display brightness (0.0 – 1.0)
constexpr float MAX_BRIGHTNESS = 0.05f;

// Pin assignments
namespace PinConfig
{
    constexpr int DATA = 11;
    constexpr int CLK = 13;
    constexpr int CS = 12;
    constexpr int RED = 8;
    constexpr int ORANGE = 6;
    constexpr int YELLOW = 9;
    constexpr int BLUE = 7;
    constexpr int GREEN = 5;
    constexpr int CALTRAIN = 10;
    constexpr int ACE = 17;
    constexpr int LEGEND = 18;
}

// LED strip lengths
constexpr uint8_t RED_LEN = 30;
constexpr uint8_t ORANGE_LEN = 27;
constexpr uint8_t YELLOW_LEN = 34;
constexpr uint8_t BLUE_LEN = 28;
constexpr uint8_t GREEN_LEN = 34;
constexpr uint8_t CAL_LEN = 30;
constexpr uint8_t ACE_LEN = 37;
constexpr uint8_t LEGEND_LEN = 7;

// Lines enumeration
enum Line
{
    RED,
    ORANGE,
    YELLOW,
    GREEN,
    BLUE,
    CALTRAIN,
    ACE,
    NUM_LINES
};

// External-to-internal station mapping
enum StationId
{
    VASCO_RD = 0,
    LIVERMORE,
    PLEASANTON,
    FREMONT,
    GREAT_AMERICA,
    SANTA_CLARA,
    ST_12TH_ST_OAKLAND_CITY_CENTER,
    ST_16TH_ST_MISSION,
    ST_19TH_ST_OAKLAND,
    ST_22ND_STREET,
    ST_24TH_ST_MISSION,
    ANTIOCH,
    ASHBY,
    BALBOA_PARK,
    BAY_FAIR,
    BAYSHORE,
    BELMONT,
    BERRYESSA_NORTH_SAN_JOSE,
    BROADWAY,
    BURLINGAME,
    CALIFORNIA_AVE,
    CASTRO_VALLEY,
    CIVIC_CENTER_UN_PLAZA,
    COLISEUM,
    COLLEGE_PARK,
    COLMA,
    CONCORD,
    DALY_CITY,
    DOWNTOWN_BERKELEY,
    DUBLIN_PLEASANTON,
    EL_CERRITO_DEL_NORTE,
    EL_CERRITO_PLAZA,
    EMBARCADERO,
    FRUITVALE,
    GLEN_PARK,
    HAYWARD,
    HAYWARD_PARK,
    HILLSDALE,
    LAFAYETTE,
    LAKE_MERRITT,
    LAWRENCE,
    MACARTHUR,
    MENLO_PARK,
    MILLBRAE,
    MILPITAS,
    MONTGOMERY_ST,
    MOUNTAIN_VIEW,
    NORTH_BERKELEY,
    NORTH_CONCORD_MARTINEZ,
    ORINDA,
    PALO_ALTO,
    PITTSBURG_BAY_POINT,
    PITTSBURG_CENTER,
    PLEASANT_HILL_CONTRA_COSTA_CENTRE,
    POWELL_ST,
    REDWOOD_CITY,
    RICHMOND,
    ROCKRIDGE,
    SAN_ANTONIO,
    SAN_BRUNO,
    SAN_CARLOS,
    SAN_FRANCISCO,
    SAN_FRANCISCO_INTERNATIONAL_AIRPORT,
    SAN_JOSE_DIRIDON,
    SAN_LEANDRO,
    SAN_MATEO,
    SOUTH_HAYWARD,
    SOUTH_SAN_FRANCISCO,
    STANFORD,
    SUNNYVALE,
    TAMIEN,
    UNION_CITY,
    WALNUT_CREEK,
    WARM_SPRINGS_SOUTH_FREMONT,
    WEST_DUBLIN_PLEASANTON,
    WEST_OAKLAND,
    NUM_STATIONS
};

//=============================================================================
//                             GLOBAL DATA
//=============================================================================

// LED buffers for each line
CRGB redLine[RED_LEN];
CRGB orangeLine[ORANGE_LEN];
CRGB yellowLine[YELLOW_LEN];
CRGB blueLine[BLUE_LEN];
CRGB greenLine[GREEN_LEN];
CRGB calTrain[CAL_LEN];
CRGB aceTrain[ACE_LEN];
CRGB legend[LEGEND_LEN];

// Easy indexed access
CRGB *lineLeds[NUM_LINES] = {
    redLine, orangeLine, yellowLine,
    greenLine, blueLine, calTrain, aceTrain};
uint8_t lineLen[NUM_LINES] = {
    RED_LEN, ORANGE_LEN, YELLOW_LEN,
    GREEN_LEN, BLUE_LEN, CAL_LEN, ACE_LEN};

// Storage for the parsed train trips
struct StopRecord
{
    String tripId;
    String lineId;
    StationId station;
    time_t arrival;
    time_t departure;
};
struct Trip
{
    String tripId;
    String lineId;
    std::vector<StopRecord> stops;
};
std::vector<Trip> trips;

struct TrainPosition
{
    bool valid;
    bool isTraveling;
    uint8_t idx;
    uint8_t fromIdx;
    uint8_t toIdx;
    time_t departTime;
    time_t arriveTime;
};

// Supabase function endpoint
const char *SUPABASE_FN = "https://icalcigcgkzaxyvhnfgc.supabase.co/functions/v1/getStatus";

// Timing trackers
unsigned long lastFetchMs = 0;
unsigned long lastDrawMs = 0;
unsigned long lastDispMs = 0;

// Segment Text
constexpr uint8_t SEG_F = 0x47;
constexpr uint8_t SEG_A = 0x77;
constexpr uint8_t SEG_I = 0x06;
constexpr uint8_t SEG_L = 0x0E;

const uint8_t segFailCodes[4] = {SEG_F, SEG_A, SEG_I, SEG_L};

//=============================================================================
//                            UTILITY FUNCTIONS
//=============================================================================

// DST HEADACHE IM GONNA KMS
static long compute_offset(void)
{
    time_t now = time(NULL);

    struct tm gmt = {0}, local = {0};
    gmtime_r(&now, &gmt);
    localtime_r(&now, &local);

    gmt.tm_isdst = 0;

    // bro why the hell is this working its DST rn
    // i swear if it breaks after dst
    local.tm_isdst = 0;

    time_t t_gmt = mktime(&gmt);
    time_t t_local = mktime(&local);

    return t_local - t_gmt;
}

time_t parseISO(const char *s)
{
    struct tm tm = {};
    if (!strptime(s, "%Y-%m-%dT%H:%M:%S", &tm))
        return (time_t)-1;

    tm.tm_isdst = -1;
    time_t t_local = mktime(&tm);

    return t_local + compute_offset();
}

String formatTime(time_t t)
{
    struct tm tm;
    localtime_r(&t, &tm);
    char buf[20];
    snprintf(buf, sizeof(buf),
             "%04d-%02d-%02d %02d:%02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    return String(buf);
}

Line lineFromString(const String &s)
{
    String u = s;
    u.toUpperCase();
    u.trim();
    int pos = 0;
    while (pos < u.length() && isAlpha(u.charAt(pos)))
    {
        pos++;
    }
    if (pos > 0)
    {
        u = u.substring(0, pos);
    }

    if (u == "RED")
        return RED;
    else if (u == "ORANGE")
        return ORANGE;
    else if (u == "YELLOW")
        return YELLOW;
    else if (u == "GREEN")
        return GREEN;
    else if (u == "BLUE")
        return BLUE;
    else if (u == "LOCAL" || u == "SOUTH" || u == "LIMITED" || u == "EXPRESS")
        return CALTRAIN;
    else if (u == "ACETRAIN")
        return ACE;
    return RED;
}

CRGB colorForLine(Line L)
{
    switch (L)
    {
    case RED:
        return CRGB::Red;
    case ORANGE:
        return CRGB(204, 85, 0);
    case YELLOW:
        return CRGB::Yellow;
    case GREEN:
        return CRGB::Green;
    case BLUE:
        return CRGB::Blue;
    case CALTRAIN:
        return CRGB::Maroon;
    case ACE:
        return CRGB::Purple;
    default:
        return CRGB::White;
    }
}

CRGB dynamicLineColor(Line line, const TrainPosition &p, time_t nowMs)
{
    CRGB base = colorForLine(line);

    if (!p.valid)
    {
        return CRGB::Black;
    }

    if (p.isTraveling)
    {
        const uint16_t period = 1600;
        uint16_t phase = (nowMs + p.idx * 3600) % period;
        float t = float(phase) / float(period);
        float angle = 2 * PI * t - (PI / 2);
        float brightness = 0.5f * (1.0f + sinf(angle));
        uint8_t bri8 = uint8_t(brightness * 255);
        return base.nscale8_video(bri8);
    }
    else
    {
        return base;
    }
}

//==============================================================================
//                    EXTERNAL ID → StationId MAPPING
//==============================================================================

StationId stationIdFromExternalId(const std::string &raw)
{
    std::string id = raw;
    std::transform(id.begin(), id.end(), id.begin(), ::toupper);

    if (id == "73368" || id == "902901" || id == "902902")
        return FREMONT;
    if (id == "73422")
        return GREAT_AMERICA;
    if (id == "73548")
        return LIVERMORE;
    if (id == "73757")
        return PLEASANTON;
    if (id == "73752" || id == "70261" || id == "70262")
        return SAN_JOSE_DIRIDON;
    if (id == "73722" || id == "70241" || id == "70242")
        return SANTA_CLARA;
    if (id == "73827")
        return VASCO_RD;

    if (id == "900101" || id == "900102" || id == "900103")
        return ST_12TH_ST_OAKLAND_CITY_CENTER;
    if (id == "901501" || id == "901502")
        return ST_16TH_ST_MISSION;
    if (id == "900201" || id == "900202" || id == "900203")
        return ST_19TH_ST_OAKLAND;
    if (id == "901601" || id == "901602")
        return ST_24TH_ST_MISSION;
    if (id == "908301" || id == "908302")
        return ANTIOCH;
    if (id == "904101" || id == "904102")
        return ASHBY;
    if (id == "901801" || id == "901802")
        return BALBOA_PARK;
    if (id == "902501" || id == "902502")
        return BAY_FAIR;
    if (id == "909501" || id == "909502")
        return BERRYESSA_NORTH_SAN_JOSE;
    if (id == "905101" || id == "905102")
        return CASTRO_VALLEY;
    if (id == "901401" || id == "901402")
        return CIVIC_CENTER_UN_PLAZA;
    if (id == "902301" || id == "902302")
        return COLISEUM;
    if (id == "906101" || id == "906102")
        return COLMA;
    if (id == "903601" || id == "903602")
        return CONCORD;
    if (id == "901901" || id == "901902" || id == "901903")
        return DALY_CITY;
    if (id == "904201" || id == "904202")
        return DOWNTOWN_BERKELEY;
    if (id == "905301" || id == "905302")
        return DUBLIN_PLEASANTON;
    if (id == "904501" || id == "904502")
        return EL_CERRITO_DEL_NORTE;
    if (id == "904401" || id == "904402")
        return EL_CERRITO_PLAZA;
    if (id == "901161" || id == "901162")
        return EMBARCADERO;
    if (id == "902201" || id == "902202")
        return FRUITVALE;
    if (id == "901701" || id == "901702")
        return GLEN_PARK;
    if (id == "902601" || id == "902602")
        return HAYWARD;
    if (id == "903301" || id == "903302")
        return LAFAYETTE;
    if (id == "902101" || id == "902102")
        return LAKE_MERRITT;
    if (id == "900301" || id == "900302" || id == "900303" || id == "900304")
        return MACARTHUR;
    if (id == "906403")
        return MILLBRAE;
    if (id == "909401" || id == "909402")
        return MILPITAS;
    if (id == "901201" || id == "901202")
        return MONTGOMERY_ST;
    if (id == "904301" || id == "904302")
        return NORTH_BERKELEY;
    if (id == "903701" || id == "903702")
        return NORTH_CONCORD_MARTINEZ;
    if (id == "903201" || id == "903202")
        return ORINDA;
    if (id == "903801" || id == "903802")
        return PITTSBURG_BAY_POINT;
    if (id == "908201" || id == "908202")
        return PITTSBURG_CENTER;
    if (id == "903501" || id == "903502")
        return PLEASANT_HILL_CONTRA_COSTA_CENTRE;
    if (id == "901301" || id == "901302")
        return POWELL_ST;
    if (id == "904601" || id == "904602")
        return RICHMOND;
    if (id == "903101" || id == "903102")
        return ROCKRIDGE;
    if (id == "906301" || id == "906302")
        return SAN_BRUNO;
    if (id == "907101" || id == "907102" || id == "907103")
        return SAN_FRANCISCO_INTERNATIONAL_AIRPORT;
    if (id == "902401" || id == "902402")
        return SAN_LEANDRO;
    if (id == "902701" || id == "902702")
        return SOUTH_HAYWARD;
    if (id == "906201" || id == "906202" || id == "70041" || id == "70042")
        return SOUTH_SAN_FRANCISCO;
    if (id == "902801" || id == "902802")
        return UNION_CITY;
    if (id == "903401" || id == "903402")
        return WALNUT_CREEK;
    if (id == "909201" || id == "909202")
        return WARM_SPRINGS_SOUTH_FREMONT;
    if (id == "905201" || id == "905202")
        return WEST_DUBLIN_PLEASANTON;
    if (id == "901101" || id == "901102")
        return WEST_OAKLAND;

    if (id == "70021" || id == "70022")
        return ST_22ND_STREET;
    if (id == "70031" || id == "70032")
        return BAYSHORE;
    if (id == "70121" || id == "70122")
        return BELMONT;
    if (id == "70251" || id == "70252")
        return COLLEGE_PARK;
    if (id == "70071" || id == "70072")
        return BROADWAY;
    if (id == "70081" || id == "70082")
        return BURLINGAME;
    if (id == "70191" || id == "70192")
        return CALIFORNIA_AVE;
    if (id == "70101" || id == "70102")
        return HAYWARD_PARK;
    if (id == "70111" || id == "70112")
        return HILLSDALE;
    if (id == "70231" || id == "70232")
        return LAWRENCE;
    if (id == "70161" || id == "70162")
        return MENLO_PARK;
    if (id == "70061" || id == "70062")
        return MILLBRAE;
    if (id == "70211" || id == "70212")
        return MOUNTAIN_VIEW;
    if (id == "70171" || id == "70172")
        return PALO_ALTO;
    if (id == "70141" || id == "70142")
        return REDWOOD_CITY;
    if (id == "70201" || id == "70202")
        return SAN_ANTONIO;
    if (id == "70051" || id == "70052")
        return SAN_BRUNO;
    if (id == "70131" || id == "70132")
        return SAN_CARLOS;
    if (id == "70011" || id == "70012")
        return SAN_FRANCISCO;
    if (id == "70091" || id == "70092")
        return SAN_MATEO;
    if (id == "70221" || id == "70222")
        return SUNNYVALE;
    if (id == "70271" || id == "70272")
        return TAMIEN;

    DPRINTF("Unknown station ID: %s\n", id.c_str());

    return NUM_STATIONS;
}

//=============================================================================
//                       STATION INDEX LOOKUP
//=============================================================================

const uint8_t stationIndexMap[NUM_LINES][NUM_STATIONS] = {
    // RED
    {
        255, 255, 255, 255, 255, 255, 8, 20, 7, 255, 21, 255, 5, 23, 255, 255,
        255, 255, 255, 255, 255, 255, 19, 255, 255, 25, 255, 24, 4, 255, 1, 2,
        16, 255, 22, 255, 255, 255, 255, 255, 255, 6, 255, 29, 255, 17, 255, 3,
        255, 255, 255, 255, 255, 255, 18, 255, 0, 255, 255, 27, 255, 255, 28, 255,
        255, 255, 255, 26, 255, 255, 255, 255, 255, 255, 255, 9},
    // ORANGE
    {
        255, 255, 255, 8, 255, 255, 18, 255, 19, 255, 255, 255, 21, 255, 12, 255,
        255, 0, 255, 255, 255, 255, 255, 14, 255, 255, 255, 255, 22, 255, 25, 24,
        255, 15, 255, 11, 255, 255, 255, 16, 255, 20, 255, 255, 2, 255, 255, 23,
        255, 255, 255, 255, 255, 255, 255, 255, 26, 255, 255, 255, 255, 255, 255, 255,
        13, 255, 10, 255, 255, 255, 255, 9, 255, 4, 255, 255},
    // YELLOW
    {
        255, 255, 255, 255, 255, 255, 13, 25, 12, 255, 26, 0, 255, 28, 255, 255,
        255, 255, 255, 255, 255, 255, 24, 255, 255, 30, 5, 29, 255, 255, 255, 255,
        21, 255, 27, 255, 255, 255, 8, 255, 255, 11, 255, 255, 255, 22, 255, 255,
        4, 9, 255, 2, 1, 6, 23, 255, 255, 10, 255, 32, 255, 255, 33, 255,
        255, 255, 255, 31, 255, 255, 255, 255, 7, 255, 255, 14},
    // GREEN
    {
        255, 255, 255, 8, 255, 255, 255, 29, 255, 255, 30, 255, 255, 32, 12, 255,
        255, 0, 255, 255, 255, 255, 28, 14, 255, 255, 255, 33, 255, 255, 255, 255,
        25, 15, 31, 11, 255, 255, 255, 16, 255, 255, 255, 255, 2, 26, 255, 255,
        255, 255, 255, 255, 255, 255, 27, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        13, 255, 10, 255, 255, 255, 255, 9, 255, 4, 255, 18},
    // BLUE
    {
        255, 255, 255, 255, 255, 255, 255, 23, 255, 255, 24, 255, 255, 26, 6, 255,
        255, 255, 255, 255, 255, 4, 22, 8, 255, 255, 255, 27, 255, 0, 255, 255,
        19, 9, 25, 255, 255, 255, 255, 10, 255, 255, 255, 255, 255, 20, 255, 255,
        255, 255, 255, 255, 255, 255, 21, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        7, 255, 255, 255, 255, 255, 255, 255, 255, 255, 2, 12},
    // CALTRAIN
    {
        255, 255, 255, 255, 255, 24, 255, 255, 255, 1, 255, 255, 255, 255, 255, 3,
        13, 255, 8, 9, 19, 255, 255, 255, 25, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 11, 12, 255, 255, 23, 255, 16, 7, 255, 255, 21, 255,
        255, 255, 17, 255, 255, 255, 255, 15, 255, 255, 20, 6, 14, 0, 255, 26,
        255, 10, 255, 5, 18, 22, 29, 255, 255, 255, 255, 255},
    // ACE
    {
        3, 5, 8, 15, 23, 24, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 26,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
};

uint8_t getStationIndex(Line line, StationId st)
{
    return (st < NUM_STATIONS)
               ? stationIndexMap[line][st]
               : 255;
}

std::vector<uint8_t> getLineIndices(Line L)
{
    std::vector<uint8_t> out;
    for (uint8_t s = 0; s < NUM_STATIONS; ++s)
    {
        uint8_t idx = stationIndexMap[L][s];
        if (idx < lineLen[L])
            out.push_back(idx);
    }
    return out;
}

//=============================================================================
//                         DISPLAY & LED HELPERS
//=============================================================================

void maxSend(uint8_t reg, uint8_t data)
{
    SPI.beginTransaction(SPISettings(8e6, MSBFIRST, SPI_MODE0));
    digitalWrite(PinConfig::CS, LOW);
    SPI.transfer(reg);
    SPI.transfer(data);
    digitalWrite(PinConfig::CS, HIGH);
    SPI.endTransaction();
}

void showDashes()
{
    for (uint8_t r = 1; r <= 4; ++r)
        maxSend(r, 0x0A);
}

void displayNumber(uint16_t v, bool dot2nd)
{
    uint8_t digits[4] = {
        uint8_t(v % 10),
        uint8_t((v / 10) % 10),
        uint8_t((v / 100) % 10),
        uint8_t((v / 1000) % 10)};
    for (uint8_t reg = 1; reg <= 4; ++reg)
    {
        uint8_t x = digits[4 - reg];
        if (dot2nd && reg == 2)
            x |= 0x80;
        maxSend(reg, x);
    }
}

void clearAllLeds()
{
    for (int L = 0; L < NUM_LINES; ++L)
        for (int i = 0; i < lineLen[L]; ++i)
            lineLeds[L][i] = CRGB::Black;
}

void drawLineBases()
{
    for (int L = 0; L < NUM_LINES; ++L)
    {
        for (auto idx : getLineIndices(Line(L)))
        {
            lineLeds[L][idx] = CRGB(50, 50, 50);
        }
    }
}

void updateTrainsOnStrip()
{
    time_t now = time(nullptr);

    clearAllLeds();
    drawLineBases();

    for (const auto &trip : trips)
    {
        Line L = lineFromString(trip.lineId);
        unsigned long nowMs = millis();
        TrainPosition p = computeTrainPosition(trip, now);
        if (p.valid && p.idx < lineLen[L])
        {
            lineLeds[L][p.idx] = dynamicLineColor(L, p, nowMs);
        }
    }
    FastLED.show();
}

void multiWipe(CRGB *leds[], uint8_t lens[], const CRGB colors[], uint8_t count, uint8_t segmentLen, uint16_t delayMs)
{
    uint8_t maxLen = 0;
    for (uint8_t s = 0; s < count; ++s)
        if (lens[s] > maxLen)
            maxLen = lens[s];

    for (int head = -segmentLen; head < maxLen; ++head)
    {
        for (uint8_t s = 0; s < count; ++s)
            for (uint8_t i = 0; i < lens[s]; ++i)
                leds[s][i] = CRGB::Black;
        for (uint8_t s = 0; s < count; ++s)
            for (uint8_t k = 0; k < segmentLen; ++k)
            {
                int idx = head + k;
                if (idx >= 0 && idx < lens[s])
                    leds[s][idx] = colors[s];
            }
        FastLED.show();
        delay(delayMs);
    }
}

//=============================================================================
//                  FETCH + PARSE REMOTE TRAIN DATA
//=============================================================================

bool fetchAndParse()
{
    HTTPClient http;

    http.setTimeout(10000);
    http.useHTTP10(true);
    http.begin(SUPABASE_FN);

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        DPRINTF("HTTP GET failed: %s (%d)\n",
                http.errorToString(code).c_str(), code);
        http.end();
        return false;
    }

    DynamicJsonDocument doc(16 * 1024);
    auto err = deserializeJson(doc, http.getStream());
    if (err)
    {
        DPRINTF("  JSON parse error: %s\n", err.c_str());
        return false;
    }
    DPRINTF("  JSON parsed, array size=%u\n", doc.size());
    http.end();

    std::map<String, Trip> grouped;
    for (JsonObject el : doc.as<JsonArray>())
    {
        StopRecord rec;
        rec.tripId = el["trip_id"].as<String>();
        rec.lineId = el["line_id"].as<String>();
        rec.station = stationIdFromExternalId(el["station_id"]);
        rec.arrival = el["arrival_time"] ? parseISO(el["arrival_time"]) : 0;
        rec.departure = el["departure_time"] ? parseISO(el["departure_time"]) : 0;
        grouped[rec.tripId].tripId = rec.tripId;
        grouped[rec.tripId].lineId = rec.lineId;
        grouped[rec.tripId].stops.push_back(rec);
    }
    trips.clear();
    for (auto &kv : grouped)
    {
        auto &t = kv.second;
        std::sort(t.stops.begin(), t.stops.end(),
                  [](const StopRecord &a, const StopRecord &b)
                  {
                      time_t ta = a.departure ? a.departure : a.arrival;
                      time_t tb = b.departure ? b.departure : b.arrival;
                      return ta < tb;
                  });
        trips.push_back(std::move(t));
    }
    return true;
}

//=============================================================================
//                   TRAIN POSITION INTERPOLATION
//=============================================================================

TrainPosition computeTrainPosition(const Trip &trip, time_t now)
{
    DPRINT(F("Trip ID: "));
    DPRINTLN(trip.tripId);
    DPRINT(F("Now:    "));
    DPRINTLN(formatTime(now));

    Line line = lineFromString(trip.lineId);
    int maxIndex = lineLen[line] - 1;
    DPRINT(F("Line length: "));
    DPRINTLN(lineLen[line]);

    struct Event
    {
        time_t t;
        uint8_t idx;
        bool isArrival;
        bool isDeparture;
    };
    std::vector<Event> evts;
    evts.reserve(trip.stops.size() * 2);

    for (const auto &s : trip.stops)
    {
        uint8_t idx = getStationIndex(line, s.station);
        if (idx == 255)
            continue;
        if (s.arrival > 0)
            evts.push_back(Event{s.arrival, idx, true, false});
        if (s.departure > 0)
            evts.push_back(Event{s.departure, idx, false, true});
    }

    if (evts.empty())
    {
        DPRINTLN(F("→ no valid events; returning –1"));
        return {false, false, 0, 0, 0, 0, 0};
    }

    std::sort(evts.begin(), evts.end(),
              [](const Event &a, const Event &b)
              {
                  return a.t < b.t;
              });
    evts.erase(std::unique(evts.begin(), evts.end(),
                           [](const Event &a, const Event &b)
                           {
                               return a.t == b.t &&
                                      a.idx == b.idx &&
                                      a.isArrival == b.isArrival &&
                                      a.isDeparture == b.isDeparture;
                           }),
               evts.end());

    time_t firstDepT = 0;
    uint8_t firstDepIdx = 255;
    for (auto &e : evts)
    {
        if (e.isDeparture)
        {
            firstDepT = e.t;
            firstDepIdx = e.idx;
            break;
        }
    }
    if (firstDepT > 0 && now < firstDepT)
    {
        DPRINT(F("Now before first departure ("));
        DPRINT(formatTime(firstDepT));
        DPRINT(F(") → clamp idx="));
        DPRINTLN(firstDepIdx);
        return {true, false, firstDepIdx, firstDepIdx, firstDepIdx, firstDepT, firstDepT};
    }

    for (size_t i = 0; i + 1 < evts.size(); ++i)
    {
        const auto &A = evts[i], &B = evts[i + 1];
        if (now >= A.t && now <= B.t)
        {
            if (A.idx == B.idx && A.isArrival && B.isDeparture)
            {
                DPRINT(F("Dwell at idx="));
                DPRINTLN(A.idx);
                return {true, false, A.idx, A.idx, A.idx, A.t, B.t};
            }
            if (A.idx != B.idx && A.isDeparture && B.isArrival)
            {
                double span = double(B.t - A.t);
                double frac = double(now - A.t) / span;
                int pos = A.idx + int(std::round(frac * (B.idx - A.idx)));
                pos = constrain(pos, 0, maxIndex);

                DPRINT(F("Travel "));
                DPRINT(A.idx);
                DPRINT(F("@"));
                DPRINT(formatTime(A.t));
                DPRINT(F(" → "));
                DPRINT(B.idx);
                DPRINT(F("@"));
                DPRINTLN(formatTime(B.t));
                DPRINT(F(" frac="));
                DPRINTLN(frac, 3);
                DPRINT(F("→ interp idx="));
                DPRINTLN(pos);

                return {true, true,
                        static_cast<uint8_t>(pos),
                        A.idx, B.idx,
                        A.t, B.t};
            }
            DPRINTLN(F("→ gap with no valid travel/dwell → –1"));
            return {false, false, 0, 0, 0, 0, 0};
        }
    }

    const auto &last = evts.back();
    DPRINT(F("Now after last event ("));
    DPRINT(formatTime(last.t));
    DPRINT(F(") → clamp idx="));
    DPRINTLN(last.idx);
    return {true, false,
            last.idx,
            last.idx, last.idx,
            last.t, last.t};
}

//=============================================================================
//                                SETUP / LOOP
//=============================================================================

void initMax7219()
{
    pinMode(PinConfig::CS, OUTPUT);
    digitalWrite(PinConfig::CS, HIGH);
    SPI.begin(PinConfig::CLK, -1, PinConfig::DATA, PinConfig::CS);
    maxSend(0x09, 0xFF);
    maxSend(0x0A, 0x0F);
    maxSend(0x0B, 0x03);
    maxSend(0x0C, 0x01);
    maxSend(0x0F, 0x00);
    showDashes();
}

void initFastLED()
{
    FastLED.addLeds<WS2812B, PinConfig::RED, GRB>(redLine, RED_LEN);
    FastLED.addLeds<WS2812B, PinConfig::ORANGE, GRB>(orangeLine, ORANGE_LEN);
    FastLED.addLeds<WS2812B, PinConfig::YELLOW, GRB>(yellowLine, YELLOW_LEN);
    FastLED.addLeds<WS2812B, PinConfig::BLUE, GRB>(blueLine, BLUE_LEN);
    FastLED.addLeds<WS2812B, PinConfig::GREEN, GRB>(greenLine, GREEN_LEN);
    FastLED.addLeds<WS2812B, PinConfig::CALTRAIN, GRB>(calTrain, CAL_LEN);
    FastLED.addLeds<WS2812B, PinConfig::ACE, GRB>(aceTrain, ACE_LEN);
    FastLED.addLeds<WS2812B, PinConfig::LEGEND, GRB>(legend, LEGEND_LEN);
    FastLED.clear(true);
    FastLED.setBrightness(uint8_t(MAX_BRIGHTNESS * 255));

    legend[0] = CRGB::Red;
    legend[1] = CRGB(204, 85, 0);
    legend[2] = CRGB::Green;
    legend[3] = CRGB::Blue;
    legend[4] = CRGB::Yellow;
    legend[5] = CRGB::Maroon;
    legend[6] = CRGB::Purple;
    FastLED.show();
}

void initWiFi()
{
    WiFi.begin(SSID, PASSWORD);
    unsigned long start = millis();
    DPRINT("WiFi connecting…");
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
    {
        showDashes();
        delay(500);
        DPRINT('.');
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        DPRINTLN("\nWiFi connected. IP=" + WiFi.localIP().toString());
    }
    else
    {
        DPRINTLN("\nWiFi failed to connect in 10s.");
    }
}

void initTime()
{
    configTzTime("PST8PDT,M3.2.0/2,M11.1.0/2", "pool.ntp.org", "time.nist.gov");
    DPRINTLN("Waiting for SNTP sync…");
}

void sweep()
{
    CRGB *strips[] = {redLine, orangeLine, yellowLine, blueLine, greenLine, calTrain, aceTrain};
    uint8_t lens[] = {RED_LEN, ORANGE_LEN, YELLOW_LEN, BLUE_LEN, GREEN_LEN, CAL_LEN, ACE_LEN};
    CRGB colors[] = {CRGB::Red, CRGB(204, 85, 0), CRGB::Yellow, CRGB::Blue, CRGB::Green, CRGB::Maroon, CRGB::Purple};
    const uint8_t WIPE = 3;
    const uint16_t SPD = 100;
    multiWipe(strips, lens, colors, 7, WIPE, SPD);
}

void setup()
{
    Serial.begin(115200);
    delay(100);
    initMax7219();
    initFastLED();
    sweep();
    initWiFi();
    initTime();
    lastFetchMs = millis() - FETCH_INTERVAL_MS;
}

void loop()
{
    unsigned long nowMs = millis();

    // 7-segment time display
    if (nowMs - lastDispMs >= DISPLAY_UPDATE_INTERVAL_MS)
    {
        lastDispMs = nowMs;
        struct tm tm;
        if (getLocalTime(&tm))
        {
            uint16_t hhmm = tm.tm_hour * 100 + tm.tm_min;
            displayNumber(hhmm, (tm.tm_sec % 2) == 0);
        }
    }

    // remote fetch
    if (nowMs - lastFetchMs >= FETCH_INTERVAL_MS)
    {
        lastFetchMs = nowMs;
        bool success = false;

        for (int attempt = 0; attempt <= 3; ++attempt)
        {
            if (attempt > 0)
            {
                delay(5000);
            }
            if (fetchAndParse())
            {
                DPRINTLN("Train data updated");
                success = true;
                break;
            }
            else
            {
                DPRINT("Fetch failed, attempt ");
                DPRINTLN(attempt + 1);
            }
        }

        if (!success)
        {
            DPRINTLN("All fetch attempts failed");
        }
    }

    // LED strip update
    if (nowMs - lastDrawMs >= DISPLAY_UPDATE_INTERVAL_MS)
    {
        lastDrawMs = nowMs;
        updateTrainsOnStrip();
    }
}