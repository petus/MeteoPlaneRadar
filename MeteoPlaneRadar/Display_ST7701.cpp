// =============================================================================
//  MeteoPlaneRadar
//  ST7701 display driver (RGB panel + SPI init sequence).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Boards:  Waveshare ESP32-S3-Touch-LCD-2.1  (ST7701 + CST820)
//           Waveshare ESP32-S3-Touch-LCD-2.8C (ST7701 + GT911)
//           Same pins, same resolution, different panel: the register
//           sequence and the vertical timing are picked from Board_Model().
// =============================================================================
#include "Display_ST7701.h"
#include "TCA9554.h"
#include "Board.h"
#include "Config.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

esp_lcd_panel_handle_t panel_handle = NULL;
static spi_device_handle_t s_spi = NULL;

// VSYNC gate: given from the panel's VSYNC ISR, waited on by LCD_Flush so the
// full-frame copy always starts at the top of a frame (see LCD_Flush).
static SemaphoreHandle_t s_vsyncSem = NULL;

// Frames actually scanned out by the panel. This is the only honest proof that
// the display is still alive: it is incremented by the panel's own interrupt,
// so it keeps rising even if the sketch draws nothing, and it stops dead the
// moment the RGB peripheral gives up - which is exactly the failure we are
// after (lit backlight, black picture, sketch running happily).
static volatile uint32_t s_vsyncCount = 0;

static bool IRAM_ATTR lcd_on_vsync(esp_lcd_panel_handle_t panel,
                                   const esp_lcd_rgb_panel_event_data_t* edata,
                                   void* user_ctx) {
  (void)panel; (void)edata; (void)user_ctx;
  s_vsyncCount++;
  BaseType_t hp = pdFALSE;
  if (s_vsyncSem) xSemaphoreGiveFromISR(s_vsyncSem, &hp);
  return hp == pdTRUE;
}

uint32_t LCD_VsyncCount() { return s_vsyncCount; }

// --- ST7701 command/data over SPI (command_bits=1, address_bits=8) ---
static void ST7701_Cmd(uint8_t cmd) {
  spi_transaction_t t = {};
  t.cmd = 0;        // 0 = command
  t.addr = cmd;
  t.length = 0;
  spi_device_transmit(s_spi, &t);
}
static void ST7701_Dat(uint8_t data) {
  spi_transaction_t t = {};
  t.cmd = 1;        // 1 = data
  t.addr = data;
  t.length = 0;
  spi_device_transmit(s_spi, &t);
}

static void ST7701_CS_En()  { TCA9554_SetPin(EXIO_LCD_CS, false); vTaskDelay(pdMS_TO_TICKS(10)); }
static void ST7701_CS_Dis() { TCA9554_SetPin(EXIO_LCD_CS, true);  vTaskDelay(pdMS_TO_TICKS(10)); }

static void ST7701_Reset() {
  TCA9554_SetPin(EXIO_LCD_RST, false);
  vTaskDelay(pdMS_TO_TICKS(10));
  TCA9554_SetPin(EXIO_LCD_RST, true);
  vTaskDelay(pdMS_TO_TICKS(50));
}

// --- 2.1: register init sequence, exactly as in the proven Waveshare demo ----
static void ST7701_SendInit_2_1() {
  ST7701_CS_En();

  ST7701_Cmd(0xFF); ST7701_Dat(0x77); ST7701_Dat(0x01); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x10);
  ST7701_Cmd(0xC0); ST7701_Dat(0x3B); ST7701_Dat(0x00);
  ST7701_Cmd(0xC1); ST7701_Dat(0x0B); ST7701_Dat(0x02);
  ST7701_Cmd(0xC2); ST7701_Dat(0x07); ST7701_Dat(0x02);
  ST7701_Cmd(0xCC); ST7701_Dat(0x10);
  ST7701_Cmd(0xCD); ST7701_Dat(0x08);

  ST7701_Cmd(0xB0);
  ST7701_Dat(0x00); ST7701_Dat(0x11); ST7701_Dat(0x16); ST7701_Dat(0x0e); ST7701_Dat(0x11); ST7701_Dat(0x06);
  ST7701_Dat(0x05); ST7701_Dat(0x09); ST7701_Dat(0x08); ST7701_Dat(0x21); ST7701_Dat(0x06); ST7701_Dat(0x13);
  ST7701_Dat(0x10); ST7701_Dat(0x29); ST7701_Dat(0x31); ST7701_Dat(0x18);

  ST7701_Cmd(0xB1);
  ST7701_Dat(0x00); ST7701_Dat(0x11); ST7701_Dat(0x16); ST7701_Dat(0x0e); ST7701_Dat(0x11); ST7701_Dat(0x07);
  ST7701_Dat(0x05); ST7701_Dat(0x09); ST7701_Dat(0x09); ST7701_Dat(0x21); ST7701_Dat(0x05); ST7701_Dat(0x13);
  ST7701_Dat(0x11); ST7701_Dat(0x2a); ST7701_Dat(0x31); ST7701_Dat(0x18);

  ST7701_Cmd(0xFF); ST7701_Dat(0x77); ST7701_Dat(0x01); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x11);
  ST7701_Cmd(0xB0); ST7701_Dat(0x6d);
  ST7701_Cmd(0xB1); ST7701_Dat(0x37);
  ST7701_Cmd(0xB2); ST7701_Dat(0x81);
  ST7701_Cmd(0xB3); ST7701_Dat(0x80);
  ST7701_Cmd(0xB5); ST7701_Dat(0x43);
  ST7701_Cmd(0xB7); ST7701_Dat(0x85);
  ST7701_Cmd(0xB8); ST7701_Dat(0x20);
  ST7701_Cmd(0xC1); ST7701_Dat(0x78);
  ST7701_Cmd(0xC2); ST7701_Dat(0x78);
  ST7701_Cmd(0xD0); ST7701_Dat(0x88);

  ST7701_Cmd(0xE0); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x02);
  ST7701_Cmd(0xE1);
  ST7701_Dat(0x03); ST7701_Dat(0xA0); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x04); ST7701_Dat(0xA0);
  ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x20); ST7701_Dat(0x20);
  ST7701_Cmd(0xE2);
  ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00);
  ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00);
  ST7701_Dat(0x00);
  ST7701_Cmd(0xE3); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x11); ST7701_Dat(0x00);
  ST7701_Cmd(0xE4); ST7701_Dat(0x22); ST7701_Dat(0x00);
  ST7701_Cmd(0xE5);
  ST7701_Dat(0x05); ST7701_Dat(0xEC); ST7701_Dat(0xA0); ST7701_Dat(0xA0); ST7701_Dat(0x07); ST7701_Dat(0xEE);
  ST7701_Dat(0xA0); ST7701_Dat(0xA0); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00);
  ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00);
  ST7701_Cmd(0xE6); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x11); ST7701_Dat(0x00);
  ST7701_Cmd(0xE7); ST7701_Dat(0x22); ST7701_Dat(0x00);
  ST7701_Cmd(0xE8);
  ST7701_Dat(0x06); ST7701_Dat(0xED); ST7701_Dat(0xA0); ST7701_Dat(0xA0); ST7701_Dat(0x08); ST7701_Dat(0xEF);
  ST7701_Dat(0xA0); ST7701_Dat(0xA0); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00);
  ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00);
  ST7701_Cmd(0xEB);
  ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x40); ST7701_Dat(0x40); ST7701_Dat(0x00); ST7701_Dat(0x00);
  ST7701_Dat(0x00);
  ST7701_Cmd(0xED);
  ST7701_Dat(0xFF); ST7701_Dat(0xFF); ST7701_Dat(0xFF); ST7701_Dat(0xBA); ST7701_Dat(0x0A); ST7701_Dat(0xBF);
  ST7701_Dat(0x45); ST7701_Dat(0xFF); ST7701_Dat(0xFF); ST7701_Dat(0x54); ST7701_Dat(0xFB); ST7701_Dat(0xA0);
  ST7701_Dat(0xAB); ST7701_Dat(0xFF); ST7701_Dat(0xFF); ST7701_Dat(0xFF);
  ST7701_Cmd(0xEF); ST7701_Dat(0x10); ST7701_Dat(0x0D); ST7701_Dat(0x04); ST7701_Dat(0x08); ST7701_Dat(0x3F); ST7701_Dat(0x1F);

  ST7701_Cmd(0xFF); ST7701_Dat(0x77); ST7701_Dat(0x01); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x13);
  ST7701_Cmd(0xEF); ST7701_Dat(0x08);
  ST7701_Cmd(0xFF); ST7701_Dat(0x77); ST7701_Dat(0x01); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00);

  ST7701_Cmd(0x36); ST7701_Dat(0x00);
  ST7701_Cmd(0x3A); ST7701_Dat(0x66);   // RGB666/565
  ST7701_Cmd(0x11);                      // sleep out
  vTaskDelay(pdMS_TO_TICKS(480));
  ST7701_Cmd(0x20);                      // display inversion off
  vTaskDelay(pdMS_TO_TICKS(120));
  ST7701_Cmd(0x29);                      // display on
  ST7701_CS_Dis();
}

// --- 2.8C: a different panel, not a variation on the one above --------------
// Transcribed from Waveshare's board support for the 2.8C (the vendor init in
// esp-arduino-libs/ESP32_Display_Panel). Almost nothing carries over: the gamma
// tables (B0/B1), the power settings (C1/C2, B0/B1/B5 on page 0x11) and the
// whole E0-ED block are all different values, the 0x13 page is written first
// rather than last, sleep-out comes before the pixel format instead of after,
// and it ends with 0x35 (tearing effect on) where the 2.1 sends 0x20 (inversion
// off). Do not try to merge the two - they only look alike.
static void ST7701_SendInit_2_8C() {
  ST7701_CS_En();

  ST7701_Cmd(0xFF); ST7701_Dat(0x77); ST7701_Dat(0x01); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x13);
  ST7701_Cmd(0xEF); ST7701_Dat(0x08);

  ST7701_Cmd(0xFF); ST7701_Dat(0x77); ST7701_Dat(0x01); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x10);
  ST7701_Cmd(0xC0); ST7701_Dat(0x3B); ST7701_Dat(0x00);
  ST7701_Cmd(0xC1); ST7701_Dat(0x10); ST7701_Dat(0x0C);
  ST7701_Cmd(0xC2); ST7701_Dat(0x07); ST7701_Dat(0x0A);
  ST7701_Cmd(0xC7); ST7701_Dat(0x00);
  ST7701_Cmd(0xCC); ST7701_Dat(0x10);
  ST7701_Cmd(0xCD); ST7701_Dat(0x08);

  ST7701_Cmd(0xB0);
  ST7701_Dat(0x05); ST7701_Dat(0x12); ST7701_Dat(0x98); ST7701_Dat(0x0E); ST7701_Dat(0x0F); ST7701_Dat(0x07);
  ST7701_Dat(0x07); ST7701_Dat(0x09); ST7701_Dat(0x09); ST7701_Dat(0x23); ST7701_Dat(0x05); ST7701_Dat(0x52);
  ST7701_Dat(0x0F); ST7701_Dat(0x67); ST7701_Dat(0x2C); ST7701_Dat(0x11);

  ST7701_Cmd(0xB1);
  ST7701_Dat(0x0B); ST7701_Dat(0x11); ST7701_Dat(0x97); ST7701_Dat(0x0C); ST7701_Dat(0x12); ST7701_Dat(0x06);
  ST7701_Dat(0x06); ST7701_Dat(0x08); ST7701_Dat(0x08); ST7701_Dat(0x22); ST7701_Dat(0x03); ST7701_Dat(0x51);
  ST7701_Dat(0x11); ST7701_Dat(0x66); ST7701_Dat(0x2B); ST7701_Dat(0x0F);

  ST7701_Cmd(0xFF); ST7701_Dat(0x77); ST7701_Dat(0x01); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x11);
  ST7701_Cmd(0xB0); ST7701_Dat(0x5D);
  ST7701_Cmd(0xB1); ST7701_Dat(0x3E);
  ST7701_Cmd(0xB2); ST7701_Dat(0x81);
  ST7701_Cmd(0xB3); ST7701_Dat(0x80);
  ST7701_Cmd(0xB5); ST7701_Dat(0x4E);
  ST7701_Cmd(0xB7); ST7701_Dat(0x85);
  ST7701_Cmd(0xB8); ST7701_Dat(0x20);
  ST7701_Cmd(0xC1); ST7701_Dat(0x78);
  ST7701_Cmd(0xC2); ST7701_Dat(0x78);
  ST7701_Cmd(0xD0); ST7701_Dat(0x88);

  ST7701_Cmd(0xE0); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x02);
  ST7701_Cmd(0xE1);
  ST7701_Dat(0x06); ST7701_Dat(0x30); ST7701_Dat(0x08); ST7701_Dat(0x30); ST7701_Dat(0x05); ST7701_Dat(0x30);
  ST7701_Dat(0x07); ST7701_Dat(0x30); ST7701_Dat(0x00); ST7701_Dat(0x33); ST7701_Dat(0x33);
  ST7701_Cmd(0xE2);
  ST7701_Dat(0x11); ST7701_Dat(0x11); ST7701_Dat(0x33); ST7701_Dat(0x33); ST7701_Dat(0xF4); ST7701_Dat(0x00);
  ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0xF4); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00);
  ST7701_Cmd(0xE3); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x11); ST7701_Dat(0x11);
  ST7701_Cmd(0xE4); ST7701_Dat(0x44); ST7701_Dat(0x44);
  ST7701_Cmd(0xE5);
  ST7701_Dat(0x0D); ST7701_Dat(0xF5); ST7701_Dat(0x30); ST7701_Dat(0xF0); ST7701_Dat(0x0F); ST7701_Dat(0xF7);
  ST7701_Dat(0x30); ST7701_Dat(0xF0); ST7701_Dat(0x09); ST7701_Dat(0xF1); ST7701_Dat(0x30); ST7701_Dat(0xF0);
  ST7701_Dat(0x0B); ST7701_Dat(0xF3); ST7701_Dat(0x30); ST7701_Dat(0xF0);
  ST7701_Cmd(0xE6); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x11); ST7701_Dat(0x11);
  ST7701_Cmd(0xE7); ST7701_Dat(0x44); ST7701_Dat(0x44);
  ST7701_Cmd(0xE8);
  ST7701_Dat(0x0C); ST7701_Dat(0xF4); ST7701_Dat(0x30); ST7701_Dat(0xF0); ST7701_Dat(0x0E); ST7701_Dat(0xF6);
  ST7701_Dat(0x30); ST7701_Dat(0xF0); ST7701_Dat(0x08); ST7701_Dat(0xF0); ST7701_Dat(0x30); ST7701_Dat(0xF0);
  ST7701_Dat(0x0A); ST7701_Dat(0xF2); ST7701_Dat(0x30); ST7701_Dat(0xF0);
  ST7701_Cmd(0xE9); ST7701_Dat(0x36); ST7701_Dat(0x01);
  ST7701_Cmd(0xEB);
  ST7701_Dat(0x00); ST7701_Dat(0x01); ST7701_Dat(0xE4); ST7701_Dat(0xE4); ST7701_Dat(0x44); ST7701_Dat(0x88);
  ST7701_Dat(0x40);
  ST7701_Cmd(0xED);
  ST7701_Dat(0xFF); ST7701_Dat(0x10); ST7701_Dat(0xAF); ST7701_Dat(0x76); ST7701_Dat(0x54); ST7701_Dat(0x2B);
  ST7701_Dat(0xCF); ST7701_Dat(0xFF); ST7701_Dat(0xFF); ST7701_Dat(0xFC); ST7701_Dat(0xB2); ST7701_Dat(0x45);
  ST7701_Dat(0x67); ST7701_Dat(0xFA); ST7701_Dat(0x01); ST7701_Dat(0xFF);
  ST7701_Cmd(0xEF); ST7701_Dat(0x08); ST7701_Dat(0x08); ST7701_Dat(0x08); ST7701_Dat(0x45); ST7701_Dat(0x3F); ST7701_Dat(0x54);

  ST7701_Cmd(0xFF); ST7701_Dat(0x77); ST7701_Dat(0x01); ST7701_Dat(0x00); ST7701_Dat(0x00); ST7701_Dat(0x00);

  ST7701_Cmd(0x11);                      // sleep out
  vTaskDelay(pdMS_TO_TICKS(120));
  ST7701_Cmd(0x3A); ST7701_Dat(0x66);    // RGB666/565
  ST7701_Cmd(0x36); ST7701_Dat(0x00);
  ST7701_Cmd(0x35); ST7701_Dat(0x00);    // tearing effect line on
  ST7701_Cmd(0x29);                      // display on
  ST7701_CS_Dis();
}

static void ST7701_SendInit() {
  if (Board_Model() == BOARD_LCD_2_8C) ST7701_SendInit_2_8C();
  else                                 ST7701_SendInit_2_1();
}

bool ST7701_Init() {
  ST7701_Reset();

  // SPI bus for the init sequence (no CS - that is handled via EXIO3).
  spi_bus_config_t buscfg = {};
  buscfg.mosi_io_num = LCD_MOSI_PIN;
  buscfg.miso_io_num = -1;
  buscfg.sclk_io_num = LCD_CLK_PIN;
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  buscfg.max_transfer_sz = 64;
  esp_err_t err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    Serial.printf("FATAL: spi_bus_initialize selhalo (0x%x)\n", err);
    return false;
  }

  spi_device_interface_config_t devcfg = {};
  devcfg.command_bits = 1;
  devcfg.address_bits = 8;
  devcfg.mode = 0;
  devcfg.clock_speed_hz = 40 * 1000 * 1000;
  devcfg.spics_io_num = -1;
  devcfg.queue_size = 1;
  err = spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);
  if (err != ESP_OK) {
    Serial.printf("FATAL: spi_bus_add_device selhalo (0x%x)\n", err);
    return false;
  }

  ST7701_SendInit();

  // RGB panel. TWO framebuffers in PSRAM *and* bounce buffers - both at once.
  //
  // They are NOT mutually exclusive (an earlier comment here claimed they were;
  // the IDF source shows the bounce refill simply follows cur_fb_index). Each
  // solves a different problem and we need both:
  //
  //   num_fbs = 2   -> the UI is drawn into the framebuffer that is not on
  //                    screen, so a redraw can never tear the visible image.
  //   bounce bufs   -> the DMA is fed from small buffers in internal SRAM
  //                    instead of reading PSRAM directly, so it survives a
  //                    busy PSRAM bus. Without them the whole picture flickers.
  //
  // Constraint from the driver: fb_size must be divisible by 2 * bounce size.
  // 460800 / (2 * 9600) = 24, so 10 lines is a valid choice.
  esp_lcd_rgb_panel_config_t rgb = {};
  rgb.clk_src = LCD_CLK_SRC_DEFAULT;
  rgb.timings.pclk_hz = RGB_FREQ_HZ;
  rgb.timings.h_res = LCD_WIDTH;
  rgb.timings.v_res = LCD_HEIGHT;
  rgb.timings.hsync_pulse_width = RGB_HPW;
  rgb.timings.hsync_back_porch  = RGB_HBP;
  rgb.timings.hsync_front_porch = RGB_HFP;
  const bool is28c = (Board_Model() == BOARD_LCD_2_8C);
  rgb.timings.vsync_pulse_width = is28c ? RGB_VPW_2_8C : RGB_VPW_2_1;
  rgb.timings.vsync_back_porch  = is28c ? RGB_VBP_2_8C : RGB_VBP_2_1;
  rgb.timings.vsync_front_porch = RGB_VFP;
  rgb.timings.flags.pclk_active_neg = false;
  rgb.data_width = 16;
  rgb.bits_per_pixel = 16;
  rgb.num_fbs = 2;                               // double buffering (no tearing)
  rgb.bounce_buffer_size_px = 10 * LCD_WIDTH;    // steady DMA feed (no flicker)
  rgb.psram_trans_align = 64;
  rgb.hsync_gpio_num = RGB_HSYNC;
  rgb.vsync_gpio_num = RGB_VSYNC;
  rgb.de_gpio_num    = RGB_DE;
  rgb.pclk_gpio_num  = RGB_PCLK;
  rgb.disp_gpio_num  = -1;
  rgb.data_gpio_nums[0]  = RGB_D0;   rgb.data_gpio_nums[1]  = RGB_D1;
  rgb.data_gpio_nums[2]  = RGB_D2;   rgb.data_gpio_nums[3]  = RGB_D3;
  rgb.data_gpio_nums[4]  = RGB_D4;   rgb.data_gpio_nums[5]  = RGB_D5;
  rgb.data_gpio_nums[6]  = RGB_D6;   rgb.data_gpio_nums[7]  = RGB_D7;
  rgb.data_gpio_nums[8]  = RGB_D8;   rgb.data_gpio_nums[9]  = RGB_D9;
  rgb.data_gpio_nums[10] = RGB_D10;  rgb.data_gpio_nums[11] = RGB_D11;
  rgb.data_gpio_nums[12] = RGB_D12;  rgb.data_gpio_nums[13] = RGB_D13;
  rgb.data_gpio_nums[14] = RGB_D14;  rgb.data_gpio_nums[15] = RGB_D15;
  rgb.flags.fb_in_psram = true;
  // NOTE: double_fb is just an alias for num_fbs=2, which is set above.
  // NOTE: with bounce buffers the driver switches framebuffers via bb_fb_index
  // at the start of a frame, so handing over a framebuffer still works.

  // The usual reason this fails is that PSRAM is not set to OPI in the IDE:
  // two 460 kB framebuffers simply do not fit anywhere else. Without this check
  // panel_handle stays NULL and the first draw_bitmap takes the board down with
  // an unhelpful backtrace, so say it out loud instead.
  err = esp_lcd_new_rgb_panel(&rgb, &panel_handle);
  if (err != ESP_OK || !panel_handle) {
    panel_handle = nullptr;
    Serial.printf("FATAL: esp_lcd_new_rgb_panel selhalo (0x%x) - je PSRAM v IDE "
                  "nastavena na OPI PSRAM?\n", err);
    return false;
  }
  esp_lcd_panel_reset(panel_handle);
  esp_lcd_panel_init(panel_handle);

  // Register a VSYNC callback so LCD_Flush can synchronise the frame copy to the
  // start of a scan-out cycle (removes the mid-screen tearing band).
  s_vsyncSem = xSemaphoreCreateBinary();
  esp_lcd_rgb_panel_event_callbacks_t cbs = {};
  cbs.on_vsync = lcd_on_vsync;
  esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL);
  return true;
}

uint16_t* LCD_FrameBuffer(int idx) {
  if (!panel_handle || idx < 0 || idx > 1) return nullptr;
  void* fb0 = nullptr; void* fb1 = nullptr;
  if (esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 2, &fb0, &fb1) != ESP_OK) return nullptr;
  return (uint16_t*)(idx == 0 ? fb0 : fb1);
}

void LCD_Flush(const uint16_t* fb) {
  if (!panel_handle || !fb) return;   // panel init failed - nothing to draw on
  // With two framebuffers the driver recognises one of its own buffers, skips
  // the copy entirely and only repoints the DMA (verified in the IDF source:
  // draw_bitmap sets do_copy = false when the pointer matches a framebuffer).
#if FLUSH_DEBUG
  uint32_t t0 = micros();
#endif
  esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, (void*)fb);

  // The DMA only picks up the new buffer at the end of the frame it is drawing
  // right now. Wait for that boundary before returning, otherwise the caller
  // would start drawing into a buffer that is still on screen - which is the
  // very tearing we are getting rid of.
  if (s_vsyncSem) {
    xSemaphoreTake(s_vsyncSem, 0);                    // drop any stale event
    xSemaphoreTake(s_vsyncSem, pdMS_TO_TICKS(100));   // wait for the swap
  }
#if FLUSH_DEBUG
  uint32_t dt = micros() - t0;
  static uint32_t mn = 0xFFFFFFFF, mx = 0, last = 0, cnt = 0, tPrev = 0;
  if (dt < mn) mn = dt;
  if (dt > mx) mx = dt;
  last = dt; cnt++;
  uint32_t now = millis();
  if (now - tPrev >= 1000) {
    tPrev = now;
    Serial.printf("FLUSH: min=%lu us  posl=%lu us  max=%lu us  (%lu/s, snimek ~34200 us)\n",
                  (unsigned long)mn, (unsigned long)last, (unsigned long)mx,
                  (unsigned long)cnt);
    mn = 0xFFFFFFFF; mx = 0; cnt = 0;
  }
#endif
}

// --- Backlight ---
void Backlight_Init() {
  ledcAttach(LCD_BL_PIN, BL_PWM_FREQ, BL_PWM_RES);
  Set_Backlight(80);
}

void Set_Backlight(uint8_t light) {
  if (light > BL_MAX) light = BL_MAX;
  uint32_t duty = (uint32_t)light * 1023 / 100;
  ledcWrite(LCD_BL_PIN, duty);
}
