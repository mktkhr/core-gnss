#include <M5Core2.h>
#include <TinyGPSPlus.h>
#include "config.h"

TinyGPSPlus gps;

static void smartDelay(unsigned long ms);
static void printFloat(float val, bool valid, int len, int prec);
static void printInt(unsigned long val, bool valid, int len);
static void printDateTime(TinyGPSDate &d, TinyGPSTime &t);
static void printStr(const char *str, int len);

void setup()
{
    M5.begin(false); // (bool LCDEnable, bool SDEnable, bool SerialEnable, bool I2CEnable, mbus_mode_t mode, bool SpeakerEnable)

    // SDカードへの接続を最大3回まで試行
    for (int i = 0; i < 3; i++)
    {
        if (SD.begin())
        {
            uint8_t cardType = SD.cardType();
            if (cardType == CARD_NONE)
            {
                Serial.println("No SD card attached");
                return;
            }

            Serial.print("SD Card Type: ");
            if (cardType == CARD_MMC)
            {
                Serial.println("MMC");
            }
            else if (cardType == CARD_SD)
            {
                Serial.println("SDSC");
            }
            else if (cardType == CARD_SDHC)
            {
                Serial.println("SDHC");
            }
            else
            {
                Serial.println("UNKNOWN");
            }
            break;
        }
        delay(100);
        Serial.println("Card Mount Failed");
    }

    // テスト用に1度ログファイルを削除
    // deleteFileIfExist(LOG_FILE_PATH);

    // ログファイルの生成
    createFileIfNotExist(LOG_FILE_PATH);

    // GNSSの設定
    Serial2.begin(GNSS_BAUD_RATE, SERIAL_8N1, GNSS_RX, GNSS_TX);

    Serial.println();
    Serial.println(F(
        "Sats HDOP  Latitude   Longitude   Fix  Date       Time     Date Alt   "
        " Course Speed Card  Distance Course Card  Chars Sentences Checksum"));
    Serial.println(
        F("           (deg)      (deg)       Age                      Age  (m) "
          "   --- from GPS ----  ---- to London  ----  RX    RX        Fail"));
    Serial.println(F(
        "----------------------------------------------------------------------"
        "------------------------------------------------------------------"));
}

void loop()
{
    static const double LONDON_LAT = 51.508131, LONDON_LON = -0.128002;
    String csvRow = "";

    // macアドレス
    csvRow += getMacAddress() + ",";
    // 日付
    csvRow += generateDateTimeStr(gps.date, gps.time) + ",";

    // 衛星数
    printInt(gps.satellites.value(), gps.satellites.isValid(), 5);
    csvRow += gps.satellites.isValid() ? String(gps.satellites.value()) + "," : "***,";

    // hdop
    printFloat(gps.hdop.hdop(), gps.hdop.isValid(), 6, 1);
    csvRow += gps.hdop.isValid() ? String(gps.hdop.hdop()) + "," : "***,";

    // 緯度
    printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);
    csvRow += gps.location.isValid() ? String(gps.location.lat()) + "," : "***,";

    // 経度
    printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);
    csvRow += gps.location.isValid() ? String(gps.location.lng()) + "," : "***,";

    printInt(gps.location.age(), gps.location.isValid(), 5);
    printDateTime(gps.date, gps.time);

    // 高度
    printFloat(gps.altitude.meters(), gps.altitude.isValid(), 7, 2);
    csvRow += gps.altitude.isValid() ? String(gps.altitude.meters()) + "," : "***,";

    printFloat(gps.course.deg(), gps.course.isValid(), 7, 2);

    // 速度
    printFloat(gps.speed.kmph(), gps.speed.isValid(), 6, 2);
    csvRow += gps.speed.isValid() ? String(gps.speed.kmph()) + "," : "***,";

    // 方角
    csvRow += gps.course.isValid() ? String(gps.course.deg()) + "," : "***,";

    // Age
    csvRow += gps.location.isValid() ? String(gps.location.age()) + "," : "***,";

    printStr(
        gps.course.isValid() ? TinyGPSPlus::cardinal(gps.course.deg()) : "*** ",
        6);

    unsigned long distanceKmToLondon =
        (unsigned long)TinyGPSPlus::distanceBetween(
            gps.location.lat(), gps.location.lng(), LONDON_LAT, LONDON_LON) /
        1000;
    printInt(distanceKmToLondon, gps.location.isValid(), 9);

    double courseToLondon = TinyGPSPlus::courseTo(
        gps.location.lat(), gps.location.lng(), LONDON_LAT, LONDON_LON);

    printFloat(courseToLondon, gps.location.isValid(), 7, 2);

    const char *cardinalToLondon = TinyGPSPlus::cardinal(courseToLondon);

    printStr(gps.location.isValid() ? cardinalToLondon : "*** ", 6);

    printInt(gps.charsProcessed(), true, 6);
    printInt(gps.sentencesWithFix(), true, 10);
    printInt(gps.failedChecksum(), true, 9);
    Serial.println();

    csvRow += "\n"; // 改行
    writeFile(LOG_FILE_PATH, csvRow);

    smartDelay(1000);

    if (millis() > 5000 && gps.charsProcessed() < 10)
        Serial.println(F("No GPS data received: check wiring"));
}

static void smartDelay(unsigned long ms)
{
    unsigned long start = millis();
    do
    {
        while (Serial2.available())
            gps.encode(Serial2.read());
    } while (millis() - start < ms);
}

static void printFloat(float val, bool valid, int len, int prec)
{
    if (!valid)
    {
        while (len-- > 1)
            Serial.print('*');
        Serial.print(' ');
    }
    else
    {
        Serial.print(val, prec);
        int vi = abs((int)val);
        int flen = prec + (val < 0.0 ? 2 : 1); // . and -
        flen += vi >= 1000 ? 4 : vi >= 100 ? 3
                             : vi >= 10    ? 2
                                           : 1;
        for (int i = flen; i < len; ++i)
            Serial.print(' ');
    }
    smartDelay(0);
}

static void printInt(unsigned long val, bool valid, int len)
{
    char sz[32] = "*****************";
    if (valid)
        sprintf(sz, "%ld", val);
    sz[len] = 0;
    for (int i = strlen(sz); i < len; ++i)
        sz[i] = ' ';
    if (len > 0)
        sz[len - 1] = ' ';
    Serial.print(sz);
    smartDelay(0);
}

static void printDateTime(TinyGPSDate &d, TinyGPSTime &t)
{
    if (!d.isValid())
    {
        Serial.print(F("********** "));
    }
    else
    {
        char sz[32];
        sprintf(sz, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
        Serial.print(sz);
    }

    if (!t.isValid())
    {
        Serial.print(F("******** "));
    }
    else
    {
        char sz[32];
        sprintf(sz, "%02d:%02d:%02d ", t.hour(), t.minute(), t.second());
        Serial.print(sz);
    }

    printInt(d.age(), d.isValid(), 5);
    smartDelay(0);
}

/**
 * @brief 日時の文字列を生成
 *
 * @param date 日付オブジェクト
 * @param time 時刻オブジェクト
 * @return String 日付文字列
 */
String generateDateTimeStr(TinyGPSDate &date, TinyGPSTime &time)
{
    String datetime = "";

    if (!date.isValid())
    {
        datetime += "**/**/**";
    }
    else
    {
        datetime += String(date.year()) + "/" + String(date.month()) + "/" + String(date.day());
    }

    if (!time.isValid())
    {
        datetime += " **:**:**";
    }
    else
    {
        datetime += " " + String(time.hour()) + ":" + String(time.minute()) + ":" + String(time.second());
    }

    return datetime;
}

static void printStr(const char *str, int len)
{
    int slen = strlen(str);
    for (int i = 0; i < len; ++i)
        Serial.print(i < slen ? str[i] : ' ');
    smartDelay(0);
}

/**
 * @brief ファイルの追記
 *
 * @param path ファイルパス
 * @param message 追記する文字列
 * @return true 追記成功
 * @return false 追記失敗
 */
bool writeFile(const char *path, String message)
{

    File file = SD.open(path, FILE_APPEND);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return false;
    }

    if (file.print(message))
    {
        file.close();
        return true;
    }

    Serial.println("Write file failed");
    file.close();
    return false;
}

/**
 * @brief ファイルが存在しない場合，ファイルを追加する
 *
 * @param path ファイルパス
 */
void createFileIfNotExist(const char *path)
{

    File file = SD.open(path);
    if (file)
    {
        // 既にファイルがある場合
        Serial.printf("File already exist at: %s\n", path);
        file.close();
        return;
    }

    file.close();

    // ファイルが無い場合
    Serial.printf("Creating file: %s\n", path);
    File created_file = SD.open(path, FILE_WRITE);

    if (!created_file)
    {
        Serial.println("Failed to create file");
        return;
    }
    created_file.close();

    // ヘッダーの追記
    writeFile(path, CSV_HEADER);
    Serial.println("Appneding header row...");
}

/**
 * @brief ファイルを削除する
 *
 * @param path ファイルパス
 */
void deleteFileIfExist(const char *path)
{

    File file = SD.open(path);
    if (!file)
    {
        Serial.printf("File does not exist at: %s\n", path);
    }

    Serial.printf("Deleting file: %s\n", path);
    if (SD.remove(path))
    {
        Serial.println("File deleted successfully");
    }

    Serial.println("File delete failed");
}

/**
 * @brief 端末のMACアドレスを取得する
 *
 * @return String MACアドレス
 */
String getMacAddress()
{
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    char baseMacChr[18] = {0};
    return String(baseMac[0]) + ":" + String(baseMac[1]) + ":" + String(baseMac[2]) + ":" + String(baseMac[3]) + ":" + String(baseMac[4]) + ":" + String(baseMac[5]);
}