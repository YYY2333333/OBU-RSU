//静态测试使用的wifi探针程序，增加了定时器为了评估信号稳定状态，每5秒统计一次收到了多少次信号，一般为6-7次（算上屏幕显示有延迟）
//功能1：显示图片
//功能2：显示信号强度
//功能3：评估信号稳定状态
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include <Arduino.h>
#include <JPEGDecoder.h>
#include <Ticker.h>
#include <LittleFS.h>
#define JpegFS LittleFS
Ticker timer;
int count=0;
// Return the minimum of two values a and b
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

// Include the sketch header file that contains the image stored as an array of bytes
// More than one image array could be stored in each header file.
#define LED_GPIO_PIN 15
#define WIFI_CHANNEL_SWITCH_INTERVAL  (5)
#define WIFI_CHANNEL_MAX               (13)
#define LED_GPIO_PIN 15       // LED 控制引脚·

#define SD_CS        11       // TF卡片选引脚（接CS）
#define SD_MOSI      13       // SPI MOSI
#define SD_MISO      14       // SPI MISO
#define SD_SCK       12       // SPI SCK

#include <SPI.h>

#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

#define GRIDX 160
#define GRIDY 106
#define CELLXY 3

#define GEN_DELAY 0

hw_timer_t *tim1 = NULL;
int tim1_IRQ_count = 0;
uint8_t current_channel = 1;
uint8_t level = 0, channel = 1;

static wifi_country_t wifi_country = {.cc="CN", .schan = 1, .nchan = 13}; //Most recent esp32 library struct

unsigned long lastMatchedTime = 0;
bool hasMatched = false;
// 定义三个MAC地址
uint8_t aggressiveMAC[6] = {0x16, 0x98, 0x12, 0x34, 0x56, 0x44};
uint8_t moderateMAC[6] = {0x16, 0x12, 0x12, 0x34, 0x56, 0x44};
uint8_t downMAC[6] = {0x16, 0x13, 0x12, 0x34, 0x56, 0x44};

typedef struct {
  unsigned frame_ctrl:16;
  unsigned duration_id:16;
  uint8_t addr1[6]; /* receiver address */
  uint8_t addr2[6]; /* sender address */
  uint8_t addr3[6]; /* filtering address */
  unsigned sequence_ctrl:16;
  uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

static esp_err_t event_handler(void *ctx, system_event_t *event);
static void wifi_sniffer_init(void);
static void wifi_sniffer_set_channel(uint8_t channel);
static const char *wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type);
static void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type);

esp_err_t event_handler(void *ctx, system_event_t *event)
{
  return ESP_OK;
}

void drawSdJpeg(const char *filename, int xpos, int ypos) {

  // Open the named file (the Jpeg decoder library will close it)
  File jpegFile = SD.open( filename, FILE_READ);  // or, file handle reference for SD library
 
  if ( !jpegFile ) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println ("\" not found!");
    return;
  }

//  Serial.println("===========================");
//  Serial.print("Drawing file: "); Serial.println(filename);
//  Serial.println("===========================");

  // Use one of the following methods to initialise the decoder:
  bool decoded = JpegDec.decodeSdFile(jpegFile);  // Pass the SD file handle to the decoder,
  //bool decoded = JpegDec.decodeSdFile(filename);  // or pass the filename (String or character array)

  if (decoded) {
    // print information about the image to the serial port
    //jpegInfo();
    // render the image onto the screen at given coordinates
    jpegRender(xpos, ypos);
  }
  else {
    Serial.println("Jpeg file format not supported!");
  }
}
void onDataReceived(){
  count++;
}
void jpegRender(int xpos, int ypos) {

  //jpegInfo(); // Print information from the JPEG file (could comment this line out)

  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  bool swapBytes = tft.getSwapBytes();
  tft.setSwapBytes(true);
  
  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = jpg_min(mcu_w, max_x % mcu_w);
  uint32_t min_h = jpg_min(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // Fetch data from the file, decode and display
  while (JpegDec.read()) {    // While there is more data in the file
    pImg = JpegDec.pImage ;   // Decode a MCU (Minimum Coding Unit, typically a 8x8 or 16x16 pixel block)

    // Calculate coordinates of top left corner of current MCU
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w)
    {
      uint16_t *cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++)
      {
        p += mcu_w;
        for (int w = 0; w < win_w; w++)
        {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    // calculate how many pixels must be drawn
    uint32_t mcu_pixels = win_w * win_h;

    // draw image MCU block only if it will fit on the screen
    if (( mcu_x + win_w ) <= tft.width() && ( mcu_y + win_h ) <= tft.height())
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    else if ( (mcu_y + win_h) >= tft.height())
      JpegDec.abort(); // Image has run off bottom of screen so abort decoding
  }

  tft.setSwapBytes(swapBytes);

}

//####################################################################################################
// Print image information to the serial port (optional)
//####################################################################################################
// JpegDec.decodeFile(...) or JpegDec.decodeArray(...) must be called before this info is available!
void jpegInfo() {

  // Print information extracted from the JPEG file
  Serial.println("JPEG image info");
  Serial.println("===============");
  Serial.print("Width      :");
  Serial.println(JpegDec.width);
  Serial.print("Height     :");
  Serial.println(JpegDec.height);
  Serial.print("Components :");
  Serial.println(JpegDec.comps);
  Serial.print("MCU / row  :");
  Serial.println(JpegDec.MCUSPerRow);
  Serial.print("MCU / col  :");
  Serial.println(JpegDec.MCUSPerCol);
  Serial.print("Scan type  :");
  Serial.println(JpegDec.scanType);
  Serial.print("MCU width  :");
  Serial.println(JpegDec.MCUWidth);
  Serial.print("MCU height :");
  Serial.println(JpegDec.MCUHeight);
  Serial.println("===============");
  Serial.println("");
}

void wifi_sniffer_init(void)
{
  nvs_flash_init();
  tcpip_adapter_init();
  ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_country(&wifi_country) ); /* set country for channel range [1, 13] */
  ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
  ESP_ERROR_CHECK( esp_wifi_start() );
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
}

void wifi_sniffer_set_channel(uint8_t channel)
{
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

const char * wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type)
{
  switch(type) {
  case WIFI_PKT_MGMT: return "MGMT";
  case WIFI_PKT_DATA: return "DATA";
  default:  
  case WIFI_PKT_MISC: return "MISC";
  }
}

uint8_t lastMatchedMAC[6] = {0}; // 上一次匹配的MAC地址
bool hasLastMatchedMAC = false;  // 标记是否有上一次MAC


void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
  const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
  const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

  if (ppkt->rx_ctrl.sig_len > 0) {
    const uint8_t *currentMAC = hdr->addr2;

    if (memcmp(currentMAC, aggressiveMAC, 6) == 0) {
      Serial.println("显示图片: LEFT");
      printf("强度值：%02d\n", ppkt->rx_ctrl.rssi);

      // 判断是否和上次相同MAC
      if (!hasLastMatchedMAC || memcmp(currentMAC, lastMatchedMAC, 6) != 0) {
        tft.fillScreen(TFT_BLACK); 
        drawSdJpeg("/left.jpg", (tft.width() - 300) / 2 - 1, (tft.height() - 300) / 2 - 1);
        memcpy(lastMatchedMAC, currentMAC, 6);
        hasLastMatchedMAC = true;
      }
      
      onDataReceived();
      //tft.fillScreen(random(0xFFFF));
      lastMatchedTime = millis();
      hasMatched = true;
      return;

    } else if (memcmp(currentMAC, moderateMAC, 6) == 0) {
      Serial.println("显示图片: NO_STRAIGHT");
      printf("强度值：%02d\n", ppkt->rx_ctrl.rssi);

      if (!hasLastMatchedMAC || memcmp(currentMAC, lastMatchedMAC, 6) != 0) {
        tft.fillScreen(TFT_BLACK); 
        drawSdJpeg("/no_straight.jpg", (tft.width() - 300) / 2 - 1, (tft.height() - 300) / 2 - 1);
        memcpy(lastMatchedMAC, currentMAC, 6);
        hasLastMatchedMAC = true;
      }

      onDataReceived();
      //tft.fillScreen(random(0xFFFF));
      lastMatchedTime = millis();
      hasMatched = true;
      return;

    } else if (memcmp(currentMAC, downMAC, 6) == 0) {
      Serial.println("显示图片: RIGHT");
      printf("强度值：%02d\n", ppkt->rx_ctrl.rssi);

      if (!hasLastMatchedMAC || memcmp(currentMAC, lastMatchedMAC, 6) != 0) {
        tft.fillScreen(TFT_BLACK); 
        drawSdJpeg("/right.jpg", (tft.width() - 300) / 2 - 1, (tft.height() - 300) / 2 - 1);
        memcpy(lastMatchedMAC, currentMAC, 6);
        hasLastMatchedMAC = true;
      }

      onDataReceived();
      //tft.fillScreen(random(0xFFFF));
      lastMatchedTime = millis();
      hasMatched = true;
      return;
    }
  }
}
  /*if ((hdr->addr1[0]==16) && (hdr->addr1[1]==98) && (hdr->addr1[2]==12) && (hdr->addr1[3]==34) && (hdr->addr1[4]==56) && (hdr->addr1[5]==44))*/


// the setup function runs once when you press reset or power the board


//##################################################################################################################################################################################


// Count how many times the image is drawn for test purposes
uint32_t icount = 0;
//----------------------------------------------------------------------------------------------------

void(* resetFunc) (void) = 0;

void timerCallback(){
  Serial.print("5秒内ESP32接收到指定MAC地址次数为：");
  Serial.println(count);
  count = 0;
  }



void setup() {
  // initialize digital pin 5 as an output.
  Serial.begin(115200);
  delay(10);
  tft.init();
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI)) {
    Serial.println("Card Mount Failed");
    resetFunc();
    return;
  }
  tft.setRotation(3);
  wifi_sniffer_init();
  pinMode(LED_GPIO_PIN, OUTPUT);
  timer.attach(5.0,timerCallback);
}
bool imageShowing = false;
unsigned long lastImageTime = 0;
const unsigned long imageDisplayDuration = 2000;

// the loop function runs over and over again forever
void loop() {
  vTaskDelay(WIFI_CHANNEL_SWITCH_INTERVAL / portTICK_PERIOD_MS);
  wifi_sniffer_set_channel(1);
}
