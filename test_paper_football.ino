#include "M5Unified.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// WiFi Configuration
const char* ssid = "TP-Link_235C";
const char* password = "06772512";

// 時刻設定
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9 * 3600;  // JST (UTC+9)
const int daylightOffset_sec = 0;

// 手動日付設定（NTP失敗時のフォールバック）
const String CURRENT_DATE = "2025-06-18";  // 今日の日付を手動設定
const String DATE_7_DAYS_AGO = "2025-06-11";
const String DATE_14_DAYS_AGO = "2025-06-04";
const String DATE_30_DAYS_AGO = "2025-06-01";  // 6/1に変更

// スコア用の構造体
struct MatchScore {
  String homeTeam;
  String awayTeam;
  int homeScore;
  int awayScore;
  String matchDate;
  String competition;
  bool isFinished;
  unsigned long dateTimestamp; // ソート用のタイムスタンプ
};

// スコア表示用の設定
const int MAX_MATCHES = 15;
MatchScore recentMatches[MAX_MATCHES];
int matchCount = 0;
unsigned long lastRefreshTime = 0;
bool isRefreshing = false;

// デバッグ情報
String debugInfo = "";
String apiStatus = "";
int httpResponseCode = 0;

// APIキー
const char* apiKey = "da4aeda6d7b942ddbdeb727fda3059a2";

// Display Configuration (横長用)
const int HEADER_HEIGHT = 60;
const int RELOAD_BUTTON_WIDTH = 120;
const int RELOAD_BUTTON_HEIGHT = 40;

// タッチボタンエリア
int reloadButtonX, reloadButtonY;

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  
  // 横長表示に変更（回転90度）
  M5.Display.setRotation(1); // 横長モード
  M5.Display.fillScreen(TFT_WHITE);
  
  // リロードボタンの位置を計算
  reloadButtonX = M5.Display.width() - RELOAD_BUTTON_WIDTH - 10;
  reloadButtonY = 10;
  
  Serial.println("M5Paper Football Scores Display Starting (Landscape)...");
  
  showDebugScreen("Starting up...");
  
  connectToWiFi();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected Successfully!");
    showDebugScreen("WiFi Connected\nSetting up time...");
    
    // NTP時刻同期を試行
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    struct tm timeinfo;
    int attempts = 0;
    bool timeSync = false;
    
    while (attempts < 10) {
      delay(1000);
      attempts++;
      if (getLocalTime(&timeinfo)) {
        // 年が2025年の範囲内かチェック
        int year = timeinfo.tm_year + 1900;
        if (year == 2025) {
          timeSync = true;
          break;
        } else {
          Serial.println("Invalid NTP year detected: " + String(year) + ", retrying...");
        }
      }
      showDebugScreen("Syncing time... " + String(attempts) + "/10");
    }
    
    if (timeSync) {
      showDebugScreen("Time synced!\n" + getCurrentDateString());
      Serial.println("NTP sync successful: " + getCurrentDateString());
    } else {
      showDebugScreen("Time sync failed\nUsing manual dates\n" + CURRENT_DATE);
      Serial.println("NTP sync failed, using manual dates");
    }
    
    delay(2000);
    
    fetchRecentScores();
    displayResults();
    lastRefreshTime = millis();
  } else {
    Serial.println("WiFi Connection Failed");
    showDebugScreen("WiFi Connection Failed");
    loadMockScores();
    displayResults();
  }
}

void loop() {
  M5.update();
  
  // 物理ボタン（従来機能）
  if (M5.BtnC.wasPressed()) {
    Serial.println("Physical button pressed - Refreshing scores...");
    refreshScores();
  }
  
  // タッチ操作の処理
  if (M5.Touch.getCount()) {
    auto touch = M5.Touch.getDetail();
    if (touch.wasPressed()) {
      int touchX = touch.x;
      int touchY = touch.y;
      
      // リロードボタンのタッチ判定
      if (touchX >= reloadButtonX && touchX <= (reloadButtonX + RELOAD_BUTTON_WIDTH) &&
          touchY >= reloadButtonY && touchY <= (reloadButtonY + RELOAD_BUTTON_HEIGHT)) {
        Serial.println("Reload button touched - Refreshing scores...");
        refreshScores();
      }
    }
  }
  
  // 30分ごとに自動更新
  if (WiFi.status() == WL_CONNECTED && 
      (millis() - lastRefreshTime) > (30 * 60 * 1000) &&
      !isRefreshing) {
    refreshScores();
  }
  
  delay(100);
}

void refreshScores() {
  showDebugScreen("Refreshing scores...");
  fetchRecentScores();
  displayResults();
}

// 日付文字列をタイムスタンプに変換（ソート用）
unsigned long dateStringToTimestamp(String dateStr) {
  if (dateStr.length() < 10) return 0;
  
  int year = dateStr.substring(0, 4).toInt();
  int month = dateStr.substring(5, 7).toInt();
  int day = dateStr.substring(8, 10).toInt();
  
  // 簡単なタイムスタンプ計算（年月日のみ）
  return (unsigned long)year * 10000 + month * 100 + day;
}

// 試合データを日付順（新しい順）にソート
void sortMatchesByDate() {
  Serial.println("Sorting matches by date (newest first)...");
  
  // バブルソート（配列が小さいので十分）
  for (int i = 0; i < matchCount - 1; i++) {
    for (int j = 0; j < matchCount - i - 1; j++) {
      if (recentMatches[j].dateTimestamp < recentMatches[j + 1].dateTimestamp) {
        // 要素を交換
        MatchScore temp = recentMatches[j];
        recentMatches[j] = recentMatches[j + 1];
        recentMatches[j + 1] = temp;
      }
    }
  }
  
  Serial.println("Matches sorted successfully");
  
  // ソート結果をログ出力
  Serial.println("=== Sorted Match Order ===");
  for (int i = 0; i < min(matchCount, 5); i++) {
    Serial.println(String(i + 1) + ". " + 
                   recentMatches[i].matchDate + " | " + 
                   recentMatches[i].homeTeam + " vs " + 
                   recentMatches[i].awayTeam + " | " + 
                   recentMatches[i].competition);
  }
  Serial.println("=== End Sort Order ===");
}

// 現在日付を取得（確実なフォールバック付き）
String getCurrentDateString() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int year = timeinfo.tm_year + 1900;
    // 2025年の有効な範囲内かチェック
    if (year == 2025) {
      char dateString[11];
      strftime(dateString, sizeof(dateString), "%Y-%m-%d", &timeinfo);
      String result = String(dateString);
      
      Serial.println("NTP Date: " + result);
      return result;
    } else {
      Serial.println("Invalid NTP year: " + String(year) + " (using manual date)");
    }
  }
  
  // フォールバック：手動設定の日付を使用
  Serial.println("Using manual current date: " + CURRENT_DATE);
  return CURRENT_DATE;
}

// 指定日数前の日付を取得（確実なフォールバック付き）
String getDateDaysAgo(int daysAgo) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int year = timeinfo.tm_year + 1900;
    // 2025年の有効な範囲内かチェック
    if (year == 2025) {
      time_t now = mktime(&timeinfo);
      now -= (daysAgo * 24 * 60 * 60);
      struct tm* pastTime = localtime(&now);
      
      char dateString[11];
      strftime(dateString, sizeof(dateString), "%Y-%m-%d", pastTime);
      String result = String(dateString);
      
      Serial.println("NTP Date " + String(daysAgo) + " days ago: " + result);
      return result;
    } else {
      Serial.println("Invalid NTP year: " + String(year) + " (using manual date)");
    }
  }
  
  // フォールバック：手動設定の日付を使用
  String result;
  switch(daysAgo) {
    case 1: result = "2025-06-17"; break;
    case 2: result = "2025-06-16"; break;
    case 3: result = "2025-06-15"; break;
    case 4: result = "2025-06-14"; break;
    case 5: result = "2025-06-13"; break;
    case 6: result = "2025-06-12"; break;
    case 7: result = DATE_7_DAYS_AGO; break;
    case 14: result = DATE_14_DAYS_AGO; break;
    case 30: result = DATE_30_DAYS_AGO; break;
    default: result = DATE_30_DAYS_AGO;
  }
  
  Serial.println("Using manual date " + String(daysAgo) + " days ago: " + result);
  return result;
}

// より広範囲でClub World Cupを含むAPIのURLを生成
String getRecentMatchesURL() {
  // 2025年6月10日〜19日の10日間の範囲を使用（API制限内）
  String dateFrom = "2025-06-10";
  String dateTo = "2025-06-19";
  
  String url = "https://api.football-data.org/v4/matches?status=FINISHED&dateFrom=" + 
               dateFrom + "&dateTo=" + dateTo;
  
  Serial.println("Generated API URL (2025 Jun 10-19): " + url);
  Serial.println("Date validation - From: " + dateFrom + ", To: " + dateTo);
  
  return url;
}

// Club World Cup専用のAPIも試す
String getClubWorldCupURL() {
  return "https://api.football-data.org/v4/competitions/CWC/matches?status=FINISHED";
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    Serial.print(".");
    attempts++;
    
    if (attempts % 5 == 0) {
      showDebugScreen("Connecting to WiFi...\nAttempt: " + String(attempts) + "/30");
    }
  }
}

void showDebugScreen(String message) {
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setCursor(10, 15);
  M5.Display.println("DEBUG INFO");
  
  M5.Display.drawFastHLine(0, 45, M5.Display.width(), TFT_BLACK);
  
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 60);
  M5.Display.println(message);
}

void fetchRecentScores() {
  if (isRefreshing) return;
  
  isRefreshing = true;
  debugInfo = "";
  apiStatus = "";
  httpResponseCode = 0;
  
  showDebugScreen("Generating API URL...");
  
  String apiURL = getRecentMatchesURL();
  
  debugInfo += "Date Range (2025 Jun 10-19):\n";
  debugInfo += "From: 2025-06-10\n";
  debugInfo += "To: 2025-06-19\n";
  debugInfo += "WiFi: " + String(WiFi.status()) + "\n";
  
  Serial.println("=== Fetching Scores Debug ===");
  Serial.println("API URL: " + apiURL);
  
  if (WiFi.status() == WL_CONNECTED) {
    showDebugScreen("Sending HTTP request...");
    
    HTTPClient http;
    http.begin(apiURL);
    http.addHeader("X-Auth-Token", apiKey);
    http.addHeader("User-Agent", "M5Paper-Football-App/1.0");
    http.setTimeout(15000);
    
    httpResponseCode = http.GET();
    debugInfo += "HTTP Code: " + String(httpResponseCode) + "\n";
    
    Serial.printf("HTTP Code: %d\n", httpResponseCode);
    
    if (httpResponseCode == HTTP_CODE_OK) {
      String payload = http.getString();
      debugInfo += "Response Length: " + String(payload.length()) + "\n";
      
      Serial.printf("Payload length: %d\n", payload.length());
      
      showDebugScreen("Parsing JSON data...");
      
      if (payload.length() > 50) {
        parseScoreData(payload);
        debugInfo += "Matches Found: " + String(matchCount) + "\n";
        
        // Club World Cup専用検索も試す
        if (matchCount < 5) {
          debugInfo += "Trying Club World Cup API...\n";
          tryClubWorldCupAPI();
        }
        
        // 日付順にソート
        if (matchCount > 0) {
          sortMatchesByDate();
          apiStatus = "SUCCESS: " + String(matchCount) + " matches (sorted)";
        } else {
          apiStatus = "No matches found";
          loadMockScores();
        }
      } else {
        debugInfo += "Response too short\n";
        apiStatus = "ERROR: Empty response";
        loadMockScores();
      }
    } else {
      handleHTTPError(httpResponseCode);
      loadMockScores();
    }
    
    http.end();
  } else {
    debugInfo += "No WiFi Connection\n";
    apiStatus = "ERROR: No WiFi";
    loadMockScores();
  }
  
  isRefreshing = false;
  lastRefreshTime = millis();
  Serial.println("=== Debug End ===");
}

// Club World Cup専用API呼び出し
void tryClubWorldCupAPI() {
  String cwcURL = getClubWorldCupURL();
  debugInfo += "CWC URL: " + cwcURL + "\n";
  
  HTTPClient http;
  http.begin(cwcURL);
  http.addHeader("X-Auth-Token", apiKey);
  http.setTimeout(10000);
  
  int code = http.GET();
  debugInfo += "CWC HTTP Code: " + String(code) + "\n";
  
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    debugInfo += "CWC Response Length: " + String(payload.length()) + "\n";
    
    Serial.println("=== Club World Cup API Response ===");
    Serial.println(payload.substring(0, 500));
    
    // CWCデータを既存データに追加
    parseAdditionalScoreData(payload);
  } else {
    debugInfo += "CWC API failed: " + String(code) + "\n";
  }
  
  http.end();
}

void handleHTTPError(int code) {
  String errorDetail = "";
  switch(code) {
    case 400:
      errorDetail = "Bad Request (Check date format)";
      apiStatus = "ERROR 400: " + errorDetail;
      break;
    case 401:
      apiStatus = "ERROR 401: Unauthorized";
      break;
    case 403:
      apiStatus = "ERROR 403: Forbidden";
      break;
    case 429:
      apiStatus = "ERROR 429: Rate Limit";
      break;
    case 500:
      apiStatus = "ERROR 500: Server Error";
      break;
    default:
      apiStatus = "ERROR " + String(code) + ": HTTP Error";
  }
  debugInfo += apiStatus + "\n";
  Serial.println(apiStatus);
  
  // 400エラーの場合は日付問題の可能性が高い
  if (code == 400) {
    Serial.println("400 Error - Likely date issue. Check API URL dates.");
  }
}

void parseScoreData(String jsonData) {
  const size_t capacity = JSON_ARRAY_SIZE(100) + 100*JSON_OBJECT_SIZE(20) + 8000;
  DynamicJsonDocument doc(capacity);
  
  DeserializationError error = deserializeJson(doc, jsonData);
  
  if (error) {
    debugInfo += "JSON Error: " + String(error.c_str()) + "\n";
    apiStatus = "ERROR: JSON Parse Failed";
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    loadMockScores();
    return;
  }
  
  debugInfo += "JSON Parse: OK\n";
  
  if (!doc.containsKey("matches")) {
    debugInfo += "No 'matches' key\n";
    apiStatus = "ERROR: No matches data";
    loadMockScores();
    return;
  }
  
  JsonArray matches = doc["matches"];
  matchCount = 0;
  
  debugInfo += "Total matches: " + String(matches.size()) + "\n";
  
  // 全ての大会をログ出力してClub World Cupがあるか確認
  Serial.println("=== Competition Analysis ===");
  for (JsonObject match : matches) {
    String competition = match["competition"]["name"].as<String>();
    String homeTeam = match["homeTeam"]["name"].as<String>();
    String awayTeam = match["awayTeam"]["name"].as<String>();
    String matchDate = match["utcDate"].as<String>();
    
    Serial.println("Date: " + matchDate + " | Competition: " + competition + " | " + homeTeam + " vs " + awayTeam);
    
    if (matchCount < MAX_MATCHES) {
      recentMatches[matchCount].homeTeam = homeTeam;
      recentMatches[matchCount].awayTeam = awayTeam;
      recentMatches[matchCount].homeScore = match["score"]["fullTime"]["home"];
      recentMatches[matchCount].awayScore = match["score"]["fullTime"]["away"];
      recentMatches[matchCount].matchDate = matchDate;
      recentMatches[matchCount].competition = competition;
      recentMatches[matchCount].isFinished = match["status"].as<String>() == "FINISHED";
      recentMatches[matchCount].dateTimestamp = dateStringToTimestamp(matchDate);
      
      matchCount++;
    }
  }
  
  Serial.println("=== End Competition Analysis ===");
}

// 追加データ用の解析（CWC専用）
void parseAdditionalScoreData(String jsonData) {
  const size_t capacity = JSON_ARRAY_SIZE(50) + 50*JSON_OBJECT_SIZE(15) + 4000;
  DynamicJsonDocument doc(capacity);
  
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) return;
  
  if (!doc.containsKey("matches")) return;
  
  JsonArray matches = doc["matches"];
  int originalCount = matchCount;
  
  for (JsonObject match : matches) {
    if (matchCount >= MAX_MATCHES) break;
    
    String matchDate = match["utcDate"].as<String>();
    
    recentMatches[matchCount].homeTeam = match["homeTeam"]["name"].as<String>();
    recentMatches[matchCount].awayTeam = match["awayTeam"]["name"].as<String>();
    recentMatches[matchCount].homeScore = match["score"]["fullTime"]["home"];
    recentMatches[matchCount].awayScore = match["score"]["fullTime"]["away"];
    recentMatches[matchCount].matchDate = matchDate;
    recentMatches[matchCount].competition = match["competition"]["name"].as<String>();
    recentMatches[matchCount].isFinished = match["status"].as<String>() == "FINISHED";
    recentMatches[matchCount].dateTimestamp = dateStringToTimestamp(matchDate);
    
    matchCount++;
  }
  
  debugInfo += "Added CWC matches: " + String(matchCount - originalCount) + "\n";
}

void loadMockScores() {
  Serial.println("Loading mock score data with Club World Cup (sorted by date)...");
  matchCount = 10;
  
  // Club World Cup の結果を含む（新しい順に配置）
  recentMatches[0] = {"Manchester City", "Al-Hilal", 3, 0, "2024-06-18", "FIFA Club World Cup", true, 20240618};
  recentMatches[1] = {"Real Madrid", "Pachuca", 2, 1, "2024-06-17", "FIFA Club World Cup", true, 20240617};
  recentMatches[2] = {"Liverpool", "Arsenal", 2, 1, "2024-06-16", "Premier League", true, 20240616};
  recentMatches[3] = {"Barcelona", "PSG", 1, 2, "2024-06-15", "Champions League", true, 20240615};
  recentMatches[4] = {"Bayern Munich", "Dortmund", 4, 1, "2024-06-14", "Bundesliga", true, 20240614};
  recentMatches[5] = {"Juventus", "Inter Milan", 0, 2, "2024-06-13", "Serie A", true, 20240613};
  recentMatches[6] = {"Chelsea", "Manchester United", 3, 1, "2024-06-12", "Premier League", true, 20240612};
  recentMatches[7] = {"Ajax", "Feyenoord", 2, 2, "2024-06-11", "Eredivisie", true, 20240611};
  recentMatches[8] = {"Celtic", "Rangers", 1, 0, "2024-06-10", "Scottish Premiership", true, 20240610};
  recentMatches[9] = {"AC Milan", "Napoli", 2, 1, "2024-06-09", "Serie A", true, 20240609};
  
  if (apiStatus == "") {
    apiStatus = "Using Mock Data (sorted by date)";
  }
}

void displayResults() {
  M5.Display.fillScreen(TFT_WHITE);
  
  // ヘッダー（横長レイアウト）
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setCursor(10, 10);
  M5.Display.println("FOOTBALL SCORES");
  
  // リロードボタンを描画
  drawReloadButton();
  
  // 日付範囲とステータス表示（横並び）
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setCursor(10, 45);
  String dateRange = getDateDaysAgo(30).substring(5) + " to " + getCurrentDateString().substring(5);
  M5.Display.print("Period: " + dateRange);
  
  // API状態表示（右側）
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setCursor(400, 45);
  String status = apiStatus.substring(0, 15);
  if (apiStatus.indexOf("SUCCESS") >= 0) {
    status = "[OK] " + status;
  } else if (apiStatus.indexOf("ERROR") >= 0) {
    status = "[ERR] " + status;
  }
  M5.Display.print("Status: " + status);
  
  // 「新しい順」表示
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setCursor(650, 45);
  M5.Display.print("(Newest First)");
  
  // 区切り線
  M5.Display.drawFastHLine(0, 75, M5.Display.width(), TFT_BLACK);
  
  // スコア表示（横長レイアウト、2列表示）
  if (matchCount > 0) {
    int startY = 85;
    int matchHeight = 60;
    int leftColumn = 10;
    int rightColumn = M5.Display.width() / 2 + 10;
    int maxDisplayPerColumn = 6;
    
    for (int i = 0; i < min(matchCount, maxDisplayPerColumn * 2); i++) {
      int column = (i < maxDisplayPerColumn) ? leftColumn : rightColumn;
      int row = i % maxDisplayPerColumn;
      int y = startY + (row * matchHeight);
      
      displaySingleScoreLandscape(recentMatches[i], column, y, (M5.Display.width() / 2) - 20);
    }
  }
  
  // フッター
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setCursor(10, M5.Display.height() - 20);
  M5.Display.println("Touch 'Reload' or press button to refresh | Time: " + getTimeString());
}

void drawReloadButton() {
  // ボタンの背景（白黒なので塗りつぶし無し）
  M5.Display.drawRoundRect(reloadButtonX, reloadButtonY, RELOAD_BUTTON_WIDTH, RELOAD_BUTTON_HEIGHT, 5, TFT_BLACK);
  
  // ボタンの内側に線を追加して強調
  M5.Display.drawRoundRect(reloadButtonX + 2, reloadButtonY + 2, RELOAD_BUTTON_WIDTH - 4, RELOAD_BUTTON_HEIGHT - 4, 3, TFT_BLACK);
  
  // ボタンのテキスト
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setCursor(reloadButtonX + 20, reloadButtonY + 10);
  M5.Display.println("Reload");
}

void displaySingleScoreLandscape(MatchScore match, int x, int y, int maxWidth) {
  // 試合日付を上部に表示
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setCursor(x, y);
  M5.Display.println(formatDateWithDay(match.matchDate));
  
  // チーム名とスコア（メイン表示）
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setCursor(x, y + 12);
  
  String homeTeam = shortenTeamName(match.homeTeam);
  String awayTeam = shortenTeamName(match.awayTeam);
  
  // スコア部分（白黒用の勝敗表示）
  String scoreText;
  if (match.homeScore > match.awayScore) {
    // ホーム勝利：ホームチームに★を付ける
    scoreText = "★" + homeTeam + " " + String(match.homeScore) + "-" + String(match.awayScore) + " " + awayTeam;
  } else if (match.awayScore > match.homeScore) {
    // アウェイ勝利：アウェイチームに★を付ける
    scoreText = homeTeam + " " + String(match.homeScore) + "-" + String(match.awayScore) + " ★" + awayTeam;
  } else {
    // 引き分け：両チームに=を付ける
    scoreText = "=" + homeTeam + " " + String(match.homeScore) + "-" + String(match.awayScore) + " " + awayTeam + "=";
  }
  
  M5.Display.print(scoreText);
  
  // 大会名（下部）
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setCursor(x, y + 35);
  M5.Display.println(shortenCompetitionName(match.competition));
  
  // 区切り線
  M5.Display.drawFastHLine(x, y + 50, maxWidth, TFT_BLACK);
}

String shortenTeamName(String teamName) {
  if (teamName.length() > 10) {
    return teamName.substring(0, 10) + ".";
  }
  return teamName;
}

String shortenCompetitionName(String compName) {
  if (compName == "Premier League") return "EPL";
  if (compName == "La Liga") return "La Liga";
  if (compName == "Bundesliga") return "Bundesliga";
  if (compName == "Serie A") return "Serie A";
  if (compName == "Ligue 1") return "Ligue 1";
  if (compName == "Champions League") return "UCL";
  if (compName == "Europa League") return "UEL";
  if (compName == "FIFA Club World Cup") return "Club World Cup";
  
  if (compName.length() > 15) {
    return compName.substring(0, 15) + ".";
  }
  return compName;
}

// 現在時刻を取得する関数
String getTimeString() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeString[9];
    strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
    return String(timeString);
  }
  return "N/A";
}

// 日付を曜日付きでフォーマットする関数
String formatDateWithDay(String dateStr) {
  if (dateStr.length() < 10) return dateStr;
  
  // 日付文字列から年月日を抽出
  int year = dateStr.substring(0, 4).toInt();
  int month = dateStr.substring(5, 7).toInt();
  int day = dateStr.substring(8, 10).toInt();
  
  // 曜日を計算（Zellerの公式の簡略版）
  if (month < 3) {
    month += 12;
    year--;
  }
  int weekday = (day + (13 * (month + 1)) / 5 + year + year / 4 - year / 100 + year / 400) % 7;
  
  // 曜日の配列
  const char* days[] = {"Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri"};
  
  // MM/DD (Day) の形式で返す
  String result = String(month > 12 ? month - 12 : month) + "/" + String(day) + " (" + days[weekday] + ")";
  return result;
}