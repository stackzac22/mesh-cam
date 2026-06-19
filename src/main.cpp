#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"

// ---- XIAO ESP32-S3 Sense (OV2640) camera pins ----
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 10
#define SIOD_GPIO_NUM 40
#define SIOC_GPIO_NUM 39
#define Y9_GPIO_NUM 48
#define Y8_GPIO_NUM 11
#define Y7_GPIO_NUM 12
#define Y6_GPIO_NUM 14
#define Y5_GPIO_NUM 16
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 17
#define Y2_GPIO_NUM 15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM 47
#define PCLK_GPIO_NUM 13

// ---- UART link to Heltec ----
#define UART_TX 43   // D6 -> Heltec GPIO33 (RX)
#define UART_RX 44   // D7 <- Heltec GPIO34 (TX)
#define UART_BAUD 38400

// ---- Motion tuning ----
const int      PIXEL_DELTA = 25;
const float    MOTION_FRAC = 0.05;
const uint32_t COOLDOWN_MS = 60000;
const uint32_t WARMUP_MS   = 3000;

uint8_t *prevFrame = nullptr;
size_t   prevLen   = 0;
uint32_t lastAlert = 0, bootTime = 0;
int      photoCount = 0;
String   rxLine;

// Build a camera config for a given format/size.
void makeConfig(camera_config_t &c, pixformat_t fmt, framesize_t size, int jpegQ) {
  c = {};
  c.ledc_channel = LEDC_CHANNEL_0; c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0=Y2_GPIO_NUM; c.pin_d1=Y3_GPIO_NUM; c.pin_d2=Y4_GPIO_NUM; c.pin_d3=Y5_GPIO_NUM;
  c.pin_d4=Y6_GPIO_NUM; c.pin_d5=Y7_GPIO_NUM; c.pin_d6=Y8_GPIO_NUM; c.pin_d7=Y9_GPIO_NUM;
  c.pin_xclk=XCLK_GPIO_NUM; c.pin_pclk=PCLK_GPIO_NUM;
  c.pin_vsync=VSYNC_GPIO_NUM; c.pin_href=HREF_GPIO_NUM;
  c.pin_sccb_sda=SIOD_GPIO_NUM; c.pin_sccb_scl=SIOC_GPIO_NUM;
  c.pin_pwdn=PWDN_GPIO_NUM; c.pin_reset=RESET_GPIO_NUM;
  c.xclk_freq_hz=20000000;
  c.pixel_format=fmt; c.frame_size=size;
  c.jpeg_quality=jpegQ; c.fb_count=1;
  c.fb_location=CAMERA_FB_IN_PSRAM; c.grab_mode=CAMERA_GRAB_LATEST;
}

void startMotionMode() {
  camera_config_t c; makeConfig(c, PIXFORMAT_GRAYSCALE, FRAMESIZE_QQVGA, 12);
  esp_camera_init(&c);
}

// Reconfigure to JPEG, grab one frame, save to SD, restore motion mode.
void snapToSD() {
  esp_camera_deinit();
  camera_config_t c; makeConfig(c, PIXFORMAT_JPEG, FRAMESIZE_SVGA, 10); // 800x600
  if (esp_camera_init(&c) != ESP_OK) { Serial1.print("CAM reinit failed\n"); startMotionMode(); return; }
  delay(300); // let exposure settle
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {
    String path = "/snap_" + String(photoCount++) + ".jpg";
    File f = SD_MMC.open(path, FILE_WRITE);
    if (f) { f.write(fb->buf, fb->len); f.close(); Serial1.print("SAVED " + path + "\n"); }
    else   { Serial1.print("SD write failed\n"); }
    esp_camera_fb_return(fb);
  } else { Serial1.print("snap capture failed\n"); }
  esp_camera_deinit();
  startMotionMode();
}

void handleCommand(String cmd) {
  cmd.trim(); cmd.toLowerCase();
  if (cmd.indexOf("snap") >= 0) snapToSD();
  // add more keywords here: "status", "arm", "disarm", ...
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
  if (!SD_MMC.begin("/sdcard", true)) Serial.println("SD init failed"); // true = 1-bit mode
  startMotionMode();
  bootTime = millis();
}

void loop() {
  // 1) drain inbound mesh text (one command per line)
  while (Serial1.available()) {
    char ch = Serial1.read();
    if (ch == '\n' || ch == '\r') { if (rxLine.length()) { handleCommand(rxLine); rxLine = ""; } }
    else if (rxLine.length() < 120) rxLine += ch;
  }

  // 2) motion detection
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) { delay(50); return; }
  bool motion = false;
  if (prevFrame && prevLen == fb->len) {
    uint32_t changed = 0;
    for (size_t i = 0; i < fb->len; i++)
      if (abs((int)fb->buf[i] - (int)prevFrame[i]) > PIXEL_DELTA) changed++;
    motion = (changed > fb->len * MOTION_FRAC);
  }
  if (prevLen != fb->len) { free(prevFrame); prevFrame = (uint8_t*)malloc(fb->len); prevLen = fb->len; }
  if (prevFrame) memcpy(prevFrame, fb->buf, fb->len);
  esp_camera_fb_return(fb);

  uint32_t now = millis();
  if (motion && (now - bootTime > WARMUP_MS) && (now - lastAlert > COOLDOWN_MS)) {
    Serial1.print("MOTION at cam1\n");
    lastAlert = now;
  }
  delay(100);
}
