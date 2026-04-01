/*===========================================================================
 *  display.c — Sterownik ST7735  128x160  landscape  (ESP-IDF 5.x)
 *
 *  SPI:   SPI3_HOST (VSPI)
 *         SCLK=18, MOSI=23, CS=5, DC=2, RST=15,  27 MHz
 *
 *  Orientacja landscape 160x128:
 *    MADCTL = 0x60  (MV=1, MX=1 — obrot 90° CW)
 *
 *  Offset pikseli ST7735 (znany hardware quirk):
 *    COL_OFFSET = 2, ROW_OFFSET = 1  (typowe dla wiekszosci modulow 1.8")
 *    Jezeli obraz nadal nie pasuje — zmien te stale.
 *
 *  Transfer DMA: caly framebuffer 160*128*2 = 40960 B jednym burst'em.
 *===========================================================================*/

#include "display.h"
#include "graph.h"
#include "config.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

/*---------------------------------------------------------------------------
 *  Offset pikseli — dostosuj jesli obraz jest przesuniety
 *  Typowe wartosci dla ST7735S 1.8": COL=2, ROW=1
 *  Dla niektorych modulow: COL=0, ROW=0  lub  COL=2, ROW=3
 *--------------------------------------------------------------------------*/
#define COL_OFFSET   2
#define ROW_OFFSET   2

/*---------------------------------------------------------------------------
 *  MADCTL — orientacja wyswietlacza
 *
 *  Bit:  MY MX MV ML RGB MH - -
 *  0x00  0  0  0  0   0   = portrait normalny
 *  0x60  0  1  1  0   0   = landscape, 160 kolumn x 128 wierszy  ← uzywamy
 *  0xA0  1  0  1  0   0   = landscape odwrocony (poprzednia wartosc)
 *  0xC0  1  1  0  0   0   = portrait odwrocony 180°
 *
 *  Jezeli obraz jest odbity poziomo — zmien 0x60 na 0x20 (MX=0)
 *  Jezeli obraz jest odbity pionowo  — zmien 0x60 na 0xE0 (MY=1,MX=1,MV=1)
 *--------------------------------------------------------------------------*/
#define MADCTL_VAL   0xC0  /* MY=1 MX=1 MV=0 = portrait odwrocony, RGB — jak oryginal */

#define SPI_HOST_DEV    SPI3_HOST

static spi_device_handle_t s_spi = NULL;

/*---------------------------------------------------------------------------
 *  Niskopoziomowe funkcje SPI
 *--------------------------------------------------------------------------*/
static void dc_cmd(void)  { gpio_set_level(DISP_DC, 0); }
static void dc_data(void) { gpio_set_level(DISP_DC, 1); }

void display_command(uint8_t cmd)
{
    dc_cmd();
    spi_transaction_t t = { .length = 8, .tx_buffer = &cmd };
    spi_device_transmit(s_spi, &t);
    dc_data();
}

static void send_data(const uint8_t *buf, size_t len)
{
    if (!len) return;
    spi_transaction_t t = { .length = len * 8, .tx_buffer = buf };
    spi_device_transmit(s_spi, &t);
}

static void cmd_data(uint8_t cmd, const uint8_t *data, size_t dlen)
{
    display_command(cmd);
    if (dlen) send_data(data, dlen);
}

/*===========================================================================
 *  display_init
 *===========================================================================*/
esp_err_t display_init(void)
{
    /* GPIO dla DC i RST */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << DISP_DC) | (1ULL << DISP_RST),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    /* Magistrala SPI */
    spi_bus_config_t bus = {
        .mosi_io_num     = DISP_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = DISP_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = NX * NY * 2 + 16,
    };
    esp_err_t err = spi_bus_initialize(SPI_HOST_DEV, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_DISP, "spi_bus_initialize: %s", esp_err_to_name(err));
        return err;
    }

    /* Urzadzenie ST7735 */
    spi_device_interface_config_t dev = {
        .clock_speed_hz = DISP_SPI_FREQ,
        .mode           = 0,
        .spics_io_num   = DISP_CS,
        .queue_size     = 7,
    };
    err = spi_bus_add_device(SPI_HOST_DEV, &dev, &s_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_DISP, "spi_bus_add_device: %s", esp_err_to_name(err));
        return err;
    }

    /* Hardware Reset */
    gpio_set_level(DISP_RST, 1); vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(DISP_RST, 0); vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(DISP_RST, 1); vTaskDelay(pdMS_TO_TICKS(120));
    dc_data();

    /* Inicjalizacja rejestrow ST7735 */
    display_command(0x01);              /* Software reset */
    vTaskDelay(pdMS_TO_TICKS(150));
    display_command(0x11);              /* Sleep out */
    vTaskDelay(pdMS_TO_TICKS(300));

    cmd_data(0xB1, (uint8_t[]){0x01,0x2C,0x2D}, 3);  /* Frame rate normal */
    cmd_data(0xB2, (uint8_t[]){0x01,0x2C,0x2D}, 3);  /* Frame rate idle */
    cmd_data(0xB3, (uint8_t[]){0x01,0x2C,0x2D,
                                0x01,0x2C,0x2D}, 6);  /* Frame rate partial */
    cmd_data(0xB4, (uint8_t[]){0x07}, 1);              /* Column inversion */

    cmd_data(0xC0, (uint8_t[]){0xA2,0x02,0x84}, 3);
    cmd_data(0xC1, (uint8_t[]){0xC5}, 1);
    cmd_data(0xC2, (uint8_t[]){0x0A,0x00}, 2);
    cmd_data(0xC3, (uint8_t[]){0x8A,0x2A}, 2);
    cmd_data(0xC4, (uint8_t[]){0x8A,0xEE}, 2);
    cmd_data(0xC5, (uint8_t[]){0x0E}, 1);              /* VCOM */

    display_command(0x20);              /* Display inversion off */

    /* Orientacja landscape */
    cmd_data(0x36, (uint8_t[]){MADCTL_VAL}, 1);

    /* Kolor 16-bit RGB565 */
    cmd_data(0x3A, (uint8_t[]){0x05}, 1);

    /*
     *  Okno zapisu z offsetem (ST7735 hardware quirk)
     *  W landscape: kolumny = YW (160), wiersze = XW (128)
     *  Okno: col 0+offset .. NX-1+offset,  row 0+offset .. NY-1+offset
     */
    /* Okno portrait (jak oryginal): 2A=kolumny 0..127, 2B=wiersze 0..159 */
    cmd_data(0x2A, (uint8_t[]){
        0x00, COL_OFFSET,
        0x00, (uint8_t)(XW - 1 + COL_OFFSET)}, 4);
    cmd_data(0x2B, (uint8_t[]){
        0x00, ROW_OFFSET,
        0x00, (uint8_t)(YW - 1 + ROW_OFFSET)}, 4);

    /* Gamma */
    cmd_data(0xE0, (uint8_t[]){
        0x02,0x1C,0x07,0x12,0x37,0x32,0x29,0x2D,
        0x29,0x25,0x2B,0x39,0x00,0x01,0x03,0x10}, 16);
    cmd_data(0xE1, (uint8_t[]){
        0x03,0x1D,0x07,0x06,0x2E,0x2C,0x29,0x2D,
        0x2E,0x2E,0x37,0x3F,0x00,0x00,0x02,0x10}, 16);

    display_command(0x13);              /* Normal display on */
    vTaskDelay(pdMS_TO_TICKS(10));
    display_command(0x29);              /* Display on */
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG_DISP,
             "ST7735 OK — landscape %dx%d, SPI %d MHz, "
             "MADCTL=0x%02X, offset col=%d row=%d",
             NX, NY, DISP_SPI_FREQ / 1000000,
             MADCTL_VAL, COL_OFFSET, ROW_OFFSET);

    /* Podswietlenie PWM przez LEDC */
    ledc_timer_config_t bl_timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_BL,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz         = LEDC_FREQ_BL,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&bl_timer);
    ledc_channel_config_t bl_ch = {
        .gpio_num   = DISP_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_BL,
        .timer_sel  = LEDC_TIMER_BL,
        .duty       = (BRIGHTNESS_DEFAULT * 255) / 100,
        .hpoint     = 0,
    };
    ledc_channel_config(&bl_ch);

    return ESP_OK;
}

/*===========================================================================
 *  display_trans65k — RGB888 → RGB565 big-endian
 *===========================================================================*/
void display_trans65k(void)
{
    for (int x = 0; x < NX; x++) {
        for (int y = 0; y < NY; y++) {
            uint16_t c = ((uint16_t)(R_GRAM[x][y] & 0xF8) << 8)
                       | ((uint16_t)(G_GRAM[x][y] & 0xFC) << 3)
                       | ((uint16_t)(B_GRAM[x][y]        ) >> 3);
            GRAM65k[x][y] = (c >> 8) | (c << 8);   /* big-endian swap */
        }
    }
}

/*===========================================================================
 *  display_transfer_image — DMA burst calego framebuffera
 *===========================================================================*/
static uint16_t DRAM_ATTR s_dma_buf[NX * NY];

void display_transfer_image(void)
{
    /* Transfer kolumnowy — identyczny z oryginaem Arduino:
     * xp=pt/NY, yp=pt%NY  ->  s_dma_buf[x*NY + y] = GRAM65k[x][y]
     * ST7735 w portrait odbiera 128 kolumn x 160 wierszy,
     * dane idace kolumnami daja efekt landscape na ekranie */
    for (int x = 0; x < NX; x++)
        for (int y = 0; y < NY; y++)
            s_dma_buf[x * NY + y] = GRAM65k[x][y];

    /* Okno zapisu portrait z offsetem */
    cmd_data(0x2A, (uint8_t[]){
        0x00, COL_OFFSET,
        0x00, (uint8_t)(XW - 1 + COL_OFFSET)}, 4);
    cmd_data(0x2B, (uint8_t[]){
        0x00, ROW_OFFSET,
        0x00, (uint8_t)(YW - 1 + ROW_OFFSET)}, 4);
    display_command(0x2C);  /* Memory write */

    spi_transaction_t t = {
        .length    = NX * NY * 16,
        .tx_buffer = s_dma_buf,
    };
    spi_device_transmit(s_spi, &t);
}

/*===========================================================================
 *  display_brightness_set — ustaw jasnosc podswietlenia [%]
 *===========================================================================*/
void display_brightness_set(uint8_t pct)
{
    if (pct > 100) pct = 100;
    if (pct < BRIGHTNESS_MIN) pct = BRIGHTNESS_MIN;
    uint32_t duty = ((uint32_t)pct * 255) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_BL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_BL);
}
