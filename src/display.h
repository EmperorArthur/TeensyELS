
#include "config.h"
#include "globalstate.h"

#define SSD1306_128_64 0

#if ELS_DISPLAY == SSD1306_128_64

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// some displays can have different addresses, this is what we attempt to init
#define SCREEN_ADDRESS 0x3C

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#else

#error "Please choose a valid display. Refer to config.h for options"

#endif

class Display {
 public:
#if ELS_DISPLAY == SSD1306_128_64
  Adafruit_SSD1306 m_ssd1306;
#endif
  Display() {
#if ELS_DISPLAY == SSD1306_128_64
    this->m_ssd1306 =
        Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, PIN_DISPLAY_RESET);
#endif
  }

  void init();
  void update(boolean lock);

 protected:
  void drawMode();
  void drawPitch();
  void drawEnabled();
  void drawLocked(boolean locked);
};