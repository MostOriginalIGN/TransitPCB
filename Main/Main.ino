#include <WiFi.h>
#include <time.h>
#include <esp_sntp.h>
#include <SPI.h>
#include <FastLED.h>
#include "WifiCredentials.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <ctype.h>
#include <string.h>

const long gmtOffset_sec = -8 * 3600;
const int daylightOffset_sec = 0;

#define DATA_PIN 11
#define CLK_PIN 13
#define CS_PIN 12
#define SPI_FREQ 8000000UL

#define MAX_BRIGHTNESS 0.05

#define RED_PIN 8
#define ORANGE_PIN 6
#define YELLOW_PIN 9
#define BLUE_PIN 7
#define GREEN_PIN 5
#define CAL_PIN 10
#define ACE_PIN 17
#define LEGEND_PIN 18

#define RED_LEN 30
#define ORANGE_LEN 27
#define YELLOW_LEN 34
#define BLUE_LEN 28
#define GREEN_LEN 34
#define CAL_LEN 30
#define ACE_LEN 37
#define LEGEND_LEN 7

#include <map>
#include <vector>

struct StopRecord;
struct Trip;
extern const uint8_t lineLens[];
extern CRGB *lineLeds[];

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

std::vector<uint8_t> getLineIndices(enum Line line);

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
  NUM_STATIONS
};

StationId stationIdFromExternalId(const std::string &rawId)
{
  std::string id = rawId;
  std::transform(id.begin(), id.end(), id.begin(),
                 [](unsigned char c)
                 { return std::toupper(c); });

  if (id == "73368" || id == "FRMT")
    return StationId::FREMONT;
  if (id == "73422")
    return StationId::GREAT_AMERICA;
  if (id == "73548")
    return StationId::LIVERMORE;
  if (id == "73757")
    return StationId::PLEASANTON;
  if (id == "73752" || id == "70261" || id == "70262")
    return StationId::SAN_JOSE_DIRIDON;
  if (id == "73722" || id == "70241" || id == "70242")
    return StationId::SANTA_CLARA;
  if (id == "73827")
    return StationId::VASCO_RD;

  if (id == "12TH")
    return StationId::ST_12TH_ST_OAKLAND_CITY_CENTER;
  if (id == "16TH")
    return StationId::ST_16TH_ST_MISSION;
  if (id == "19TH")
    return StationId::ST_19TH_ST_OAKLAND;
  if (id == "24TH")
    return StationId::ST_24TH_ST_MISSION;
  if (id == "ANTC")
    return StationId::ANTIOCH;
  if (id == "ASHB")
    return StationId::ASHBY;
  if (id == "BALB")
    return StationId::BALBOA_PARK;
  if (id == "BAYF")
    return StationId::BAY_FAIR;
  if (id == "BERY")
    return StationId::BERRYESSA_NORTH_SAN_JOSE;
  if (id == "CAST")
    return StationId::CASTRO_VALLEY;
  if (id == "CIVC")
    return StationId::CIVIC_CENTER_UN_PLAZA;
  if (id == "COLS")
    return StationId::COLISEUM;
  if (id == "COLM")
    return StationId::COLMA;
  if (id == "CONC")
    return StationId::CONCORD;
  if (id == "DALY")
    return StationId::DALY_CITY;
  if (id == "DBRK")
    return StationId::DOWNTOWN_BERKELEY;
  if (id == "DUBL")
    return StationId::DUBLIN_PLEASANTON;
  if (id == "DELN")
    return StationId::EL_CERRITO_DEL_NORTE;
  if (id == "PLZA")
    return StationId::EL_CERRITO_PLAZA;
  if (id == "EMBR")
    return StationId::EMBARCADERO;
  if (id == "FTVL")
    return StationId::FRUITVALE;
  if (id == "GLEN")
    return StationId::GLEN_PARK;
  if (id == "HAYW")
    return StationId::HAYWARD;
  if (id == "LAFY")
    return StationId::LAFAYETTE;
  if (id == "LAKE")
    return StationId::LAKE_MERRITT;
  if (id == "MCAR")
    return StationId::MACARTHUR;
  if (id == "MLBR")
    return StationId::MILLBRAE;
  if (id == "MLPT")
    return StationId::MILPITAS;
  if (id == "MONT")
    return StationId::MONTGOMERY_ST;
  if (id == "NBRK")
    return StationId::NORTH_BERKELEY;
  if (id == "NCON")
    return StationId::NORTH_CONCORD_MARTINEZ;
  if (id == "ORIN")
    return StationId::ORINDA;
  if (id == "PITT")
    return StationId::PITTSBURG_BAY_POINT;
  if (id == "PCTR")
    return StationId::PITTSBURG_CENTER;
  if (id == "PHIL")
    return StationId::PLEASANT_HILL_CONTRA_COSTA_CENTRE;
  if (id == "POWL")
    return StationId::POWELL_ST;
  if (id == "RICH")
    return StationId::RICHMOND;
  if (id == "ROCK")
    return StationId::ROCKRIDGE;
  if (id == "SBRN")
    return StationId::SAN_BRUNO;
  if (id == "SFIA")
    return StationId::SAN_FRANCISCO_INTERNATIONAL_AIRPORT;
  if (id == "SANL")
    return StationId::SAN_LEANDRO;
  if (id == "SHAY")
    return StationId::SOUTH_HAYWARD;
  if (id == "SSAN")
    return StationId::SOUTH_SAN_FRANCISCO;
  if (id == "UCTY")
    return StationId::UNION_CITY;
  if (id == "WCRK")
    return StationId::WALNUT_CREEK;
  if (id == "WARM")
    return StationId::WARM_SPRINGS_SOUTH_FREMONT;
  if (id == "WDUB")
    return StationId::WEST_DUBLIN_PLEASANTON;

  if (id == "70021" || id == "70022")
    return StationId::ST_22ND_STREET;
  if (id == "70031" || id == "70032")
    return StationId::BAYSHORE;
  if (id == "70121" || id == "70122")
    return StationId::BELMONT;
  if (id == "70251" || id == "70252")
    return StationId::COLLEGE_PARK;
  if (id == "70071" || id == "70072")
    return StationId::BROADWAY;
  if (id == "70081" || id == "70082")
    return StationId::BURLINGAME;
  if (id == "70191" || id == "70192")
    return StationId::CALIFORNIA_AVE;
  if (id == "70101" || id == "70102")
    return StationId::HAYWARD_PARK;
  if (id == "70111" || id == "70112")
    return StationId::HILLSDALE;
  if (id == "70231" || id == "70232")
    return StationId::LAWRENCE;
  if (id == "70161" || id == "70162")
    return StationId::MENLO_PARK;
  if (id == "70061" || id == "70062")
    return StationId::MILLBRAE;
  if (id == "70211" || id == "70212")
    return StationId::MOUNTAIN_VIEW;
  if (id == "70171" || id == "70172")
    return StationId::PALO_ALTO;
  if (id == "70141" || id == "70142")
    return StationId::REDWOOD_CITY;
  if (id == "70201" || id == "70202")
    return StationId::SAN_ANTONIO;
  if (id == "70051" || id == "70052")
    return StationId::SAN_BRUNO;
  if (id == "70131" || id == "70132")
    return StationId::SAN_CARLOS;
  if (id == "70011" || id == "70012")
    return StationId::SAN_FRANCISCO;
  if (id == "70091" || id == "70092")
    return StationId::SAN_MATEO;
  if (id == "70221" || id == "70222")
    return StationId::SUNNYVALE;
  if (id == "70271" || id == "70272")
    return StationId::TAMIEN;

  return StationId::NUM_STATIONS;
}

const uint8_t stationIndexMap[NUM_LINES][NUM_STATIONS] = {
    // RED
    {
        255, 255, 255, 255, 255, 255, 8, 20, 7, 255, 21, 255, 5, 23, 255, 255,
        255, 255, 255, 255, 255, 255, 19, 255, 255, 25, 255, 24, 4, 255, 1, 2,
        16, 255, 22, 255, 255, 255, 255, 255, 255, 6, 255, 29, 255, 17, 255, 3,
        255, 255, 255, 255, 255, 255, 18, 255, 0, 255, 255, 27, 255, 255, 28, 255,
        255, 255, 255, 26, 255, 255, 255, 255, 255, 255, 255},
    // ORANGE
    {
        255, 255, 255, 8, 255, 255, 18, 255, 19, 255, 255, 255, 21, 255, 12, 255,
        255, 0, 255, 255, 255, 255, 255, 14, 255, 255, 255, 255, 22, 255, 25, 24,
        255, 15, 255, 11, 255, 255, 255, 16, 255, 20, 255, 255, 2, 255, 255, 23,
        255, 255, 255, 255, 255, 255, 255, 255, 26, 255, 255, 255, 255, 255, 255, 255,
        13, 255, 10, 255, 255, 255, 255, 9, 255, 4, 255},
    // YELLOW
    {
        255, 255, 255, 255, 255, 255, 13, 25, 12, 255, 26, 0, 255, 28, 255, 255,
        255, 255, 255, 255, 255, 255, 24, 255, 255, 30, 5, 29, 255, 255, 255, 255,
        21, 255, 27, 255, 255, 255, 8, 255, 255, 11, 255, 255, 255, 22, 255, 255,
        4, 9, 255, 2, 1, 6, 23, 255, 255, 10, 255, 32, 255, 255, 33, 255,
        255, 255, 255, 31, 255, 255, 255, 255, 7, 255, 255},
    // GREEN
    {
        255, 255, 255, 8, 255, 255, 255, 29, 255, 255, 30, 255, 255, 32, 12, 255,
        255, 0, 255, 255, 255, 255, 28, 14, 255, 255, 255, 33, 255, 255, 255, 255,
        25, 15, 31, 11, 255, 255, 255, 16, 255, 255, 255, 255, 2, 26, 255, 255,
        255, 255, 255, 255, 255, 255, 27, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        13, 255, 10, 255, 255, 255, 255, 9, 255, 4, 255},
    // BLUE
    {
        255, 255, 255, 255, 255, 255, 255, 23, 255, 255, 24, 255, 255, 26, 6, 255,
        255, 255, 255, 255, 255, 4, 22, 8, 255, 255, 255, 27, 255, 0, 255, 255,
        19, 9, 25, 255, 255, 255, 255, 10, 255, 255, 255, 255, 255, 20, 255, 255,
        255, 255, 255, 255, 255, 255, 21, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        7, 255, 255, 255, 255, 255, 255, 255, 255, 255, 2},
    // CALTRAIN
    {
        255, 255, 255, 255, 255, 24, 255, 255, 255, 1, 255, 255, 255, 255, 255, 3,
        13, 255, 8, 9, 19, 255, 255, 255, 25, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 11, 12, 255, 255, 23, 255, 16, 7, 255, 255, 21, 255,
        255, 255, 17, 255, 255, 255, 255, 15, 255, 255, 20, 6, 14, 0, 255, 26,
        255, 10, 255, 5, 18, 22, 29, 255, 255, 255, 255},
    // ACE
    {
        3, 5, 8, 15, 23, 24, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 26,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
};

struct StopRecord
{
  String trip_id;
  String line_id;
  StationId station;
  time_t arrival;
  time_t departure;
};

struct Trip
{
  String trip_id;
  String line_id;
  std::vector<StopRecord> stops;
};

const char *SUPABASE_FN = "https://icalcigcgkzaxyvhnfgc.supabase.co/functions/v1/getStatus";
const unsigned long FETCH_INTERVAL_MS = 5 * 60 * 1000UL;
unsigned long lastFetch = 0;
std::vector<Trip> trips;

bool fetchAndParse()
{
  HTTPClient http;
  http.begin(SUPABASE_FN);
  int code = http.GET();
  if (code != HTTP_CODE_OK)
  {
    Serial.printf("HTTP error %d\n", code);
    http.end();
    return false;
  }

  DynamicJsonDocument doc(16 * 1024);
  auto payload = http.getString();
  http.end();

  DeserializationError err = deserializeJson(doc, payload);
  if (err)
  {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    return false;
  }

  std::map<String, Trip> grouped;
  JsonArray arr = doc.as<JsonArray>();
  for (JsonVariant el : arr)
  {
    StopRecord r;
    r.trip_id = el["trip_id"].as<String>();
    r.line_id = el["line_id"].as<String>();
    r.station = stationIdFromExternalId(el["station_id"].as<const char *>());
    r.arrival = el["arrival_time"] ? parseISO(el["arrival_time"].as<const char *>()) : 0;
    r.departure = el["departure_time"] ? parseISO(el["departure_time"].as<const char *>()) : 0;

    auto &t = grouped[r.trip_id];
    t.trip_id = r.trip_id;
    t.line_id = r.line_id;
    t.stops.push_back(r);
  }

  trips.clear();
  for (auto &p : grouped)
  {
    Trip &t = p.second;
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

time_t parseISO(const char *s)
{
  struct tm tm = {};
  strptime(s, "%Y-%m-%dT%H:%M:%S", &tm);
  tm.tm_isdst = -1;
  return mktime(&tm);
}

Line lineFromString(const String &s)
{
  if (s == "RED")
    return RED;
  if (s == "ORANGE")
    return ORANGE;
  if (s == "YELLOW")
    return YELLOW;
  if (s == "GREEN")
    return GREEN;
  if (s == "BLUE")
    return BLUE;
  if (s == "CALTRAIN")
    return CALTRAIN;
  if (s == "ACE")
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
    return CRGB::Orange;
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

int computeTrainIndex(const Trip &t, time_t now)
{
  StopRecord const *prev = nullptr;
  StopRecord const *next = nullptr;

  for (auto &s : t.stops)
  {
    if (s.departure && s.departure <= now)
    {
      if (!prev || s.departure > prev->departure)
        prev = &s;
    }
    if (s.arrival && s.arrival >= now)
    {
      if (!next || s.arrival < next->arrival)
        next = &s;
    }
  }
  if (!prev)
  {
    return getStationIndex(lineFromString(t.line_id), t.stops.front().station);
  }
  if (!next)
  {
    return getStationIndex(lineFromString(t.line_id), prev->station);
  }

  uint8_t idx1 = getStationIndex(lineFromString(t.line_id), prev->station);
  uint8_t idx2 = getStationIndex(lineFromString(t.line_id), next->station);
  long dtTotal = next->arrival - prev->departure;
  long dtNow = now - prev->departure;
  float frac = constrain(float(dtNow) / float(dtTotal), 0, 1);
  return idx1 + round(frac * (idx2 - idx1));
}

void updateTrains()
{
  time_t now = time(nullptr);

  for (int L = 0; L < NUM_LINES; ++L)
    for (int i = 0; i < lineLens[L]; ++i)
      lineLeds[L][i] = CRGB::Black;

  for (int L = 0; L < NUM_LINES; ++L)
  {
    auto idxs = getLineIndices((Line)L);
    for (auto idx : idxs)
      lineLeds[L][idx] = CRGB::White;
  }

  for (auto &t : trips)
  {
    Line L = lineFromString(t.line_id);
    int idx = computeTrainIndex(t, now);
    if (idx >= 0 && idx < lineLens[L])
      lineLeds[L][idx] = colorForLine(L);
  }

  FastLED.show();
}

unsigned long lastMillis = 0;
const unsigned long interval = 1000;

CRGB redLine[RED_LEN];
CRGB orangeLine[ORANGE_LEN];
CRGB yellowLine[YELLOW_LEN];
CRGB blueLine[BLUE_LEN];
CRGB greenLine[GREEN_LEN];
CRGB calTrain[CAL_LEN];
CRGB ace[ACE_LEN];
CRGB legend[LEGEND_LEN];

CRGB *lineLeds[NUM_LINES] = {
    redLine, orangeLine, yellowLine,
    greenLine, blueLine, calTrain, ace};

const uint8_t lineLens[NUM_LINES] = {
    RED_LEN, ORANGE_LEN, YELLOW_LEN,
    GREEN_LEN, BLUE_LEN, CAL_LEN, ACE_LEN};

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

void maxSend(uint8_t reg, uint8_t data)
{
  SPI.beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE0));
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(reg);
  SPI.transfer(data);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}

void showDashes()
{
  for (uint8_t r = 1; r <= 4; ++r)
    maxSend(r, 0x0A);
}

void displayNumber(uint16_t v, bool dot2nd)
{
  uint8_t d0 = v % 10;
  uint8_t d1 = (v / 10) % 10;
  uint8_t d2 = (v / 100) % 10;
  uint8_t d3 = (v / 1000) % 10;
  uint8_t arr[4] = {d0, d1, d2, d3};
  for (uint8_t reg = 1; reg <= 4; ++reg)
  {
    uint8_t x = arr[4 - reg];
    if (dot2nd && reg == 2)
      x |= 0x80;
    maxSend(reg, x);
  }
}

uint8_t getStationIndex(Line line, StationId st)
{
  return (st < NUM_STATIONS) ? stationIndexMap[line][st] : 255;
}

void setStationWhite(Line line, StationId st)
{
  uint8_t idx = getStationIndex(line, st);
  if (idx < lineLens[line])
  {
    lineLeds[line][idx] = CRGB::White;
    FastLED.show();
  }
}

void whiteOutAllLines()
{
  for (uint8_t line = 0; line < NUM_LINES; ++line)
  {
    for (uint8_t st = 0; st < NUM_STATIONS; ++st)
    {
      uint8_t idx = stationIndexMap[line][st];
      if (idx != 255 && idx < lineLens[line])
      {
        lineLeds[line][idx] = CRGB::White;
        fadeToBlackBy(&lineLeds[line][idx], 1, 200);
      }
    }
  }
}

void setLineWhite(Line line)
{
  for (uint8_t i = 0; i < lineLens[line]; ++i)
    lineLeds[line][i] = CRGB::White;
  FastLED.show();
}

std::vector<StationId> getLineStations(Line line)
{
  std::vector<StationId> out;
  for (uint8_t s = 0; s < NUM_STATIONS; ++s)
    if (stationIndexMap[line][s] < lineLens[line])
      out.push_back((StationId)s);
  return out;
}

std::vector<uint8_t> getLineIndices(Line line)
{
  std::vector<uint8_t> out;
  for (uint8_t s = 0; s < NUM_STATIONS; ++s)
  {
    uint8_t idx = stationIndexMap[line][s];
    if (idx < lineLens[line])
      out.push_back(idx);
  }
  return out;
}

void setup()
{
  Serial.begin(115200);
  delay(100);

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  SPI.begin(CLK_PIN, -1, DATA_PIN, CS_PIN);

  maxSend(0x09, 0xFF);
  maxSend(0x0A, 0x0F);
  maxSend(0x0B, 0x03);
  maxSend(0x0C, 0x01);
  maxSend(0x0F, 0x00);

  showDashes();

  FastLED.addLeds<WS2812B, RED_PIN, GRB>(redLine, RED_LEN);
  FastLED.addLeds<WS2812B, ORANGE_PIN, GRB>(orangeLine, ORANGE_LEN);
  FastLED.addLeds<WS2812B, YELLOW_PIN, GRB>(yellowLine, YELLOW_LEN);
  FastLED.addLeds<WS2812B, BLUE_PIN, GRB>(blueLine, BLUE_LEN);
  FastLED.addLeds<WS2812B, GREEN_PIN, GRB>(greenLine, GREEN_LEN);
  FastLED.addLeds<WS2812B, CAL_PIN, GRB>(calTrain, CAL_LEN);
  FastLED.addLeds<WS2812B, ACE_PIN, GRB>(ace, ACE_LEN);
  FastLED.addLeds<WS2812B, LEGEND_PIN, GRB>(legend, LEGEND_LEN);

  FastLED.clear(true);
  FastLED.setBrightness(uint8_t(MAX_BRIGHTNESS * 255));

  legend[0] = CRGB::Red;
  legend[1] = CRGB::Orange;
  legend[2] = CRGB::Yellow;
  legend[3] = CRGB::Blue;
  legend[4] = CRGB::Green;
  legend[5] = CRGB::Maroon;
  legend[6] = CRGB::Purple;
  FastLED.show();

  {
    CRGB *strips[] = {redLine, orangeLine, yellowLine, blueLine, greenLine, calTrain, ace};
    uint8_t lens[] = {RED_LEN, ORANGE_LEN, YELLOW_LEN, BLUE_LEN, GREEN_LEN, CAL_LEN, ACE_LEN};
    CRGB colors[] = {CRGB::Red, CRGB::Orange, CRGB::Yellow, CRGB::Blue, CRGB::Green, CRGB::Maroon, CRGB::Purple};
    const uint8_t WIPE = 3;
    const uint16_t SPD = 100;
    multiWipe(strips, lens, colors, 7, WIPE, SPD);
  }

  WiFi.begin(SSID, PASSWORD);
  Serial.print("Wi‑Fi connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    showDashes();
    delay(500);
    Serial.print('.');
  }
  Serial.println("\nWi‑Fi connected!");

  configTzTime(
      "PST8PDT,M3.2.0/2,M11.1.0/2",
      "pool.ntp.org", "time.nist.gov");
}

void loop()
{
  unsigned long now = millis();

  if (now - lastMillis >= interval)
  {
    lastMillis += interval;

    struct tm tm;
    if (getLocalTime(&tm))
    {
      uint16_t hhmm = tm.tm_hour * 100 + tm.tm_min;
      bool dotOn = (tm.tm_sec % 2) == 0;
      displayNumber(hhmm, dotOn);
    }
  }

  if (now - lastFetch >= FETCH_INTERVAL_MS)
  {
    lastFetch = now;
    if (!fetchAndParse())
    {
      Serial.println("Failed to update train data");
    }
    else
    {
      Serial.println("Train data updated");
    }
  }

  static unsigned long lastDraw = 0;
  if (now - lastDraw >= 1000)
  {
    lastDraw = now;
    updateTrains();
  }
}