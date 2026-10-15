#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define I2C_SCL_IO          GPIO_NUM_22
#define I2C_SDA_IO          GPIO_NUM_21
#define I2C_MASTER_FREQ_HZ  100000
#define LCD_SLAVE_ADDR      0x27

/* PCF8574T bit -> HD44780 mapping (see Wokwi wokwi-lcd1602 docs)
 *   P0 = RS        P1 = R/W       P2 = E        P3 = Backlight
 *   P4 = D4        P5 = D5        P6 = D6       P7 = D7
 */
#define LCD_RS_BIT   0x01
#define LCD_RW_BIT   0x02
#define LCD_EN_BIT   0x04
#define LCD_BL_BIT   0x08

static const char *TAG = "I2C_PROJECT";
static i2c_master_dev_handle_t s_lcd;

/* Pulse the Enable line (high -> low) so the HD44780 latches the nibble. */
static void lcd_pulse_enable(uint8_t data)
{
    uint8_t buf[2] = { (uint8_t)(data | LCD_EN_BIT), (uint8_t)(data & ~LCD_EN_BIT) };
    ESP_ERROR_CHECK(i2c_master_transmit(s_lcd, buf, sizeof(buf), pdMS_TO_TICKS(1000)));
}

/* Write a single 4-bit nibble with the given RS mode. */
static void lcd_write_nibble(uint8_t nibble, uint8_t mode)
{
    uint8_t data = (uint8_t)((nibble << 4) | mode | LCD_BL_BIT);
    lcd_pulse_enable(data);
}

/* Write a full byte as two nibbles (4-bit interface). */
static void lcd_send(uint8_t value, uint8_t mode)
{
    lcd_write_nibble((uint8_t)(value >> 4), mode);
    lcd_write_nibble((uint8_t)(value & 0x0F), mode);
}

#define lcd_cmd(c)   lcd_send((c), 0)
#define lcd_data(d)  lcd_send((d), LCD_RS_BIT)

static void lcd_init(void)
{
    /* HD44780 needs ~40ms after power-on before it accepts commands */
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Force the controller into 4-bit mode (datasheet initialisation sequence) */
    lcd_write_nibble(0x03, 0); vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(0x03, 0); vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(0x03, 0); vTaskDelay(pdMS_TO_TICKS(1));
    lcd_write_nibble(0x02, 0); vTaskDelay(pdMS_TO_TICKS(1));

    lcd_cmd(0x28);                 /* 4-bit bus, 2 lines, 5x8 dots */
    lcd_cmd(0x08);                 /* display off */
    lcd_cmd(0x01);                 /* clear display */
    vTaskDelay(pdMS_TO_TICKS(2));  /* clear needs ~1.52ms */
    lcd_cmd(0x06);                 /* entry mode: cursor right, no shift */
    lcd_cmd(0x0C);                 /* display on, cursor off, blink off */
}

static void lcd_set_cursor(uint8_t col, uint8_t row)
{
    static const uint8_t row_offsets[] = { 0x00, 0x40 };
    lcd_cmd((uint8_t)(0x80 | (col + row_offsets[row & 0x01])));
}

static void lcd_print(const char *text)
{
    while (*text) {
        lcd_data((uint8_t)*text++);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing Modern I2C Master Bus...");

    /* 1. Define Bus Configuration Structure */
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_SCL_IO,
        .sda_io_num = I2C_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    /* 2. Instantiate Master Bus Handle */
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    /* 3. Register Slave Device Configuration */
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = LCD_SLAVE_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &s_lcd));

    /* 4. Probe the bus before talking to the display */
    esp_err_t err = i2c_master_probe(bus_handle, LCD_SLAVE_ADDR, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No ACK from LCD at 0x27 (%s).", esp_err_to_name(err));
        ESP_LOGE(TAG, "Check wiring: SDA=GPIO21, SCL=GPIO22, VCC=5V, GND=GND");
        return;
    }
    ESP_LOGI(TAG, "LCD acknowledged at 0x27. Initializing HD44780...");

    /* 5. Bring up the display and show some text */
    lcd_init();

    lcd_set_cursor(0, 0);
    lcd_print("ESP-IDF v6.1");
    lcd_set_cursor(0, 1);
    lcd_print("I2C LCD  OK");

    ESP_LOGI(TAG, "Text written to the LCD.");

    /* 6. Heartbeat: refresh the second line every second */
    uint32_t seconds = 0;
    char line[17];
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        seconds++;
        snprintf(line, sizeof(line), "Uptime: %lus", (unsigned long)seconds);
        lcd_set_cursor(0, 1);
        lcd_print(line);
    }
}
