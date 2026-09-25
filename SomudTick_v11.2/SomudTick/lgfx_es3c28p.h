#pragma once
// LovyanGFX config for ES3C28P (2.8" IPS ILI9341V 240x320 + FT6336G touch)
// Pin map from LCDWIKI "ES3C28P&ES3N28P Arduino Demo Instructions"
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ---- ถ้าจอแสดงผลผิด ปรับตรงนี้ ----
#define LCD_INVERT      true    // สีกลับด้าน (ขาวเป็นดำ) → เปลี่ยนเป็น false
#define LCD_RGB_ORDER   false   // แดงกับน้ำเงินสลับกัน → เปลี่ยนเป็น true
#define TOUCH_FLIP_X    false   // แตะซ้ายแต่ไปโดนขวา → true
#define TOUCH_FLIP_Y    false   // แตะบนแต่ไปโดนล่าง → true

#ifdef SIM
// computer simulator: the "screen" is just a picture in memory
class LGFX_ES3C28P : public LGFX_Sprite {
  uint8_t r_ = 0;
 public:
  bool init() { setColorDepth(16); return createSprite(240, 320) != nullptr; }
  void setBrightness(uint8_t) {}
  bool asleep = false;
  void sleep() { asleep = true; }
  void wakeup() { asleep = false; }
  int getTouch(lgfx::touch_point_t*, int = 1) { return 0; }
  int getTouchRaw(lgfx::touch_point_t*, int = 1) { return 0; }
  void calibrateTouch(uint16_t*, uint32_t, uint32_t, uint8_t = 10) {}
  void setTouchCalibrate(uint16_t*) {}
  void setRotation(uint8_t v) { r_ = v & 3; }
  int32_t width() const { return (r_ & 1) ? 320 : 240; }
  int32_t height() const { return (r_ & 1) ? 240 : 320; }
};
#else
class LGFX_ES3C28P : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;
  lgfx::Touch_FT5x06  _touch;   // FT6336G ใช้ไดรเวอร์ตระกูลเดียวกัน
public:
  LGFX_ES3C28P() {
    { auto c = _bus.config();
      c.spi_host = SPI2_HOST;
      c.spi_mode = 0;
      c.freq_write = 40000000;
      c.freq_read  = 16000000;
      c.spi_3wire = false;
      c.use_lock = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = 12;
      c.pin_mosi = 11;
      c.pin_miso = 13;
      c.pin_dc   = 46;
      _bus.config(c);
      _panel.setBus(&_bus);
    }
    { auto c = _panel.config();
      c.pin_cs = 10;
      c.pin_rst = -1;          // ต่อร่วมกับขา EN ของ ESP32-S3
      c.pin_busy = -1;
      c.panel_width = 240;
      c.panel_height = 320;
      c.offset_x = 0;
      c.offset_y = 0;
      c.offset_rotation = 0;
      c.readable = true;
      c.invert = LCD_INVERT;
      c.rgb_order = LCD_RGB_ORDER;
      c.dlen_16bit = false;
      c.bus_shared = false;
      _panel.config(c);
    }
    { auto c = _light.config();
      c.pin_bl = 45;
      c.invert = false;
      c.freq = 44100;
      c.pwm_channel = 7;
      _light.config(c);
      _panel.setLight(&_light);
    }
    { auto c = _touch.config();
      c.x_min = TOUCH_FLIP_X ? 239 : 0;
      c.x_max = TOUCH_FLIP_X ? 0 : 239;
      c.y_min = TOUCH_FLIP_Y ? 319 : 0;
      c.y_max = TOUCH_FLIP_Y ? 0 : 319;
      c.pin_int = 17;
      c.pin_rst = 18;
      c.bus_shared = false;
      c.offset_rotation = 0;
      c.i2c_port = 1;
      c.i2c_addr = 0x38;
      c.pin_sda = 16;
      c.pin_scl = 15;
      c.freq = 400000;
      _touch.config(c);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};
#endif  // SIM
