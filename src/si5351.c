/*===========================================================================
 *  si5351.c — Sterownik Si5351A  (ESP-IDF 5.x)
 *
 *  Wyjscia:
 *    CLK0 (reg 16) — nosna glowna I,   PLL_A, MS0, faza 0°,  8 mA
 *    CLK1 (reg 17) — LO (VFO),         PLL_B, MS1, strojone enkoderem
 *    CLK2 (reg 18) — nosna Q,          PLL_A, MS2, INV=90°,  8 mA
 *
 *  Rejestr 3 (Output Enable Control):
 *    bit0=CLK0  bit1=CLK1  bit2=CLK2   1=wyciszony 0=aktywny
 *
 *  Strategia fixed-PLL:
 *    PLL reset tylko przy zmianie calkowitego dzielnika MS (zmiana podzakresu).
 *    Normalne strojenie enkoderem — zero resetow, zero glitchy.
 *===========================================================================*/

#include "si5351.h"
#include "config.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

/*--- stale ---*/
#define I2C_PORT        I2C_NUM_0
#define I2C_TIMEOUT_MS  10
#define SI5351_PMAX     0xFFFFFUL
#define FVCO_NOMINAL    900000000UL   /* docelowe fVCO [Hz] */

/*--- cache rejestrow (unika zbednych transakcji I2C) ---*/
#define REG_CACHE_SIZE  200
static uint8_t           s_reg_cache[REG_CACHE_SIZE];
static bool              s_cache_valid = false;

/*--- uchwyty I2C i mutex ---*/
static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;
static SemaphoreHandle_t       s_mtx = NULL;

/*--- ostatnie dzielniki MS do detekcji potrzeby resetu PLL ---*/
static uint32_t s_ms_lo  = 0;   /* CLK1 / PLL_B */
static uint32_t s_ms_car = 0;   /* CLK0+CLK2 / PLL_A */

/*--- kalibracja kwarcu [Hz], domyslnie 0 ---*/
static int32_t  s_xtal_cal = 0;

/* -------------------------------------------------------------------------
 *  I2C — zapis
 * ---------------------------------------------------------------------- */
static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    if (s_cache_valid && reg < REG_CACHE_SIZE && s_reg_cache[reg] == val)
        return ESP_OK;
    uint8_t buf[2] = { reg, val };
    esp_err_t e = i2c_master_transmit(s_dev, buf, 2, I2C_TIMEOUT_MS);
    if (e == ESP_OK && reg < REG_CACHE_SIZE) s_reg_cache[reg] = val;
    return e;
}

static void regs_write(uint8_t base, const uint8_t *d, uint8_t n)
{
    uint8_t buf[11];
    if (n > 10) n = 10;
    buf[0] = base;
    memcpy(&buf[1], d, n);
    if (i2c_master_transmit(s_dev, buf, n + 1, I2C_TIMEOUT_MS) == ESP_OK)
        for (uint8_t i = 0; i < n; i++)
            if ((base + i) < REG_CACHE_SIZE) s_reg_cache[base + i] = d[i];
}

/* -------------------------------------------------------------------------
 *  Obliczenia PLL
 *  fvco = fxtal * (a + b/c)
 *  P1 = 128*a + floor(128*b/c) - 512
 *  P2 = 128*b - c * floor(128*b/c)
 *  P3 = c
 * ---------------------------------------------------------------------- */
static void calc_pll(uint32_t fvco, uint32_t fxtal,
                     uint32_t *P1, uint32_t *P2, uint32_t *P3)
{
    uint32_t a  = fvco / fxtal;
    uint32_t c  = SI5351_PMAX;
    uint32_t b  = (uint32_t)(((uint64_t)(fvco - a * fxtal) * c) / fxtal);
    uint32_t dd = (128 * b) / c;
    *P1 = 128 * a + dd - 512;
    *P2 = 128 * b - c * dd;
    *P3 = c;
}

/* -------------------------------------------------------------------------
 *  Obliczenia MultiSynth output divider
 *  fout = fvco / (MS_a * R)
 *  MS_a musi byc parzyste i miescic sie w [6, 1800], lub rowne 4 (divby4)
 *  R = 1, 2, 4 ... 128  (kodowany jako R_code = 0..7)
 * ---------------------------------------------------------------------- */
static void calc_ms(uint32_t fvco, uint32_t fout,
                    uint32_t *P1, uint32_t *P2, uint32_t *P3,
                    uint32_t *R_code, uint32_t *MS_a)
{
    *R_code = 0;
    uint32_t r = 1;
    uint32_t ms = fvco / fout;

    /* Zwieksz R az MS_a wejdzie w zakres 6-1800 */
    while (ms > 1800 && *R_code < 7) {
        (*R_code)++;
        r <<= 1;
        ms = fvco / (fout * r);
    }

    /* divby4 — tryb specjalny dla f > ~150 MHz */
    if (ms <= 4) {
        *MS_a = 4;
        *P1 = 0; *P2 = 0; *P3 = 1;
        return;
    }

    /* Zaokraglij do parzystej (integer mode = najnizszy jitter) */
    uint32_t a = ms & ~1UL;
    if (a < 6) a = 6;
    *MS_a = a;

    /* Czlon ulamkowy dla precyzyjnej czestotliwosci */
    uint32_t fvco_int = fout * r * a;
    uint32_t c  = SI5351_PMAX;
    uint64_t rem = ((uint64_t)fvco > (uint64_t)fvco_int)
                   ? (uint64_t)fvco - fvco_int : 0;
    uint32_t b  = (uint32_t)((rem * c) / ((uint64_t)fout * r));
    uint32_t dd = (128 * b) / c;

    *P1 = 128 * a + dd - 512;
    *P2 = 128 * b - c * dd;
    *P3 = c;
}

/* -------------------------------------------------------------------------
 *  Zapis blokow rejestrów Si5351
 * ---------------------------------------------------------------------- */
static void wr_pll(uint8_t base, uint32_t P1, uint32_t P2, uint32_t P3)
{
    uint8_t d[8] = {
        (P3 >> 8) & 0xFF,  P3 & 0xFF,
        (P1 >> 16) & 0x03, (P1 >> 8) & 0xFF, P1 & 0xFF,
        ((P3 >> 12) & 0xF0) | ((P2 >> 16) & 0x0F),
        (P2 >> 8) & 0xFF,  P2 & 0xFF
    };
    regs_write(base, d, 8);
}

static void wr_ms(uint8_t base, uint32_t P1, uint32_t P2, uint32_t P3,
                  uint32_t R, uint32_t divby4)
{
    uint8_t d[8];
    d[0] = (P3 >> 8) & 0xFF;
    d[1] =  P3       & 0xFF;
    if (divby4) {
        d[2] = 0x0C; d[3] = 0; d[4] = 0;
    } else {
        d[2] = ((R & 0x07) << 4) | ((P1 >> 16) & 0x03);
        d[3] = (P1 >> 8) & 0xFF;
        d[4] =  P1       & 0xFF;
    }
    d[5] = ((P3 >> 12) & 0xF0) | ((P2 >> 16) & 0x0F);
    d[6] = (P2 >> 8) & 0xFF;
    d[7] =  P2       & 0xFF;
    regs_write(base, d, 8);
}

/* =========================================================================
 *  si5351_init
 * ====================================================================== */
esp_err_t si5351_init(void)
{
    s_mtx = xSemaphoreCreateMutex();
    if (!s_mtx) return ESP_ERR_NO_MEM;

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port                   = I2C_PORT,
        .sda_io_num                 = SI5351_SDA,
        .scl_io_num                 = SI5351_SCL,
        .clk_source                 = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt          = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_SI5351, "i2c_new_master_bus: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = SI5351_I2C_ADDR,
        .scl_speed_hz    = SI5351_I2C_FREQ,
    };
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_SI5351, "add_device: %s", esp_err_to_name(err));
        return err;
    }

    memset(s_reg_cache, 0xFF, sizeof(s_reg_cache));
    s_cache_valid = true;

    xSemaphoreTake(s_mtx, portMAX_DELAY);

    reg_write(3,   0xFF);   /* Wycisz CLK0..CLK7 */
    reg_write(183, 0x92);   /* XTAL load = 8 pF */
    reg_write(16,  0x80);   /* CLK0 power down */
    reg_write(17,  0x80);   /* CLK1 power down */
    reg_write(18,  0x80);   /* CLK2 power down */
    reg_write(177, 0xA0);   /* Reset PLL_A i PLL_B */

    /*
     *  CLK0 — nosna glowna I:
     *    MS0, src=PLL_A, nie inwertowany, 8 mA, integer mode
     *    reg16 = 0x4F  [7=0:on, 6=1:integer, 5-4=00:src=PLL_A, 3=0:no inv, 2-0=111:8mA]
     *
     *  CLK1 — LO (VFO):
     *    MS1, src=PLL_B, nie inwertowany, 8 mA, integer mode
     *    reg17 = 0x6F  [7=0:on, 6=1:integer, 5-4=10:src=PLL_B, 3=0:no inv, 2-0=111:8mA]
     *
     *  CLK2 — nosna Q (90 stopni):
     *    MS2, src=PLL_A, INVERTOWANY (daje przesunięcie 90° przy tym samym MS co CLK0)
     *    reg18 = 0x5F  [7=0:on, 6=1:integer, 5-4=01:src=PLL_A, 3=1:INV, 2-0=111:8mA]
     *
     *  Uwaga: wyjscia sa wyciszone (reg3=0xFF) az do pierwszego set_freq/set_car_freq
     */
    reg_write(16, 0x4F);   /* CLK0: MS0, PLL_A, 0°,  8mA */
    reg_write(17, 0x6F);   /* CLK1: MS1, PLL_B, 0°,  8mA */
    reg_write(18, 0x5F);   /* CLK2: MS2, PLL_A, 90°, 8mA */

    xSemaphoreGive(s_mtx);

    ESP_LOGI(TAG_SI5351,
             "OK — I2C %d kHz | XTAL %lu Hz | "
             "CLK0=nosna_I(PLL_A) CLK1=LO(PLL_B) CLK2=nosna_Q(PLL_A,90deg)",
             SI5351_I2C_FREQ / 1000,
             (unsigned long)SI5351_XTAL_FREQ);
    return ESP_OK;
}

/* =========================================================================
 *  si5351_set_freq — CLK1 (LO/VFO), PLL_B
 * ====================================================================== */
void si5351_set_freq(uint32_t freq_hz)
{
    if (freq_hz < VFO_FREQ_MIN) freq_hz = VFO_FREQ_MIN;
    if (freq_hz > VFO_FREQ_MAX) freq_hz = VFO_FREQ_MAX;

    uint32_t P1, P2, P3, R, MS_a;
    calc_ms(FVCO_NOMINAL, freq_hz, &P1, &P2, &P3, &R, &MS_a);

    /* Integer MS — P2=0, P3=1, P1=128*MS_a-512.
     * Bit MS_INT jest juz ustawiony w rejestrze CLK1 (0x6F bit6=1).
     * calc_ms zwraca ułamkowe P1/P2/P3 dla FVCO_NOMINAL, ale PLL
     * programujemy na fvco = freq*MS_a — te dwie wartości są niespójne.
     * Nadpisanie integer-MS jest wymagane do uzyskania poprawnej częstotliwości. */
    if (MS_a != 4) {
        P1 = 128 * MS_a - 512;
        P2 = 0;
        P3 = 1;
    }

    /* fVCO dokładne dla integer MS_a */
    uint32_t fvco = freq_hz * MS_a * (1u << R);
    uint32_t pp1, pp2, pp3;
    uint32_t xtal = (uint32_t)((int32_t)SI5351_XTAL_FREQ + s_xtal_cal);
    calc_pll(fvco, xtal, &pp1, &pp2, &pp3);

    bool rst = (s_ms_lo != MS_a);
    s_ms_lo = MS_a;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    wr_pll(34, pp1, pp2, pp3);                        /* PLL_B reg 34-41 */
    wr_ms (50, P1, P2, P3, R, (MS_a == 4) ? 1 : 0);  /* MS1   reg 50-57 */
    if (rst) {
        reg_write(177, 0x80);   /* Reset tylko PLL_B */
        ESP_LOGD(TAG_SI5351, "PLL_B reset (MS_a zmieniło się na %lu)", (unsigned long)MS_a);
    }
    reg_write(3, s_reg_cache[3] & ~(1u << 1));        /* Enable CLK1 */
    xSemaphoreGive(s_mtx);

    ESP_LOGD(TAG_SI5351, "LO(CLK1) %lu Hz | fVCO %lu | MS %lu | R x%lu",
             (unsigned long)freq_hz, (unsigned long)fvco,
             (unsigned long)MS_a,    (unsigned long)(1u << R));
}

/* =========================================================================
 *  si5351_set_car_freq — CLK0 (nosna I) + CLK2 (nosna Q 90°), PLL_A
 * ====================================================================== */
void si5351_set_car_freq(uint32_t freq_hz, bool enable)
{
    if (!enable) {
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        reg_write(16, 0x80);                          /* CLK0 power down */
        reg_write(18, 0x80);                          /* CLK2 power down */
        reg_write(3,  s_reg_cache[3] | 0x05);         /* Disable CLK0 + CLK2 (bity 0 i 2) */
        xSemaphoreGive(s_mtx);
        return;
    }

    if (freq_hz < 1500)       freq_hz = 1500;
    if (freq_hz > 225000000)  freq_hz = 225000000;

    uint32_t P1, P2, P3, R, MS_a;
    calc_ms(FVCO_NOMINAL, freq_hz, &P1, &P2, &P3, &R, &MS_a);

    if (MS_a != 4) {
        P1 = 128 * MS_a - 512;
        P2 = 0;
        P3 = 1;
    }

    uint32_t fvco = freq_hz * MS_a * (1u << R);
    uint32_t pp1, pp2, pp3;
    uint32_t xtal = (uint32_t)((int32_t)SI5351_XTAL_FREQ + s_xtal_cal);
    calc_pll(fvco, xtal, &pp1, &pp2, &pp3);

    bool rst = (s_ms_car != MS_a);
    s_ms_car = MS_a;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    wr_pll(26, pp1, pp2, pp3);                        /* PLL_A reg 26-33 */
    wr_ms (42, P1, P2, P3, R, (MS_a == 4) ? 1 : 0);  /* MS0 (CLK0) reg 42-49 */
    wr_ms (58, P1, P2, P3, R, (MS_a == 4) ? 1 : 0);  /* MS2 (CLK2) reg 58-65, te same param */
    if (rst) {
        reg_write(177, 0x20);   /* Reset tylko PLL_A */
    }
    /* Przywroc CLK control (na wypadek gdyby byly power-down) */
    reg_write(16, 0x4F);   /* CLK0: MS0, PLL_A, 0°  */
    reg_write(18, 0x5F);   /* CLK2: MS2, PLL_A, 90° */
    reg_write(3,  s_reg_cache[3] & ~0x05u);   /* Enable CLK0 + CLK2 */
    xSemaphoreGive(s_mtx);

    ESP_LOGD(TAG_SI5351, "CAR(CLK0+CLK2) %lu Hz | fVCO %lu | MS %lu",
             (unsigned long)freq_hz, (unsigned long)fvco, (unsigned long)MS_a);
}

/* =========================================================================
 *  si5351_set_xtal_cal — korekta czestotliwosci kwarcu [Hz]
 *  Zakres: XTAL_CAL_MIN..XTAL_CAL_MAX (+/-5000 Hz)
 *  Nowa wartosc bedzie uzyta przy nastepnym wywolaniu set_freq/set_car_freq.
 * ====================================================================== */
void si5351_set_xtal_cal(int32_t cal)
{
    if (cal > XTAL_CAL_MAX) cal = XTAL_CAL_MAX;
    if (cal < XTAL_CAL_MIN) cal = XTAL_CAL_MIN;
    s_xtal_cal = cal;
    ESP_LOGI(TAG_SI5351, "xtal_cal = %ld Hz (XTAL efektywny: %lu Hz)",
             (long)cal, (unsigned long)((int32_t)SI5351_XTAL_FREQ + cal));
}

/* =========================================================================
 *  si5351_output_enable
 * ====================================================================== */
void si5351_output_enable(uint8_t clk_num, bool enable)
{
    if (clk_num > 7) return;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    uint8_t r = s_reg_cache[3];
    if (enable) r &= ~(1u << clk_num);
    else        r |=  (1u << clk_num);
    reg_write(3, r);
    xSemaphoreGive(s_mtx);
}
