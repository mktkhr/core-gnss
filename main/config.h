const char *LOG_FILE_PATH = "/log.csv";
const unsigned long GNSS_BAUD_RATE = 38400;
const int8_t GNSS_RX = 13; // GNSS側のスイッチと一致させること
const int8_t GNSS_TX = 14; // GNSS側のスイッチと一致させること

const String CSV_HEADER = "macAddress,datetime,satellites,hdop,latitude,longitude,alt,speed,direction,dataAge\n";