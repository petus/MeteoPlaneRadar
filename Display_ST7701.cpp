// =============================================================================
//  MeteoPlaneRadar
//  ST7701 display driver (RGB panel + SPI init sequence).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "Display_ST7701.h"
#include "TCA9554.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

esp_lcd_panel_handle_t panel_handle = NULL;
static spi_device_handle_t s_spi = NULL;

// VSYNC gate: given from the panel's VSYNC ISR, waited on by LCD_Flush so the
// full-frame copy always starts at the top of a frame (see LCD_Flush).
static SemaphoreHandle_t s_vsyncSem = NULL;

static bool IRAM_ATTR lcd_on_vsync(esp_lcd_panel_handle_t panel,
                                   const esp_lcd_rgb_panel_event_data_t* edata,
                                   void* user_ctx) {
  (void)panel; (void)edata; (void)user_ctx;
  BaseType_t hp = pdFALSE;
  if (s_vsyncSem) xSemaphoreGiveFromISR(s_vsyncSem, &hp);
  return hp == pdTRUE;
}

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

// Register init sequence - exactly as in the proven Waveshare demo for this board.
static void ST7701_SendInit() {
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

void ST7701_Init() {
  ST7701_Reset();

  // SPI bus for the init sequence (no CS - that is handled via EXIO3).
  spi_bus_config_t buscfg = {};
  buscfg.mosi_io_num = LCD_MOSI_PIN;
  buscfg.miso_io_num = -1;
  buscfg.sclk_io_num = LCD_CLK_PIN;
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  buscfg.max_transfer_sz = 64;
  spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

  spi_device_interface_config_t devcfg = {};
  devcfg.command_bits = 1;
  devcfg.address_bits = 8;
  devcfg.mode = 0;
  devcfg.clock_speed_hz = 40 * 1000 * 1000;
  devcfg.spics_io_num = -1;
  devcfg.queue_size = 1;
  spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);

  ST7701_SendInit();

  // RGB panel. Framebuffer in PSRAM, single FB + bounce buffers.
  //
  // CRITICAL anti-flicker combo: num_fbs=1 AND bounce buffers. num_fbs=2
  // (double_fb) and bounce_buffer_size are MUTUALLY EXCLUSIVE driver modes;
  // the original code enabled BOTH, which causes tearing / random-pixel
  // flicker on this panel. Bounce buffers let the RGB DMA ride out short
  // PSRAM-bus stalls caused by the canvas flush and network buffers.
  esp_lcd_rgb_panel_config_t rgb = {};
  rgb.clk_src = LCD_CLK_SRC_DEFAULT;
  rgb.timings.pclk_hz = RGB_FREQ_HZ;
  rgb.timings.h_res = LCD_WIDTH;
  rgb.timings.v_res = LCD_HEIGHT;
  rgb.timings.hsync_pulse_width = RGB_HPW;
  rgb.timings.hsync_back_porch  = RGB_HBP;
  rgb.timings.hsync_front_porch = RGB_HFP;
  rgb.timings.vsync_pulse_width = RGB_VPW;
  rgb.timings.vsync_back_porch  = RGB_VBP;
  rgb.timings.vsync_front_porch = RGB_VFP;
  rgb.timings.flags.pclk_active_neg = false;
  rgb.data_width = 16;
  rgb.bits_per_pixel = 16;
  rgb.num_fbs = 1;                               // single framebuffer...
  rgb.bounce_buffer_size_px = 10 * LCD_WIDTH;    // ...paired with bounce buffers
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
  // NOTE: double_fb intentionally NOT set (it forces num_fbs=2 and conflicts
  // with the bounce buffers above - that combination was the flicker source).

  esp_lcd_new_rgb_panel(&rgb, &panel_handle);
  esp_lcd_panel_reset(panel_handle);
  esp_lcd_panel_init(panel_handle);

  // Register a VSYNC callback so LCD_Flush can synchronise the frame copy to the
  // start of a scan-out cycle (removes the mid-screen tearing band).
  s_vsyncSem = xSemaphoreCreateBinary();
  esp_lcd_rgb_panel_event_callbacks_t cbs = {};
  cbs.on_vsync = lcd_on_vsync;
  esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL);
}

void LCD_DrawBitmap(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t* color) {
  // esp_lcd treats x_end/y_end as exclusive, hence the +1.
  esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, color);
}

void LCD_Flush(const uint16_t* fb) {
  // One big transfer: whole canvas -> panel framebuffer (single bulk read on the
  // PSRAM bus instead of 230k tiny pixel writes).
  //
  // Wait for the next VSYNC first. With num_fbs=1 the copy writes into the very
  // framebuffer the RGB DMA is scanning out. If the copy STARTS while the panel
  // is already scanning the middle of the screen, the write pointer overtakes
  // the read pointer around y=240 -> a flickering band there, most visible on
  // high-contrast content (an aircraft or a rain cloud on the left/centre).
  // Starting the copy right at VSYNC keeps the write ahead of the scan-out for
  // the whole frame, so they never cross and the band disappears.
  if (s_vsyncSem) {
    xSemaphoreTake(s_vsyncSem, 0);                    // drop any stale event
    xSemaphoreTake(s_vsyncSem, pdMS_TO_TICKS(100));   // block until the next VSYNC
  }
  esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, (void*)fb);
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
