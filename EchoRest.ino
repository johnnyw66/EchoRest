/****************************************************************
 *
 * This Example only for M5AtomEcho!
 *
 * Arduino tools Setting
 * -board : M5StickC
 * -Upload Speed: 115200 / 750000 / 1500000
 *
 ****************************************************************/
#include <HTTPClient.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <HTTPClient.h>

#include "BaiduRest.h"
#include "M5Atom.h"
#include "Secrets.h"

extern size_t playBeep(int __freq = 2000, int __timems = 200, int __maxval = 10000);

const char *WifiSSID = SECRET_WIFI_SSID;
const char *WifiPWD  = SECRET_WIFI_PWD;

#define CONFIG_I2S_BCK_PIN     19
#define CONFIG_I2S_LRCK_PIN    33
#define CONFIG_I2S_DATA_PIN    22
#define CONFIG_I2S_DATA_IN_PIN 23

#define SPEAK_I2S_NUMBER I2S_NUM_0

#define MODE_MIC 0
#define MODE_SPK 1

bool InitI2SSpeakOrMic(int mode) {
    esp_err_t err = ESP_OK;

    i2s_driver_uninstall(SPEAK_I2S_NUMBER);
    i2s_config_t i2s_config = {
        .mode        = (i2s_mode_t)(I2S_MODE_MASTER),
        .sample_rate = (mode == MODE_MIC ? 16000 : 88200),
        .bits_per_sample =
            I2S_BITS_PER_SAMPLE_16BIT,  // is fixed at 12bit, stereo, MSB
        .channel_format = I2S_CHANNEL_FMT_ALL_RIGHT,
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 1, 0)
        .communication_format =
            I2S_COMM_FORMAT_STAND_I2S,  // Set the format of the communication.
#else                                   // 设置通讯格式
        .communication_format = I2S_COMM_FORMAT_I2S,
#endif
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count    = 6,
        .dma_buf_len      = 60,
    };
    if (mode == MODE_MIC) {
        i2s_config.mode =
            (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    } else {
        i2s_config.mode     = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        i2s_config.use_apll = false;
        i2s_config.tx_desc_auto_clear = true;
    }

    Serial.println("Init i2s_driver_install");

    err += i2s_driver_install(SPEAK_I2S_NUMBER, &i2s_config, 0, NULL);
    i2s_pin_config_t tx_pin_config;

#if (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 3, 0))
    tx_pin_config.mck_io_num = I2S_PIN_NO_CHANGE;
#endif
    tx_pin_config.bck_io_num   = CONFIG_I2S_BCK_PIN;
    tx_pin_config.ws_io_num    = CONFIG_I2S_LRCK_PIN;
    tx_pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
    tx_pin_config.data_in_num  = CONFIG_I2S_DATA_IN_PIN;

    Serial.println("Init i2s_set_pin");
    err += i2s_set_pin(SPEAK_I2S_NUMBER, &tx_pin_config);
    Serial.println("Init i2s_set_clk");
    err += i2s_set_clk(SPEAK_I2S_NUMBER, (mode == MODE_MIC ? 16000 : 88200), I2S_BITS_PER_SAMPLE_16BIT,
                       I2S_CHANNEL_MONO);

    return true;
}

BaiduRest rest;
static uint8_t microphonedata0[1024 * 70];
size_t byte_read = 0;
int16_t *buffptr;
uint32_t data_offset = 0;

void setup() {
    M5.begin(true, false, true);
    M5.dis.clear();

    Serial.println("Init Speaker");
    InitI2SSpeakOrMic(MODE_SPK);
    delay(100);

    Serial.println("Connecting Wifi");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WifiSSID, WifiPWD);

    M5.dis.drawpix(0, CRGB(0, 128, 0));

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
    }

    M5.dis.drawpix(0, CRGB(128, 0, 0));

    Serial.println("Connected.");

    rest.settoken("500291857fbc58d4336dbe4e30d49797");
}

#define DATA_SIZE 1024

String SpeakStr;
bool Speakflag = false;
HTTPClient http;

String urlEncode(String str) {
  String encoded = "";
  char c;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else {
      encoded += '%';
      if (c < 16) encoded += '0'; // Add leading zero for single hex digit
      encoded += String(c, HEX);
    }
  }
  return encoded;
}

int http_send(String speech) {
    Serial.print("[HTTP] begin...\n");
    String queryParams = "?tts=" + urlEncode(speech) ;
    //String url =  "https://httpbin.org/get" ;
    String url =  "http://rasppi5-8g.local:1880/voice" ;

    http.begin(url + queryParams);  // configure traged server and
                                            // url.  配置被跟踪的服务器和URL
    Serial.print("[HTTP] GET...\n");
    int httpCode = http.GET();  // start connection and send HTTP header.
                                // 开始连接服务器并发送HTTP的标头
    if (httpCode >
        0) {  // httpCode will be negative on error.  出错时httpCode将为负值
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        if (httpCode ==
            HTTP_CODE_OK) {  // file found at server.  在服务器上找到文件
            String payload = http.getString();
            Serial.println(payload);  // 打印在服务器上读取的文件.  Print
                                      //  files read on the server
        }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n",
                      http.errorToString(httpCode).c_str());
  }

  http.end();

  delay(500);
  return httpCode ;
}
void loop() {

    if (M5.Btn.isPressed()) {
        data_offset = 0;
        Speakflag   = false;
        InitI2SSpeakOrMic(MODE_MIC);
        M5.dis.drawpix(0, CRGB(128, 128, 0));
        while (1) {
            i2s_read(SPEAK_I2S_NUMBER, (char *)(microphonedata0 + data_offset),
                     DATA_SIZE, &byte_read, (100 / portTICK_RATE_MS));
            data_offset += 1024;
            M5.update();
            if (M5.Btn.isReleased() || data_offset >= 71679) break;
            // delay(60);
        }
        Serial.println("end");
        int status = 0 ;
        if (rest.Pcm2String(microphonedata0, data_offset, DEV_PID_ENGLISH,
                            &SpeakStr) != -1) {
            Serial.println(SpeakStr);
            status = http_send(SpeakStr) ;
            Speakflag = true;
            M5.dis.drawpix(0, CRGB(128, 0, 128));
        } else {
            M5.dis.drawpix(0, CRGB(0, 128, 0));
        }
        InitI2SSpeakOrMic(MODE_SPK);
        switch(status) {
          case 200:
            playBeep(440, 500);
            break ;
          default:
            playBeep(400, 200);
            delay(200);
            playBeep(300, 400);
            break ;
        }

    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wifi reconnect");
        WiFi.reconnect();
        while (WiFi.status() != WL_CONNECTED) {
            delay(100);
        }
    }
    M5.update();
    delay(100);
}
