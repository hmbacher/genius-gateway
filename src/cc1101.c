/**
 * @file cc1101.c
 * @brief CC1101 driver
 * 
 * @copyright Copyright (c) 2024-2025 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 * 
 * This file is part of Genius Gateway.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the Commons Clause restriction.
 * 
 * "Commons Clause" License Condition v1.0
 * The Software is provided to you by the Licensor under the License,
 * as defined below, subject to the following condition:
 * Without limiting other conditions in the License, the grant of rights
 * under the License will not include, and the License does not grant to you,
 * the right to Sell the Software.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 * 
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#include <string.h>
#include <stdbool.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <sys/time.h> // Required for gettimeofday
#include <esp_timer.h> // Required for esp_timer_get_time (ISR-safe timing)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp32-hal-log.h" // Remap ESP_LOGx to the Arduino log format so cc1101 logs match the rest of the project
#include "cc1101.h"

static const char *TAG = "cc1101";

/*
 * BOARD PIN PROFILE — single source of truth for per-board CC1101 pin presets.
 *
 * Hand-maintained authoring point: the GG board defines -D BOARD_GG_V1 in platformio.ini; any
 * other build falls through to the generic (Custom-only) profile. A profile bundles named presets
 * (known-good pin sets); free "Custom" selection and the chip's assignable-pin set are handled at
 * runtime by the pins service. Serialized to JSON over REST via cc1101_active_profile().
 *
 * Lives in this C translation unit because the designated initializers below are not valid in
 * strict C++ (the types cc1101_preset_t / cc1101_pin_profile_t are declared in cc1101.h so C++
 * consumers can still read a profile).
 */
#define CC1101_PRESET_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

#if defined(BOARD_GG_V1)
// Genius Gateway PCB 1.0 (Seeed XIAO ESP32-S3). The CC1101 is soldered to fixed pins; the
// "GG v1.0" preset is the known-good default. Custom remains available for rewired hardware.
static const cc1101_preset_t PROFILE_PRESETS[] = {
    {.name = "GG v1.0", .pins = {.csn = 5, .miso = 8, .mosi = 9, .sck = 7, .gdo0 = 6, .spi_host = SPI2_HOST}},
};
static const cc1101_pin_profile_t CC1101_PIN_PROFILE = {
    .presets = PROFILE_PRESETS,
    .preset_count = CC1101_PRESET_COUNT(PROFILE_PRESETS),
};
#else
// Generic ESP32-S3 target (dev boards / undocumented clones) — the default for any build that
// doesn't define BOARD_GG_V1. No presets: boots unconfigured and the user assigns pins via the
// Custom picker (any valid GPIO; flash/PSRAM/USB hard-blocked, strapping warned).
static const cc1101_pin_profile_t CC1101_PIN_PROFILE = {
    .presets = NULL,
    .preset_count = 0,
};
#endif

/* Active runtime pin assignment. Initialized to an invalid sentinel until cc1101_init()
 * copies in the configured pins. */
static cc1101_pins_t _pins = {.csn = -1, .miso = -1, .mosi = -1, .sck = -1, .gdo0 = -1, .spi_host = -1};

/*
 * MACROS
 */
/* Select CC1101 (via CSn to low) */
#define CC1101_SELECT() gpio_set_level((gpio_num_t)_pins.csn, 0)
/* Deselect CC1101 (via CSn to high) */
#define CC1101_DESELECT() gpio_set_level((gpio_num_t)_pins.csn, 1)

/* Timeout values as loop counters */
#define CC1101_MISO_TIMEOUT_LOOPS 10000     // ~1-2ms at typical CPU speeds
#define CC1101_GDO0_TIMEOUT_LOOPS 100000    // ~10-20ms at typical CPU speeds

/**
 * @brief Wait until SPI MISO line goes low with timeout
 * @return true if MISO went low within timeout, false on timeout
 */
static inline bool wait_miso_low(void)
{
    uint32_t timeout_counter = 0;
    while (gpio_get_level((gpio_num_t)_pins.miso) > 0) {
        if (++timeout_counter > CC1101_MISO_TIMEOUT_LOOPS) {
            ESP_LOGE(TAG, "MISO timeout: line did not go low");
            return false;
        }
    }
    return true;
}

/**
 * @brief Wait until GDO0 line goes high with timeout
 * @return true if GDO0 went high within timeout, false on timeout
 */
static inline bool wait_gdo0_high(void)
{
    uint32_t timeout_counter = 0;
    while (!gpio_get_level((gpio_num_t)_pins.gdo0)) {
        if (++timeout_counter > CC1101_GDO0_TIMEOUT_LOOPS) {
            ESP_LOGE(TAG, "GDO0 timeout: line did not go high");
            return false;
        }
    }
    return true;
}

/**
 * @brief Wait until GDO0 line goes low with timeout
 * @return true if GDO0 went low within timeout, false on timeout
 */
static inline bool wait_gdo0_low(void)
{
    uint32_t timeout_counter = 0;
    while (gpio_get_level((gpio_num_t)_pins.gdo0)) {
        if (++timeout_counter > CC1101_GDO0_TIMEOUT_LOOPS) {
            ESP_LOGE(TAG, "GDO0 timeout: line did not go low");
            return false;
        }
    }
    return true;
}

/* Read CC1101 configuration register value */
#define READ_CONFIG_REG(regAddr, result) cc1101_read_reg(regAddr, CC1101_CONFIG_REGISTER, result)
/* Read CC1101 status register */
#define READ_STATUS_REG(regAddr, result) cc1101_read_reg(regAddr, CC1101_STATUS_REGISTER, result)

#define DELAY_US(us) esp_rom_delay_us(us)

static cc1101_mode_t _mode = CCM_IDLE;

static spi_device_handle_t _handle;          // SPI device handle; NULL when no device is added
static bool _bus_initialized = false;        // true while the SPI bus is owned (for teardown)
static bool _isr_service_installed = false;  // true once the shared GPIO ISR service is installed

static void (*_rx_callback)() = NULL;

static uint32_t _last_rising_edge = 0; // Last rising edge timestamp for GDO0 in milliseconds
static uint32_t _last_falling_edge = 0; // Last falling edge timestamp for GDO0 in milliseconds

/* Recursive mutex serializing all coarse radio operations, so live re-init/teardown
 * (cc1101_init/deinit/probe) cannot race with RX/TX (cc1101_receive_data/send_data) or the
 * monitoring loop running on other tasks. Created lazily on first use; the first call happens
 * during single-threaded startup (or a probe while RX is idle), so creation is race-free. */
static SemaphoreHandle_t _lock = NULL;

static inline void cc1101_lock(void)
{
    if (_lock == NULL)
        _lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreTakeRecursive(_lock, portMAX_DELAY);
}

static inline void cc1101_unlock(void)
{
    xSemaphoreGiveRecursive(_lock);
}

static const uint8_t defaultCfg[] = {
    CC1101_DEFVAL_IOCFG2,
    CC1101_DEFVAL_IOCFG1,
    CC1101_DEFVAL_IOCFG0,
    CC1101_DEFVAL_FIFOTHR,
    CC1101_DEFVAL_SYNC1,
    CC1101_DEFVAL_SYNC0,
    CC1101_DEFVAL_PKTLEN,
    CC1101_DEFVAL_PKTCTRL1,
    CC1101_DEFVAL_PKTCTRL0,
    CC1101_DEFVAL_ADDR,
    CC1101_DEFVAL_CHANNR,
    CC1101_DEFVAL_FSCTRL1,
    CC1101_DEFVAL_FSCTRL0,
    CC1101_DEFVAL_FREQ2,
    CC1101_DEFVAL_FREQ1,
    CC1101_DEFVAL_FREQ0,
    CC1101_DEFVAL_MDMCFG4,
    CC1101_DEFVAL_MDMCFG3,
    CC1101_DEFVAL_MDMCFG2,
    CC1101_DEFVAL_MDMCFG1,
    CC1101_DEFVAL_MDMCFG0,
    CC1101_DEFVAL_DEVIATN,
    CC1101_DEFVAL_MCSM2,
    CC1101_DEFVAL_MCSM1,
    CC1101_DEFVAL_MCSM0,
    CC1101_DEFVAL_FOCCFG,
    CC1101_DEFVAL_BSCFG,
    CC1101_DEFVAL_AGCCTRL2,
    CC1101_DEFVAL_AGCCTRL1,
    CC1101_DEFVAL_AGCCTRL0,
    CC1101_DEFVAL_WOREVT1,
    CC1101_DEFVAL_WOREVT0,
    CC1101_DEFVAL_WORCTRL,
    CC1101_DEFVAL_FREND1,
    CC1101_DEFVAL_FREND0,
    CC1101_DEFVAL_FSCAL3,
    CC1101_DEFVAL_FSCAL2,
    CC1101_DEFVAL_FSCAL1,
    CC1101_DEFVAL_FSCAL0,
    CC1101_DEFVAL_RCCTRL1,
    CC1101_DEFVAL_RCCTRL0,
    CC1101_DEFVAL_FSTEST,
    CC1101_DEFVAL_PTEST,
    CC1101_DEFVAL_AGCTEST,
    CC1101_DEFVAL_TEST2,
    CC1101_DEFVAL_TEST1,
    CC1101_DEFVAL_TEST0};

/*
 * Declarations of static functions.
 */

/**
 * cmdStrobe
 *
 * Send command strobe to the CC1101 IC via SPI
 *
 * @param cmd Command strobe
 */
static esp_err_t cc1101_cmd_strobe(uint8_t cmd);

/**
 * writeReg
 *
 * Write single register into the CC1101 IC via SPI
 *
 * @param regAddr Register address
 * @param value Value to be writen
 */
static esp_err_t cc1101_write_reg(uint8_t regAddr, uint8_t value);

/**
 * writeBurstReg
 *
 * Write multiple registers into the CC1101 IC via SPI
 *
 * @param regAddr Register address
 * @param buffer Data to be writen
 * @param len Data length
 */
static esp_err_t cc1101_write_burst_reg(uint8_t regAddr, uint8_t *buffer, uint8_t len);

/**
 * readReg
 *
 * Read CC1101 register via SPI
 *
 * @param regAddr Register address
 * @param regType Type of register: CC1101_CONFIG_REGISTER or CC1101_STATUS_REGISTER
 *
 * Return:
 *	Data byte returned by the CC1101 IC
 */
static esp_err_t cc1101_read_reg(uint8_t regAddr, uint8_t regType, uint8_t *result);

/**
 * readBurstReg
 *
 * Read burst data from CC1101 via SPI
 *
 * @param buffer Buffer where to copy the result to
 * @param regAddr Register address
 * @param len Data length
 */
static esp_err_t cc1101_read_burst_reg(uint8_t *buffer, uint8_t regAddr, uint8_t len);

/**
 * reset
 *
 * Reset CC1101
 */
static esp_err_t cc1101_reset(void);

/**
 * @brief Reads the data packet from RX FIFO.
 *
 * @return ESP_OK if successful, ESP_FAIL otherwise
 */
static inline esp_err_t cc1101_read_rx_fifo(cc1101_packet_t *packet);

/*
 * Function definitions
 */

static void IRAM_ATTR _rxtx_finish_isr(void *arg)
{
    // Get current time using ISR-safe function (microseconds since boot)
    uint32_t current_time_ms = (unsigned long)(esp_timer_get_time() / 1000ULL);
    // Read current GPIO level to determine edge type
    int gpio_level = gpio_get_level((gpio_num_t)_pins.gdo0);
    
    if (gpio_level == 1) {  // Rising edge detected
        _last_rising_edge = current_time_ms;
    } else {    // Falling edge detected
        _last_falling_edge = current_time_ms;
        
        // Only call RX callback on falling edge (end of packet)
        if (_mode == CCM_RX && _rx_callback != NULL)
        {
            _rx_callback();
        }
    }
}

static esp_err_t cc1101_spi_init()
{
    // Configure CSn pin as GPIO for manual CSn-control
    gpio_reset_pin((gpio_num_t)_pins.csn);
    gpio_set_direction((gpio_num_t)_pins.csn, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)_pins.csn, 1);

    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.sclk_io_num = _pins.sck;
    buscfg.mosi_io_num = _pins.mosi;
    buscfg.miso_io_num = _pins.miso;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;

    if (spi_bus_initialize((spi_host_device_t)_pins.spi_host, &buscfg, SPI_DMA_DISABLED) != ESP_OK) // Not using DMA is faster, but limits the size of transactions
    {
        ESP_LOGE(TAG, "SPI bus initialization failed.");
        return ESP_FAIL;
    }
    _bus_initialized = true;
    ESP_LOGI(TAG, "SPI bus initialized.");

    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.clock_speed_hz = 5000000; // SPI clock is 5 MHz!
    devcfg.queue_size = 7;
    devcfg.mode = 0;
    devcfg.spics_io_num = -1; // we will use manual CS control
    devcfg.flags = SPI_DEVICE_NO_DUMMY;

    if (spi_bus_add_device((spi_host_device_t)_pins.spi_host, &devcfg, &_handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI device could not be added.");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "SPI device added.");

    return ESP_OK;
}

static esp_err_t cc1101_cmd_strobe(uint8_t cmd)
{
    if (_handle == NULL)
        return ESP_ERR_INVALID_STATE;

    CC1101_SELECT();
    if (!wait_miso_low()) {
        CC1101_DESELECT();
        return ESP_ERR_TIMEOUT;
    }

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 8; // 1 strobe cmd byte
    t.tx_data[0] = cmd;

    /* Polling transmit is typically faster than interrupt-based,
     * but does not allow for other tasks to run */
    esp_err_t ret = spi_device_polling_transmit(_handle, &t);

    CC1101_DESELECT();

    return ret;
}

static esp_err_t cc1101_write_reg(uint8_t regAddr, uint8_t value)
{
    if (_handle == NULL)
        return ESP_ERR_INVALID_STATE;

    CC1101_SELECT();
    if (!wait_miso_low()) {
        CC1101_DESELECT();
        return ESP_ERR_TIMEOUT;
    }

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 16; // 1 addr byte + 1 data byte
    t.tx_data[0] = regAddr;
    t.tx_data[1] = value;

    /* Polling transmit is typically faster than interrupt-based,
     * but does not allow for other tasks to run */
    esp_err_t ret = spi_device_polling_transmit(_handle, &t);

    CC1101_DESELECT();

    return ret;
}

static esp_err_t cc1101_write_burst_reg(uint8_t regAddr, uint8_t *buffer, uint8_t len)
{
    if (_handle == NULL)
        return ESP_ERR_INVALID_STATE;

    CC1101_SELECT();
    if (!wait_miso_low()) {
        CC1101_DESELECT();
        return ESP_ERR_TIMEOUT;
    }

    spi_transaction_ext_t t;
    memset(&t, 0, sizeof(t));
    t.base.flags = SPI_TRANS_VARIABLE_ADDR;
    t.base.addr = regAddr | WRITE_BURST;
    t.base.length = 8 * len;
    t.base.tx_buffer = buffer;
    t.address_bits = 8;

    /* Polling transmit is typically faster than interrupt-based,
     * but does not allow for other tasks to run */
    esp_err_t ret = spi_device_polling_transmit(_handle, (spi_transaction_t *)&t);

    CC1101_DESELECT();

    return ret;
}

static esp_err_t cc1101_read_reg(uint8_t regAddr, uint8_t regType, uint8_t *result)
{
    if (_handle == NULL)
        return ESP_ERR_INVALID_STATE;

    CC1101_SELECT();
    if (!wait_miso_low()) {
        CC1101_DESELECT();
        return ESP_ERR_TIMEOUT;
    }

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 16; // 1 addr byte + 1 data byte
    t.tx_data[0] = regAddr | regType;

    /* Polling transmit is typically faster than interrupt-based,
     * but does not allow for other tasks to run */
    esp_err_t ret = spi_device_polling_transmit(_handle, &t);

    CC1101_DESELECT();

    if (ret == ESP_OK)
    {
        /* Read register value is in the second received byte */
        *result = t.rx_data[1];
    }

    return ret;
}

static esp_err_t cc1101_read_burst_reg(uint8_t *buffer, uint8_t regAddr, uint8_t len)
{
    if (_handle == NULL)
        return ESP_ERR_INVALID_STATE;

    CC1101_SELECT();
    if (!wait_miso_low()) {
        CC1101_DESELECT();
        return ESP_ERR_TIMEOUT;
    }

    spi_transaction_ext_t t;
    memset(&t, 0, sizeof(t));
    t.base.flags = SPI_TRANS_VARIABLE_ADDR;
    t.base.addr = regAddr | READ_BURST;
    t.base.length = 8 * len;
    t.base.rx_buffer = buffer;
    t.address_bits = 8;

    /* Polling transmit is typically faster than interrupt-based,
     * but does not allow for other tasks to run */
    esp_err_t ret = spi_device_polling_transmit(_handle, (spi_transaction_t *)&t);

    CC1101_DESELECT();

    return ret;
}

static esp_err_t cc1101_reset(void)
{
    CC1101_DESELECT();
    esp_rom_delay_us(5);
    CC1101_SELECT();
    esp_rom_delay_us(10);
    CC1101_DESELECT();
    esp_rom_delay_us(41);
    CC1101_SELECT();

    if (!wait_miso_low()) {
        CC1101_DESELECT();
        return ESP_ERR_TIMEOUT;
    }
    
    /* Send RESET strobe command while keeping CS asserted.
     * Cannot use cc1101_cmd_strobe() here because CS must stay low
     * throughout the entire reset sequence per the CC1101 datasheet. */
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 8;
    t.tx_data[0] = CC1101_SRES;

    esp_err_t ret = spi_device_polling_transmit(_handle, &t);

    /* Wait for MISO to go low indicating reset is complete (CS still asserted) */
    if (ret == ESP_OK && !wait_miso_low()) {
        ret = ESP_ERR_TIMEOUT;
    }

    CC1101_DESELECT();

    return ret;
}

/* Verify GDO0 connectivity: command GDO0 to a constant 0 then 1 via IOCFG0 and confirm the
 * input pin follows, then restore the operational GDO0 configuration. Must be called with the
 * radio configured (IOCFG0 writable). Returns true only if GDO0 toggled as commanded. */
static bool cc1101_gdo0_responds(void)
{
    bool low_ok = false, high_ok = false;
    if (cc1101_write_reg(CC1101_IOCFG0, 0x2F) == ESP_OK) // GDOx_CFG = HW to 0
    {
        esp_rom_delay_us(50);
        low_ok = (gpio_get_level((gpio_num_t)_pins.gdo0) == 0);
    }
    if (cc1101_write_reg(CC1101_IOCFG0, 0x6F) == ESP_OK) // 0x40 (GDOx_INV) | 0x2F = HW to 1
    {
        esp_rom_delay_us(50);
        high_ok = (gpio_get_level((gpio_num_t)_pins.gdo0) == 1);
    }
    // Restore operational GDO0 behavior (asserts on sync word / end of packet)
    cc1101_write_reg(CC1101_IOCFG0, CC1101_DEFVAL_IOCFG0);
    return low_ok && high_ok;
}

static esp_err_t cc1101_init_impl(const cc1101_pins_t *pins, void (*rx_callback)())
{
    if (pins == NULL)
    {
        ESP_LOGE(TAG, "cc1101_init called with NULL pins.");
        return ESP_ERR_INVALID_ARG;
    }

    /* Start from a clean slate so init is safe to call repeatedly (re-init / retry).
     * Safe no-op on the first call. */
    cc1101_deinit();

    /* Adopt the supplied pin assignment for all subsequent SPI/GDO0 access */
    _pins = *pins;

    /* Configuring/initializing SPI */
    if (cc1101_spi_init() == ESP_FAIL)
    {
        ESP_LOGE(TAG, "SPI could not be configured.");
        cc1101_deinit();
        return ESP_FAIL;
    }

    /* Reset procedure according to CC1101 datasheet */
    if (cc1101_reset() != ESP_OK)
    {
        ESP_LOGE(TAG, "CC1101 could not be reset.");
        cc1101_deinit();
        return ESP_FAIL;
    }

    /* Check Chip ID */
    uint8_t chip_partnum = 0xFF;
    uint8_t chip_version = 0xFF;
    READ_STATUS_REG(CC1101_PARTNUM, &chip_partnum);
    READ_STATUS_REG(CC1101_VERSION, &chip_version);
    ESP_LOGI(TAG, "CC1101_PARTNUM %d", chip_partnum);
    ESP_LOGI(TAG, "CC1101_VERSION %d", chip_version);
    if (chip_partnum != 0 || chip_version != 20)
    {
        ESP_LOGE(TAG, "CC1101 not installed.");
        cc1101_deinit();
        return ESP_FAIL;
    }

    /* Configuring CC1101 */
    if (cc1101_write_burst_reg(0x00, (uint8_t *)defaultCfg, sizeof(defaultCfg)) != ESP_OK)
    {
        ESP_LOGE(TAG, "CC1101 could not be configured.");
        cc1101_deinit();
        return ESP_FAIL;
    }

    /* Configure interrupt on GDO0 */
    _rx_callback = rx_callback;
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE, // GPIO interrupt type : both rising and falling edges
        .pin_bit_mask = 1ULL << _pins.gdo0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0};
    gpio_config(&io_conf);
    // Install the shared GPIO ISR service once. Re-installing logs a driver-level error, so a
    // live re-init / probe-restore must not call it again.
    if (!_isr_service_installed)
    {
        esp_err_t isr_ret = gpio_install_isr_service(0);
        _isr_service_installed = (isr_ret == ESP_OK || isr_ret == ESP_ERR_INVALID_STATE);
    }
    gpio_isr_handler_add((gpio_num_t)_pins.gdo0, _rxtx_finish_isr, NULL);

    /* Verify GDO0 is actually wired. A mis-wired GDO0 leaves the radio unable to receive (the
     * ISR never fires) even though SPI works, so treat it as a bring-up failure rather than
     * reporting a "working" radio. */
    if (!cc1101_gdo0_responds())
    {
        ESP_LOGE(TAG, "GDO0 did not respond to the connectivity check — check the GDO0 wiring.");
        cc1101_deinit();
        return ESP_FAIL;
    }

    /* Setting to RX state */
    if (cc1101_set_rx_state() != ESP_OK)
    {
        ESP_LOGE(TAG, "CC1101 could not be set to RX state.");
        cc1101_deinit();
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t cc1101_init(const cc1101_pins_t *pins, void (*rx_callback)())
{
    cc1101_lock();
    esp_err_t ret = cc1101_init_impl(pins, rx_callback);
    cc1101_unlock();
    return ret;
}

void cc1101_deinit(void)
{
    cc1101_lock();
    /* Best-effort: strobe the radio to IDLE while the SPI link is still up */
    if (_handle != NULL)
    {
        cc1101_cmd_strobe(CC1101_SIDLE);
    }

    /* Detach the GDO0 interrupt and return the pin to its default state. The process-shared
     * GPIO ISR service is intentionally left installed (other peripherals may use it). */
    if (_pins.gdo0 >= 0)
    {
        gpio_isr_handler_remove((gpio_num_t)_pins.gdo0);
        gpio_reset_pin((gpio_num_t)_pins.gdo0);
    }

    /* Release the SPI device, then the bus (order matters) */
    if (_handle != NULL)
    {
        spi_bus_remove_device(_handle);
        _handle = NULL;
    }
    if (_bus_initialized)
    {
        spi_bus_free((spi_host_device_t)_pins.spi_host);
        _bus_initialized = false;
    }

    /* Return CSn to its default state */
    if (_pins.csn >= 0)
    {
        gpio_reset_pin((gpio_num_t)_pins.csn);
    }

    /* Reset driver state; restore the invalid-pin sentinel */
    _rx_callback = NULL;
    _mode = CCM_IDLE;
    _last_rising_edge = 0;
    _last_falling_edge = 0;
    _pins = (cc1101_pins_t){.csn = -1, .miso = -1, .mosi = -1, .sck = -1, .gdo0 = -1, .spi_host = -1};

    cc1101_unlock();
}

static esp_err_t cc1101_probe_impl(const cc1101_pins_t *pins, cc1101_probe_result_t *result)
{
    if (pins == NULL || result == NULL)
        return ESP_ERR_INVALID_ARG;

    result->spi_ok = false;
    result->chip_detected = false;
    result->gdo0_ok = false;
    result->partnum = 0;
    result->version = 0;

    /* Clean slate, then adopt the candidate pins. The caller guarantees the radio is not
     * actively running and is responsible for restoring any prior state afterwards. */
    cc1101_deinit();
    _pins = *pins;

    if (cc1101_spi_init() != ESP_OK)
    {
        cc1101_deinit();
        return ESP_OK; // result->spi_ok stays false
    }
    result->spi_ok = true;

    /* Reset and read the chip ID — this proves the SCK/MOSI/MISO/CSn wiring is correct. */
    if (cc1101_reset() == ESP_OK)
    {
        READ_STATUS_REG(CC1101_PARTNUM, &result->partnum);
        READ_STATUS_REG(CC1101_VERSION, &result->version);
        result->chip_detected = (result->partnum == 0 && result->version == 20);
    }

    /* GDO0 connectivity test (only meaningful once the chip responds): command GDO0 to a
     * constant 0 then 1 via IOCFG0 and confirm the input pin follows. */
    if (result->chip_detected)
    {
        gpio_config_t gdo0_conf = {
            .pin_bit_mask = 1ULL << _pins.gdo0,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&gdo0_conf);
        result->gdo0_ok = cc1101_gdo0_responds();
    }

    cc1101_deinit();
    return ESP_OK;
}

esp_err_t cc1101_probe(const cc1101_pins_t *pins, cc1101_probe_result_t *result)
{
    cc1101_lock();
    esp_err_t ret = cc1101_probe_impl(pins, result);
    cc1101_unlock();
    return ret;
}

/**
 * @brief Get system time in microseconds
 * @param time_us Pointer to store the time in microseconds since the Unix epoch
 * @return ESP_OK on success, ESP_FAIL on failure
 */
static inline esp_err_t get_system_time_us(uint64_t *time_us)
{
    struct timeval now;
    if (gettimeofday(&now, NULL) == -1) // microseconds precision
    {
        return ESP_FAIL;
    }

    *time_us = (int64_t)now.tv_sec * 1000000L + (int64_t)now.tv_usec;

    return ESP_OK;
}

esp_err_t cc1101_set_rx_state(void)
{
    cc1101_lock();
    esp_err_t ret = cc1101_cmd_strobe(CC1101_SRX);

    if (ret == ESP_OK)
        _mode = CCM_RX;

    cc1101_unlock();
    return ret;
}

esp_err_t cc1101_set_tx_state(void)
{
    cc1101_lock();
    esp_err_t ret = cc1101_cmd_strobe(CC1101_STX);

    if (ret == ESP_OK)
        _mode = CCM_TX;

    cc1101_unlock();
    return ret;
}

esp_err_t cc1101_flush_rx_fifo(void)
{
    cc1101_lock();
    esp_err_t ret;
    ret = cc1101_cmd_strobe(CC1101_SIDLE);
    ret &= cc1101_cmd_strobe(CC1101_SFRX);

    cc1101_unlock();
    return ret;
}

static inline esp_err_t cc1101_flush_tx_fifo(void)
{
    esp_err_t ret;
    ret = cc1101_cmd_strobe(CC1101_SIDLE);
    ret &= cc1101_cmd_strobe(CC1101_SFTX);

    return ret;
}

static inline esp_err_t cc1101_read_rx_fifo(cc1101_packet_t *packet)
{
    uint8_t status;
    uint8_t rxBytes = 0xFF;

    if (READ_STATUS_REG(CC1101_RXBYTES, &rxBytes) != ESP_OK)
    {
        ESP_LOGD(TAG, "Could not obtain available data.");
        return ESP_FAIL;
    }

    if ((rxBytes & 0x7F) == 0)
    {
        ESP_LOGD(TAG, "No data available.");
        return ESP_FAIL;
    }

    if (rxBytes & 0x80)
    {
        ESP_LOGD(TAG, "RX FIFO overflow.");
        return ESP_FAIL;
    }

    // Read RX FIFO buffer
    if (cc1101_read_burst_reg(packet->buffer, CC1101_RXFIFO, rxBytes) != ESP_OK)
    {
        ESP_LOGD(TAG, "Could not read RX FIFO buffer.");
        return ESP_FAIL;
    }

    // Read data length
    packet->length = packet->buffer[0];          // First byte is the length
    status = packet->buffer[packet->length + 2]; // Second appended status byte (LQI and CRC_OK)

    /* Is packet length ok? */
    if (packet->length > CC1101_MAX_PACKET_LEN)
    {
        ESP_LOGD(TAG, "Unexpected packet length: %d (Expected <= %d)", packet->length, CC1101_MAX_PACKET_LEN);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Is packet length consistent? */
    if (packet->length != rxBytes - NUM_ADDITIONAL_BYTES)
    {
        ESP_LOGD(TAG, "Packet length mismatch: %d (packet length) != %d (RX FIFO)", packet->length, rxBytes - 3);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Is CRC ok? */
    else if (!(status & 0x80))
    {
        ESP_LOGD(TAG, "CRC missmatch.");
        return ESP_ERR_INVALID_CRC;
    }

    /* Genius packet data starts after length byte */
    packet->data = &packet->buffer[1];

    /* Set timestamp */
    get_system_time_us(&packet->timestamp);

    return ESP_OK;
}

static inline esp_err_t cc1101_write_tx_fifo(unsigned char *tx_data, size_t length)
{
    if (!tx_data)
    {
        ESP_LOGE(TAG, "Packet data is NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    if (length <= 0 || length > CC1101_MAX_PACKET_LEN)
    {
        ESP_LOGE(TAG, "Invalid packet length: %d (Expected > 0 and <= %d)", length, CC1101_MAX_PACKET_LEN);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t marcState;

    cc1101_write_reg(CC1101_TXFIFO, length); // Set data length at the first position of the TX FIFO
    cc1101_write_burst_reg(CC1101_TXFIFO, tx_data, length);
    cc1101_set_tx_state();

    // Wait for the sync word to be transmitted
    if (!wait_gdo0_high()) {
        return ESP_ERR_TIMEOUT;
    }
    // Wait until the end of the packet transmission
    if (!wait_gdo0_low()) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t cc1101_receive_data(cc1101_packet_t *packet)
{
    cc1101_lock();
    esp_err_t ret = cc1101_read_rx_fifo(packet);

    if (ret != ESP_OK)
    {
        cc1101_flush_rx_fifo();
        cc1101_set_rx_state();
    }

    cc1101_unlock();
    return ret;
}

esp_err_t cc1101_send_data(unsigned char *tx_data, size_t length)
{
    cc1101_lock();
    esp_err_t ret;

    if (_handle == NULL)
    {
        ret = ESP_ERR_INVALID_STATE;
    }
    else
    {
        ret = cc1101_write_tx_fifo(tx_data, length);
        if (ret != ESP_OK)
            cc1101_flush_tx_fifo();
    }

    cc1101_unlock();
    return ret;
}

esp_err_t cc1101_check_rx_fifo(bool reset_on_any_data)
{
    cc1101_lock();
    esp_err_t ret = ESP_OK;
    uint8_t rxBytes = 0x00;
    if (READ_STATUS_REG(CC1101_RXBYTES, &rxBytes) != ESP_OK)
    {
        ESP_LOGD(TAG, "Could not obtain available data.");
        ret = ESP_FAIL;
    }
    else if (rxBytes & RXFIFO_OVERFLOW || (reset_on_any_data & rxBytes > 0))
    {
        cc1101_cmd_strobe(CC1101_SFRX);
        cc1101_set_rx_state();
    }

    cc1101_unlock();
    return ret;
}

cc1101_mode_t cc1101_get_mode(void)
{
    return _mode;
}

esp_err_t cc1101_get_state(uint8_t *state)
{
    cc1101_lock();
    esp_err_t ret = READ_STATUS_REG(CC1101_MARCSTATE, state);
    cc1101_unlock();
    return ret;
}

/**
 * @brief Get the timestamp of the last rising edge on GDO0
 * @return Timestamp in milliseconds since boot, or 0 if no rising edge occurred
 */
uint32_t cc1101_get_last_rising_edge(void)
{
    return _last_rising_edge;
}

/**
 * @brief Get the timestamp of the last falling edge on GDO0  
 * @return Timestamp in milliseconds since boot, or 0 if no falling edge occurred
 */
uint32_t cc1101_get_last_falling_edge(void)
{
    return _last_falling_edge;
}

const cc1101_pins_t *cc1101_default_pins(void)
{
    // A target with a preset boots from preset[0]; a preset-less target boots unconfigured.
    static const cc1101_pins_t invalid = {.csn = -1, .miso = -1, .mosi = -1, .sck = -1, .gdo0 = -1, .spi_host = -1};
    if (CC1101_PIN_PROFILE.preset_count > 0)
        return &CC1101_PIN_PROFILE.presets[0].pins;
    return &invalid;
}

const cc1101_pin_profile_t *cc1101_active_profile(void)
{
    return &CC1101_PIN_PROFILE;
}

int cc1101_get_gdo0_pin(void)
{
    return _pins.gdo0;
}
