#pragma once
// ============================================================================
//  Internet app: Weather + PM2.5 (Khon Kaen) and short news (RSS).
//  News has 3 groups: Tech (Thai), AI (Thai), Robots (mostly English: no Thai
//  site has a robot news feed).
//  Needs the board to be on a Wi-Fi that has internet
//  (home Wi-Fi or phone hotspot, set on the web page -> Settings).
//  Included from SomudTick.ino.
// ============================================================================
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#define WX_LAT "16.43"      // Khon Kaen
#define WX_LON "102.83"
#define WX_PLACE "Khon Kaen"
// news groups. A feed can be filtered: only stories whose title is about AI (or robots) are kept
enum NewsFilter : uint8_t { NF_ALL, NF_AI, NF_ROBOT };
struct NewsFeed { uint8_t cat; const char* url; const char* name; NewsFilter filter; };
const NewsFeed NEWS_FEEDS[] = {
  {0, "https://www.blognone.com/atom.xml", "Blognone", NF_ALL},
  {1, "https://www.techtalkthai.com/feed/", "TechTalkThai", NF_AI},
  {1, "https://www.blognone.com/atom.xml", "Blognone", NF_AI},
  {2, "https://www.therobotreport.com/feed/", "Robot Report", NF_ALL},
  {2, "https://www.blognone.com/atom.xml", "Blognone", NF_ROBOT},
};
const int N_FEEDS = sizeof(NEWS_FEEDS) / sizeof(NEWS_FEEDS[0]);
const int N_NEWSCAT = 3;
const char* NEWSCAT_N[N_NEWSCAT] = {"Tech", "AI", "Robots"};
const uint32_t NEWSCAT_COL[N_NEWSCAT] = {0x5C6670, 0x7A5CC8, 0xD9822B};   // thin line on the left of each story
#define OIL_URL "https://oil-price.bangchak.co.th/ApiOilPrice2/th"
struct Oil { String name; float today, tomorrow, dif; };
std::vector<Oil> oil;
String oilWhen;
const uint32_t NET_REFRESH_MS = 20UL * 60 * 1000;   // new data every 20 min (when the page is open)

int netTab = 0;          // 0 weather, 1 news
int netScroll = 0, netMax = 0;
int newsOpen = -1;       // -1 = list, else the story that is open
String netMsg;

struct Wx {
  bool ok = false; uint32_t at = 0;
  float temp, feels, rain, wind; int hum, code;
  int dCode[3]; float dMax[3], dMin[3]; int dRain[3];
  bool aqOk = false; float pm25, pm10; int aqi;
} wx;
struct News { String title, desc, src; uint32_t t; };
std::vector<News> newsC[N_NEWSCAT];   // stories of each group
uint32_t newsAtC[N_NEWSCAT] = {0, 0, 0};
int newsCat = 0;                       // the group on screen
std::vector<News>& curNews() { return newsC[newsCat]; }

// ---------------- download ----------------
// Reads the whole reply into PSRAM. Returns length (0 = failed). Caller frees.
size_t httpGet(const String& url, char** out, size_t maxLen) {
  *out = nullptr;
  NetworkClientSecure cli;
  cli.setInsecure();   // public data only, no login -> skip certificate check
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(8000);
  http.useHTTP10(true);   // plain reply (no "chunked" pieces), easier to read
  http.setUserAgent("SomudTick/3 (ESP32)");
  if (!http.begin(cli, url)) return 0;
  int code = http.GET();
  if (code != 200) { Serial.printf("GET %s -> %d\n", url.c_str(), code); http.end(); return 0; }
  char* buf = (char*)heap_caps_malloc(maxLen + 1, MALLOC_CAP_SPIRAM);
  if (!buf) { http.end(); return 0; }
  WiFiClient* s = http.getStreamPtr();
  size_t n = 0; uint32_t t = millis();
  int total = http.getSize();   // -1 if unknown
  while (http.connected() && n < maxLen && millis() - t < 12000) {
    size_t av = s->available();
    if (av) { n += s->readBytes(buf + n, min(av, maxLen - n)); t = millis(); }
    else if (total > 0 && (int)n >= total) break;
    else delay(2);
  }
  while (s->available() && n < maxLen) n += s->readBytes(buf + n, min((size_t)s->available(), maxLen - n));
  buf[n] = 0;
  http.end();
  *out = buf;
  return n;
}

// ---------------- weather ----------------
const char* wxWord(int c) {
  if (c == 0) return "Clear sky";
  if (c == 1) return "Mostly clear";
  if (c == 2) return "Some clouds";
  if (c == 3) return "Cloudy";
  if (c == 45 || c == 48) return "Fog";
  if (c >= 51 && c <= 57) return "Light rain";
  if (c >= 61 && c <= 67) return "Rain";
  if (c >= 71 && c <= 77) return "Snow";
  if (c >= 80 && c <= 82) return "Rain showers";
  if (c >= 95) return "Thunderstorm";
  return "-";
}
bool wxFetch(Wx& w) {   // fills w (a copy); the screen keeps showing the old one until it is done
  char* b; size_t n;
  n = httpGet("https://api.open-meteo.com/v1/forecast?latitude=" WX_LAT "&longitude=" WX_LON
              "&current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,weather_code,wind_speed_10m"
              "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
              "&timezone=Asia%2FBangkok&forecast_days=3", &b, 16000);
  if (!n) return false;
  JsonDocument d;
  bool ok = !deserializeJson(d, b, n);
  free(b);
  if (!ok || !d["current"].is<JsonObject>()) return false;
  JsonObject c = d["current"];
  w.temp = c["temperature_2m"] | 0.0f; w.feels = c["apparent_temperature"] | 0.0f;
  w.hum = c["relative_humidity_2m"] | 0; w.rain = c["precipitation"] | 0.0f;
  w.code = c["weather_code"] | 0; w.wind = c["wind_speed_10m"] | 0.0f;
  for (int i = 0; i < 3; i++) {
    w.dCode[i] = d["daily"]["weather_code"][i] | 0;
    w.dMax[i] = d["daily"]["temperature_2m_max"][i] | 0.0f;
    w.dMin[i] = d["daily"]["temperature_2m_min"][i] | 0.0f;
    w.dRain[i] = d["daily"]["precipitation_probability_max"][i] | 0;
  }
  w.ok = true; w.at = millis();
  // air quality (separate server)
  n = httpGet("https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" WX_LAT "&longitude=" WX_LON
              "&current=pm2_5,pm10,us_aqi&timezone=Asia%2FBangkok", &b, 4000);
  if (n) {
    JsonDocument a;
    if (!deserializeJson(a, b, n) && a["current"].is<JsonObject>()) {
      w.pm25 = a["current"]["pm2_5"] | 0.0f; w.pm10 = a["current"]["pm10"] | 0.0f; w.aqi = a["current"]["us_aqi"] | 0;
      w.aqOk = true;
    }
    free(b);
  }
  return true;
}
// Thai PM2.5 levels (Pollution Control Department, ug/m3)
void pmLevel(float v, const char*& word, uint32_t& col, const char*& tip) {
  if (v <= 15) { word = "Very good"; col = 0x3BA7E0; tip = "Great for outside."; }
  else if (v <= 25) { word = "Good"; col = 0x4CAF50; tip = "OK to go out."; }
  else if (v <= 37.5f) { word = "OK"; col = 0xE0B000; tip = "Sensitive? Take care."; }
  else if (v <= 75) { word = "Unhealthy"; col = 0xF08020; tip = "Wear a mask."; }
  else { word = "Very unhealthy"; col = 0xD03030; tip = "Stay inside."; }
}
void drawWxIcon(int code, int cx, int cy, int s) {   // s = size
  uint32_t sun = 0xF2B705, cloud = themeDark ? 0xBFC6CE : 0x8A96A3, rain = 0x3B82F6;
  bool hasSun = code <= 2, hasCloud = code >= 1, hasRain = (code >= 51 && code <= 67) || (code >= 80 && code <= 82) || code >= 95;
  if (hasSun) {
    int sx = hasCloud ? cx - s / 4 : cx, sy = hasCloud ? cy - s / 4 : cy;
    spr.fillCircle(sx, sy, s / 3, C(sun));
    for (int k = 0; k < 8; k++) { float a = k * PI / 4; spr.drawLine(sx + cosf(a) * s * 0.42f, sy + sinf(a) * s * 0.42f, sx + cosf(a) * s * 0.55f, sy + sinf(a) * s * 0.55f, C(sun)); }
  }
  if (hasCloud && code != 1) {
    spr.fillCircle(cx - s / 5, cy + s / 10, s / 4, C(cloud));
    spr.fillCircle(cx + s / 8, cy - s / 12, s / 3, C(cloud));
    spr.fillRoundRect(cx - s / 2 + 2, cy + s / 10, s - 4, s / 4, s / 8, C(cloud));
  }
  if (hasRain) for (int k = 0; k < 3; k++) wideLine(cx - s / 4 + k * s / 4, cy + s * 0.42f, cx - s / 4 + k * s / 4 - 3, cy + s * 0.62f, 2, C(rain));
  if (code >= 95) spr.fillTriangle(cx, cy + s * 0.3f, cx + 6, cy + s * 0.3f, cx - 2, cy + s * 0.65f, C(sun));
}

// ---------------- news ----------------
// keep only characters the fonts can draw; turn HTML into plain text
String cleanText(const char* p, const char* end) {
  String o; o.reserve(end - p);
  bool tag = false;
  while (p < end) {
    if (!strncmp(p, "<![CDATA[", 9)) { p += 9; continue; }
    if (!strncmp(p, "]]>", 3)) { p += 3; continue; }
    char ch = *p;
    if (ch == '<') { tag = true; p++; continue; }
    if (ch == '>') { tag = false; o += ' '; p++; continue; }
    if (ch == '&') {   // entities
      const char* semi = (const char*)memchr(p, ';', min((int)(end - p), 10));
      if (semi) {
        char eb[12] = {0}; memcpy(eb, p + 1, min((int)(semi - p - 1), 11)); String e(eb);
        if (tag && e != "gt") { p = semi + 1; continue; }   // inside a tag: ignore
        if (e == "amp") o += '&'; else if (e == "quot") o += '"';
        else if (e == "lt") tag = true;                   // escaped HTML tag (Blognone) -> skip it
        else if (e == "gt") { tag = false; o += ' '; }
        else if (e == "apos" || e == "#039" || e == "#8217" || e == "#8216") o += '\'';
        else if (e == "#8220" || e == "#8221") o += '"';
        else if (e == "#8211" || e == "#8212") o += '-';
        else if (e == "#8230" || e == "hellip") o += "...";
        else if (e == "nbsp" || e == "#160") o += ' ';
        p = semi + 1; continue;
      }
    }
    if (tag) { p++; continue; }
    uint8_t u = (uint8_t)ch;
    if (u < 0x80) { o += (u < 0x20) ? ' ' : ch; p++; continue; }
    int len = (u >= 0xF0) ? 4 : (u >= 0xE0) ? 3 : 2;
    if (p + len > end) break;
    uint32_t cp = len == 2 ? ((u & 0x1F) << 6) | (p[1] & 0x3F)
                : len == 3 ? ((u & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F) : 0;
    if (cp >= 0x0E01 && cp <= 0x0E5B) for (int k = 0; k < len; k++) o += p[k];   // Thai letters
    else if (cp == 0x201C || cp == 0x201D) o += '"';
    else if (cp == 0x2018 || cp == 0x2019) o += '\'';
    else if (cp == 0x2013 || cp == 0x2014) o += '-';
    else if (cp == 0x2026) o += "...";
    else if (cp == 0xA0) o += ' ';
    p += len;
  }
  // squeeze spaces
  String r; r.reserve(o.length()); bool sp = false;
  for (size_t i = 0; i < o.length(); i++) { char c = o[i]; if (c == ' ') { if (!sp && r.length()) r += ' '; sp = true; } else { r += c; sp = false; } }
  r.trim();
  return r;
}
const char* findTag(const char* p, const char* end, const char* tag, const char** innerEnd) {
  String open = String("<") + tag, close = String("</") + tag + ">";
  const char* a = strstr(p, open.c_str());
  if (!a || a >= end) return nullptr;
  a = strchr(a, '>'); if (!a || a >= end) return nullptr; a++;
  const char* b = strstr(a, close.c_str());
  if (!b || b > end) return nullptr;
  *innerEnd = b;
  return a;
}
bool oilFetch(std::vector<Oil>& oil, String& oilWhen) {   // fills the given list
  char* b; size_t n = httpGet(OIL_URL, &b, 60000);
  if (!n) return false;
  JsonDocument d;
  bool ok = !deserializeJson(d, b, n);
  free(b);
  if (!ok) return false;
  JsonObject o = d[0];
  const char* list = o["OilList"] | "";
  JsonDocument l;
  if (deserializeJson(l, list)) return false;
  oil.clear();
  for (JsonObject x : l.as<JsonArray>()) {
    Oil v; v.name = String((const char*)(x["OilName"] | "")); v.name.trim();
    v.today = x["PriceToday"] | 0.0f; v.tomorrow = x["PriceTomorrow"] | 0.0f; v.dif = x["PriceDifTomorrow"] | 0.0f;
    if (v.name.length() && v.today > 0) oil.push_back(v);
  }
  oilWhen = String((const char*)(o["OilRemark2"] | ""));
  return !oil.empty();
}
// is this title about AI / robots? ("AI" must be a word on its own, so "AIS" or "MAIN" do not count)
bool hasWord(const String& t, const char* w) {
  int n = strlen(w);
  for (int i = t.indexOf(w); i >= 0; i = t.indexOf(w, i + 1)) {
    bool a = i == 0 || !isalnum((unsigned char)t[i - 1]), b = i + n >= (int)t.length() || !isalnum((unsigned char)t[i + n]);
    if (a && b) return true;
  }
  return false;
}
bool newsMatch(const String& title, NewsFilter f) {
  if (f == NF_ALL) return true;
  String low = title; low.toLowerCase();
  if (f == NF_AI) {
    if (hasWord(title, "AI") || hasWord(title, "LLM") || hasWord(title, "GPT")) return true;
    const char* K[] = {"ปัญญาประดิษฐ์", "openai", "chatgpt", "gemini", "claude", "anthropic", "copilot", "deepseek", "llama", "nvidia", "machine learning"};
    for (auto k : K) if (low.indexOf(k) >= 0) return true;
    return false;
  }
  const char* K[] = {"หุ่นยนต์", "robot", "humanoid", "optimus", "boston dynamics", "unitree", "โดรน", "drone"};
  for (auto k : K) if (low.indexOf(k) >= 0) return true;
  return false;
}
// summary text: many feeds start it with the title again (Blognone even adds "Body"),
// and WordPress sites end it with "The post ... appeared first on ..."
String newsTidy(String d, const String& title) {
  d.trim();
  if (title.length() && d.startsWith(title)) { d = d.substring(title.length()); d.trim(); }
  if (d.startsWith("Body ")) d = d.substring(5);
  int cut = d.indexOf("The post ");
  if (cut >= 0 && d.indexOf("appeared first on", cut) > cut) d = d.substring(0, cut);
  d.trim();
  return d;
}
bool newsFetch(int cat, std::vector<News>& out) {   // fills out
  std::vector<News> got;
  for (int f = 0; f < N_FEEDS; f++) {
    const NewsFeed& F = NEWS_FEEDS[f];
    if (F.cat != cat) continue;
    char* b; size_t n = httpGet(F.url, &b, 400000);
    if (!n) continue;
    const char* p = b;
    for (int k = 0; k < 40; k++) {
      const char* it = strstr(p, "<item"); if (!it) break;
      const char* itEnd = strstr(it, "</item>"); if (!itEnd) break;
      p = itEnd + 7;
      News x; x.src = F.name; x.t = 0;
      const char* e;
      const char* t = findTag(it, itEnd, "title", &e); if (t) x.title = cleanText(t, e);
      if (!x.title.length() || !newsMatch(x.title, F.filter)) continue;
      const char* d = findTag(it, itEnd, "description", &e);
      if (d) { x.desc = newsTidy(cleanText(d, e), x.title); if (x.desc.length() > 700) x.desc = x.desc.substring(0, 700); }
      const char* pd = findTag(it, itEnd, "pubDate", &e);
      if (pd) { struct tm tm = {}; char mon[4] = {0}; int dd, yy, hh, mi, ss;
        if (sscanf(pd, "%*3s, %d %3s %d %d:%d:%d", &dd, mon, &yy, &hh, &mi, &ss) == 6) {
          const char* M = "JanFebMarAprMayJunJulAugSepOctNovDec"; const char* q = strstr(M, mon);
          tm.tm_mday = dd; tm.tm_mon = q ? (q - M) / 3 : 0; tm.tm_year = yy - 1900; tm.tm_hour = hh; tm.tm_min = mi; tm.tm_sec = ss;
          int off = 0; const char* z = pd; while (z < e && *z != '+' && *z != '-') z++; if (z < e) { int v = atoi(z + 1); off = (v / 100) * 3600 + (v % 100) * 60; if (*z == '-') off = -off; }
          x.t = (uint32_t)(mktime(&tm) + 7 * 3600 - off);   // mktime uses Thai time (UTC+7)
        } }
      bool dup = false; for (auto& o : got) if (o.title == x.title) dup = true;   // the same story from 2 feeds
      if (!dup) got.push_back(x);
    }
    free(b);
  }
  if (got.empty()) return false;
  std::sort(got.begin(), got.end(), [](const News& a, const News& b) { return a.t > b.t; });
  if (got.size() > 30) got.resize(30);
  out = got;
  return true;
}
// when a story came out: "Today 15:30", "Yesterday 23:56" or "22 Sep"
String newsWhen(uint32_t t) {
  if (!t) return "";
  if (timeApprox) return hhmm(t);
  time_t n = nowT(), tt = t; struct tm a, b; localtime_r(&n, &a); localtime_r(&tt, &b);
  if (dayKey(tt) == dayKey(n)) return "Today " + hhmm(t);
  if (dayKey(tt) == dayKey(n - 86400)) return "Yesterday " + hhmm(t);
  return String(b.tm_mday) + " " + EN_MON[b.tm_mon];
}

// ---------------- text wrap (works for Thai, which has no spaces between words) ----------------
bool thaiMark(uint32_t cp) { return cp == 0x0E31 || (cp >= 0x0E34 && cp <= 0x0E3A) || (cp >= 0x0E47 && cp <= 0x0E4E); }
std::vector<String> wrapText(const lgfx::IFont* f, const String& s, int maxW, int maxLines) {
  std::vector<String> lines;
  spr.setFont(pickFont(f, s));
  size_t i = 0, start = 0, lastSpace = 0;
  while (i < s.length() && (int)lines.size() < maxLines) {
    uint8_t u = s[i]; int len = u < 0x80 ? 1 : u >= 0xE0 ? 3 : 2;
    uint32_t cp = len == 3 ? ((u & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F) : u;
    if (s[i] == ' ') lastSpace = i;
    size_t next = i + len;
    if (!thaiMark(cp) && i > start && spr.textWidth(s.substring(start, next)) > maxW) {
      size_t cut = (lastSpace > start) ? lastSpace : i;
      lines.push_back(s.substring(start, cut));
      start = (lastSpace > start) ? lastSpace + 1 : i;
      lastSpace = 0;
      continue;
    }
    i = next;
  }
  if ((int)lines.size() < maxLines && start < s.length()) lines.push_back(s.substring(start));
  else if ((int)lines.size() == maxLines && start < s.length()) lines.back() += "..";
  return lines;
}

// ---------------- screens ----------------
bool netOnline() { return staOk(); }
// ---- downloads run in the background (they take up to ~20 s): the screen, touch and web page keep working ----
// The job fills copies (jobWx, jobNews, jobOil). loop() -> netPoll() puts them on screen when the job is done.
struct NetJob { int tab, cat; };
volatile bool netBusy = false, netDone = false;
NetJob netJob, netNext; bool netHasNext = false;
Wx jobWx; std::vector<News> jobNews; std::vector<Oil> jobOil; String jobOilWhen, jobMsg; bool jobOk = false;
uint32_t netBusyT = 0;
void netTaskFn(void*) {
  jobMsg = ""; jobOk = false;
  if (netJob.tab == 0) {
    jobWx = wx;
    jobOk = wxFetch(jobWx);
    if (!jobOk) jobMsg = "Could not get the weather. Tap Reload to try again.";
  } else {
    jobOil.clear(); jobNews.clear();
    if (netJob.cat == 0) oilFetch(jobOil, jobOilWhen);   // oil price box (Tech only) is optional
    jobOk = newsFetch(netJob.cat, jobNews);
    if (!jobOk) jobMsg = "Could not get the news. Tap Reload to try again.";
  }
  netBusy = false;   // first "not busy", then "done": netPoll() may start the next download at once
  netDone = true;
  vTaskDelete(NULL);
}
void netStart(int tab, int cat) {
  if (netBusy) { netNext = {tab, cat}; netHasNext = true; return; }   // one download at a time: do this one next
  netJob = {tab, cat}; netBusy = true; netDone = false; netBusyT = millis();
  xTaskCreatePinnedToCore(netTaskFn, "net", 16384, nullptr, 1, nullptr, 0);
}
void netPoll() {   // from loop(): a download finished -> show it
  if (!netDone) { if (netHasNext && !netBusy) { netHasNext = false; netStart(netNext.tab, netNext.cat); } return; }
  netDone = false;
  if (netJob.tab == 0) { if (jobOk) wx = jobWx; }
  else {
    if (jobOk) { newsC[netJob.cat] = jobNews; newsAtC[netJob.cat] = millis(); }
    if (netJob.cat == 0 && !jobOil.empty()) { oil = jobOil; oilWhen = jobOilWhen; }
  }
  if (netTab == netJob.tab && (netTab == 0 || newsCat == netJob.cat)) netMsg = jobMsg;   // message only for what you look at
  if (scr == S_NET) { lastTouchMs = millis(); dirty = true; }
  if (netHasNext) { netHasNext = false; netStart(netNext.tab, netNext.cat); }
}
void netLoad(bool force) {
  if (!netOnline()) { netMsg = staOn ? "No internet. Go to Settings > Wi-Fi > Choose Wi-Fi, and join your home Wi-Fi or phone hotspot." : "Internet Wi-Fi is OFF. Turn it ON in Settings (Internet: join a Wi-Fi)."; return; }
  netMsg = "";
  if (netTab == 0 && (force || !wx.ok || millis() - wx.at > NET_REFRESH_MS)) netStart(0, 0);
  if (netTab == 1 && (force || curNews().empty() || millis() - newsAtC[newsCat] > NET_REFRESH_MS)) netStart(1, newsCat);
  dirty = true;
}
bool netLoadingNow() { return netBusy && netJob.tab == netTab && (netTab == 0 || netJob.cat == newsCat); }
void netOpen() { scr = S_NET; newsOpen = -1; netScroll = 0; dirty = true; render(); netLoad(false); }

String agoMs(uint32_t at) { uint32_t m = (millis() - at) / 60000; return m == 0 ? String("just now") : String(m) + "m ago"; }
// wide screen: Back + tabs share one row, so more room is left for the content
bool netWide() { return W > H; }
int netTop() { return netWide() ? HDR_H + 38 : HDR_H + 70; }
int netTabY() { return netWide() ? HDR_H + 4 : HDR_H + 36; }
int netTabX() { return netWide() ? 78 : 8; }
int netTabW() { return (W - 72 - 4 - netTabX()) / 2; }
void drawNetTabs() {
  drawAppTitle(netWide() ? "" : (newsOpen >= 0 ? "News" : "Internet"));
  int y = netTabY(), w = netTabW(), x0 = netTabX();
  const char* n[] = {"Weather", "News"};
  for (int i = 0; i < 2; i++) btn(x0 + i * (w + 4), y, w, 28, n[i], netTab == i);
  btn(W - 72, y, 64, 28, "Reload", false);
}
void drawWeather(int top) {
  if (!wx.ok) return;
  int y = top - netScroll;
  spr.fillRoundRect(6, y, W - 12, 104, 12, C(CARD));
  drawWxIcon(wx.code, 44, y + 46, 56);
  txt(FS, WX_PLACE, 84, y + 5, SOFT);
  txt(FXL, String((int)roundf(wx.temp)), 84, y + 44, INK, D_ML);
  spr.setFont(FXL); int tw = spr.textWidth(String((int)roundf(wx.temp)));
  spr.drawCircle(90 + tw, y + 27, 3, C(INK)); spr.drawCircle(90 + tw, y + 27, 2, C(INK));   // degree sign
  txt(FB, "C", 96 + tw, y + 32, INK, D_ML);
  txt(FB, fitText(FB, wxWord(wx.code), W - 98), 84, y + 66, INK);
  {   // "Feels 28°C", same style as the big number
    String fl = "Feels " + String((int)roundf(wx.feels));
    txt(FS, fl, 84, y + 83, SOFT);
    spr.setFont(FS); int fw = spr.textWidth(fl);
    spr.drawCircle(84 + fw + 4, y + 87, 2, C(SOFT));
    txt(FS, "C", 84 + fw + 8, y + 83, SOFT);
  }
  y += 110;
  // PM2.5
  spr.fillRoundRect(6, y, W - 12, 70, 12, C(CARD));
  if (wx.aqOk) {
    const char* word; uint32_t col; const char* tip; pmLevel(wx.pm25, word, col, tip);
    spr.fillRoundRect(14, y + 10, 64, 50, 10, C(col));
    txt(FL, String((int)roundf(wx.pm25)), 46, y + 30, 0xFFFFFF, D_MC);
    txt(FS, "PM2.5", 46, y + 50, 0xFFFFFF, D_MC);
    txt(FB, word, 88, y + 12, INK);
    txt(FS, fitText(FS, tip, W - 100), 88, y + 32, SOFT);
    txt(FS, "PM10 " + String((int)roundf(wx.pm10)) + "  (ug/m3)", 88, y + 50, SOFT);
  } else txt(FS, "PM2.5: no data", 14, y + 26, SOFT);
  y += 76;
  // more details
  spr.fillRoundRect(6, y, W - 12, 44, 12, C(CARD));
  int cw = (W - 12) / 3;
  String v[3] = {String(wx.hum) + "%", String(wx.wind, 0) + " km/h", String(wx.rain, 1) + " mm"};
  const char* k[3] = {"Humidity", "Wind", "Rain now"};
  for (int i = 0; i < 3; i++) { txt(FB, v[i], 6 + i * cw + cw / 2, y + 12, INK, D_MC); txt(FS, k[i], 6 + i * cw + cw / 2, y + 31, SOFT, D_MC); }
  y += 50;
  // 3 days
  const char* dn[3] = {"Today", "", ""};   // then day names (short, so they never hit the icon)
  for (int i = 1; i < 3; i++) { time_t t2 = nowT() + i * 86400; struct tm tm; localtime_r(&t2, &tm); dn[i] = EN_DOW[tm.tm_wday]; }
  for (int i = 0; i < 3; i++) {
    spr.fillRoundRect(6, y, W - 12, 34, 10, C(CARD));
    txt(FB, dn[i], 14, y + 17, INK, D_ML);
    drawWxIcon(wx.dCode[i], W - 140, y + 16, 22);
    txt(FS, String(wx.dRain[i]) + "%", W - 84, y + 17, 0x3B82F6, D_MR);
    txt(FB, String((int)roundf(wx.dMax[i])) + " / " + String((int)roundf(wx.dMin[i])), W - 14, y + 17, INK, D_MR);
    y += 38;
  }
  txt(FS, fitText(FS, "Updated " + agoMs(wx.at) + "  (Open-Meteo)", W - 20), 10, y + 4, SOFT);
  y += 26;
  netMax = max(0, y + netScroll - FTR_Y);
}
// Oil box: [grade + oil name] [today price] [tomorrow change: red up / green down]
// The grade (95, 91, E20, B20 ...) is moved to the front in bold, so it is seen even if the name is cut.
bool gradeTok(const String& t) {   // "95", "E20", "B7" ...
  if (!t.length() || t.length() > 4) return false;
  size_t i = 0;
  if (t[0] == 'E' || t[0] == 'B') i = 1;
  if (i >= t.length()) return false;
  for (; i < t.length(); i++) if (t[i] < '0' || t[i] > '9') return false;
  return true;
}
// Bangchak writes names in Thai: show them in English (unknown words stay as they are)
String oilWord(const String& t) {
  const char* M[][2] = {{"ไฮดีเซล", "Hi Diesel"}, {"ดีเซล", "Diesel"}, {"แก๊สโซฮอล์", "Gasohol"}, {"แก๊สโซฮอล", "Gasohol"},
                        {"เบนซิน", "Benzine"}, {"ไฮ", "Hi"}, {"พรีเมียม", "Premium"}, {"พลัส", "Plus"}, {"ซูเปอร์", "Super"}};
  for (auto& m : M) if (t == m[0]) return m[1];
  return t;
}
void oilSplit(const String& name, String& grade, String& rest) {
  grade = ""; rest = "";
  int st = 0;
  while (st <= (int)name.length()) {
    int sp = name.indexOf(' ', st); if (sp < 0) sp = name.length();
    String t = name.substring(st, sp);
    if (!grade.length() && gradeTok(t)) grade = t;
    else if (t == "S" || t == "EVO" || gradeTok(t)) {}   // 2nd number (like "97 ... 95") only confuses   // Bangchak brand words, same on many rows -> drop to save room
    else if (t.length()) { if (rest.length()) rest += ' '; rest += oilWord(t); }
    st = sp + 1;
  }
}
const int OIL_PRICE_R = 66;   // today price: right edge is W - this
int oilNameW() { return W - OIL_PRICE_R - 45 - 6 - 14; }
struct OilRow { String grade; int gw; std::vector<String> lines; };
OilRow oilRow(const Oil& o) {
  OilRow r; String rest; oilSplit(o.name, r.grade, rest);
  r.gw = 0;
  forceTh = hasThai(rest);   // all English now: sharp English fonts (Thai font only if a word was not known)
  if (r.grade.length()) { spr.setFont(pickFont(FB, r.grade)); r.gw = spr.textWidth(r.grade) + 5; }
  forceTh = false;
  r.lines = wrapText(FS, rest, oilNameW() - r.gw, 2);
  if (r.lines.empty()) r.lines.push_back("");
  return r;
}
int oilRowH(const OilRow& r) { return max(26, (int)r.lines.size() * 17 + 9); }
int oilCardH() {
  if (oil.empty()) return 0;
  int h = 52 + 8;
  for (auto& o : oil) h += oilRowH(oilRow(o));
  return h;
}
void drawOilCard(int y) {
  int h = oilCardH(); if (!h) return;
  spr.fillRoundRect(6, y, W - 12, h - 8, 10, C(CARD));
  txt(FB, "Oil price", 14, y + 8, INK);
  txt(FS, "Bangchak", W - 14, y + 9, SOFT, D_TR);
  txt(FS, fitText(FS, "baht/L. Arrow = tomorrow", W - 28), 14, y + 30, SOFT);
  spr.drawFastHLine(14, y + 50, W - 28, C(LINE));
  int yy = y + 52;
  for (auto& o : oil) {
    OilRow r = oilRow(o);
    int rh = oilRowH(r);
    int ny = yy + (rh - (int)r.lines.size() * 17) / 2;
    forceTh = hasThai(o.name) && r.lines.size() && hasThai(r.lines[0]);
    if (r.grade.length()) txt(FB, r.grade, 14, ny - 1, INK);
    for (size_t k = 0; k < r.lines.size(); k++) txt(FS, r.lines[k], 14 + r.gw, ny + k * 17, INK);
    forceTh = false;
    int my = yy + rh / 2;
    txt(FB, String(o.today, 2), W - OIL_PRICE_R, my, INK, D_MR);
    bool up = o.dif > 0.005f, down = o.dif < -0.005f;
    if (up || down) {
      uint32_t col = up ? 0xD03030 : 0x2E8B57;   // red = more expensive, green = cheaper
      String v = String(fabsf(o.dif), 2);
      spr.setFont(FB); int tx = W - 14 - spr.textWidth(v) - 13;
      if (up) spr.fillTriangle(tx, my + 5, tx + 10, my + 5, tx + 5, my - 4, C(col));
      else    spr.fillTriangle(tx, my - 4, tx + 10, my - 4, tx + 5, my + 5, C(col));
      txt(FB, v, W - 14, my, col, D_MR);
    } else txt(FS, "same", W - 14, my, SOFT, D_MR);
    yy += rh;
    if (&o != &oil.back()) spr.drawFastHLine(14, yy, W - 28, C(LINE));
  }
}
int newsCardH(int nLines) { return 16 + nLines * 24 + 22; }
// Tech / AI / Robots buttons at the top of the news list (they scroll with it)
const int NEWS_CHIPS_H = 36;
int newsChipW() { return (W - 12 - 8) / N_NEWSCAT; }
void drawNewsChips(int y) {
  for (int i = 0; i < N_NEWSCAT; i++) btn(6 + i * (newsChipW() + 4), y, newsChipW(), 28, NEWSCAT_N[i], i == newsCat, NEWSCAT_COL[i]);
}
int newsChipAt(int x) { int i = (x - 6) / (newsChipW() + 4); return x >= 6 && i >= 0 && i < N_NEWSCAT ? i : -1; }
int newsListTop(int top) { return top - netScroll + NEWS_CHIPS_H + (newsCat == 0 ? oilCardH() : 0) + (curNews().empty() ? 0 : 26); }
void drawNewsList(int top) {
  int y = top - netScroll;
  drawNewsChips(y); y += NEWS_CHIPS_H;
  if (newsCat == 0) { drawOilCard(y); y += oilCardH(); }
  auto& news = curNews();
  if (!news.empty()) {
    const char* H[N_NEWSCAT] = {"Tech news", "AI news", "Robot news"};
    txt(FB, H[newsCat], 10, y + 2, INK);
    if (newsCat == 2) txt(FS, "in English", W - 10, y + 3, SOFT, D_TR);   // no Thai site has robot news
    y += 26;
  }
  for (size_t i = 0; i < news.size(); i++) {
    auto lines = wrapText(FB, news[i].title, W - 32, 3);
    int h = newsCardH(lines.size());
    if (y + h > top - 40 && y < FTR_Y) {
      navAdd(6, y, W - 12, h - 4);   // press = read the story
      spr.fillRoundRect(6, y, W - 12, h - 4, 10, C(CARD));
      spr.fillRoundRect(6, y + 8, 4, h - 20, 2, C(NEWSCAT_COL[newsCat]));
      forceTh = hasThai(news[i].title);
      for (size_t k = 0; k < lines.size(); k++) txt(FB, lines[k], 17, y + 8 + k * 24, INK);
      forceTh = false;
      String when = newsWhen(news[i].t);
      txt(FS, fitText(FS, when.length() ? when + "  -  " + news[i].src : news[i].src, W - 30), 17, y + 10 + lines.size() * 24, SOFT);
    }
    y += h;
  }
  netMax = max(0, y + netScroll - FTR_Y + 6);
}
void drawNewsStory(int top) {
  auto& news = curNews();
  if (newsOpen < 0 || newsOpen >= (int)news.size()) return;
  News& n = news[newsOpen];
  int y = top - netScroll;
  auto tl = wrapText(FB, n.title, W - 24, 8);
  forceTh = hasThai(n.title);
  for (auto& l : tl) { txt(FB, l, 12, y, INK); y += 24; }
  forceTh = false;
  y += 4;
  String when = newsWhen(n.t);
  spr.fillRoundRect(12, y + 2, 4, 14, 2, C(NEWSCAT_COL[newsCat]));
  txt(FS, fitText(FS, when.length() ? when + "  -  " + n.src : n.src, W - 33), 21, y, SOFT); y += 26;
  auto dl = wrapText(FS, n.desc.length() ? n.desc : String("(no summary)"), W - 24, 40);
  forceTh = hasThai(n.desc);
  for (auto& l : dl) { txt(FS, l, 12, y, INK); y += 22; }
  forceTh = false;
  y += 10;
  txt(FS, "Tap here to go back to the list", 12, y, SOFT); y += 30;
  netMax = max(0, y + netScroll - FTR_Y);
}
void drawNet() {
  int top = netTop();
  netScroll = constrain(netScroll, 0, netMax);
  spr.setClipRect(0, top - 2, W, FTR_Y - top + 2);
  if (netMsg.length()) {
    int my = top;
    if (netTab == 1 && netOnline()) { drawNewsChips(top); my += NEWS_CHIPS_H; }   // another group can still be picked
    auto lines = wrapText(FS, netMsg, W - 36, 5);
    spr.fillRoundRect(8, my + 6, W - 16, 20 + lines.size() * 20, 10, C(CARD));
    for (size_t k = 0; k < lines.size(); k++) txt(FS, lines[k], 18, my + 16 + k * 20, INK);
    netMax = 0;
  } else if (netTab == 0) drawWeather(top);
  else if (newsOpen >= 0) drawNewsStory(top);
  else drawNewsList(top);
  scrollBar(top, FTR_Y - top, netScroll, netMax);
  if (netLoadingNow()) {   // downloading in the background: "Loading..." (the rest of the screen still works)
    String l = "Loading" + String("...").substring(0, 1 + (millis() / 400) % 3);
    bool empty = netTab == 0 ? !wx.ok : curNews().empty();
    if (empty) {
      int my = top + (netTab == 1 ? NEWS_CHIPS_H : 0);
      spr.fillRoundRect(8, my + 6, W - 16, 40, 10, C(CARD));
      txt(FB, l, 18, my + 26, SOFT, D_ML);
    } else {
      spr.setFont(FS); int tw = spr.textWidth("Loading...") + 24;
      spr.fillRoundRect((W - tw) / 2, FTR_Y - 34, tw, 26, 13, C(INK));
      txt(FS, l, (W - tw) / 2 + 12, FTR_Y - 21, ONINK, D_ML);
    }
  }
  spr.clearClipRect();
  drawNetTabs();
}
void netTap(int x, int y) {
  if (backHit(x, y)) {
    if (newsOpen >= 0) { newsOpen = -1; netScroll = 0; }
    else scr = S_APPS;
    dirty = true; return;
  }
  int ty = netTabY();
  if (y >= ty && y < ty + 30) {
    if (x >= W - 72) { newsOpen = -1; netScroll = 0; netLoad(true); return; }
    int w = netTabW();
    int t = x < netTabX() ? -1 : (x - netTabX()) / (w + 4);
    if (t >= 0 && t < 2 && t != netTab) { netTab = t; newsOpen = -1; netScroll = 0; dirty = true; render(); netLoad(false); }
    return;
  }
  if (netTab != 1) return;
  if (newsOpen >= 0) { newsOpen = -1; netScroll = 0; dirty = true; return; }   // tap story = back to list
  int top = netTop();
  int cy = top - (netMsg.length() ? 0 : netScroll);
  if (netOnline() && y >= cy && y < cy + 30) {   // Tech / AI / Robots
    int c = newsChipAt(x);
    if (c >= 0 && c != newsCat) { newsCat = c; netScroll = 0; netMsg = ""; dirty = true; render(); netLoad(false); }
    return;
  }
  if (netMsg.length()) return;
  auto& news = curNews();
  int yy = newsListTop(top);
  for (size_t i = 0; i < news.size(); i++) {
    auto lines = wrapText(FB, news[i].title, W - 32, 3);
    int h = newsCardH(lines.size());
    if (y >= yy && y < yy + h) { newsOpen = i; netScroll = 0; dirty = true; return; }
    yy += h;
  }
}
