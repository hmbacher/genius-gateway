/**
 * @file cc1101.c
 * @brief CC1101 driver
 */

#include <string.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <sys/time.h> // Required for gettimeofday
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cc1101.h"

static const char *TAG = "cc1101";

// SPI Stuff
#if CONFIG_SPI2_HOST
#define HOST_ID SPI2_HOST
#elif CONFIG_SPI3_HOST
#define HOST_ID SPI3_HOST
#endif

/*
 * MACROS
 */
/* Select CC1101 (via CSn to low) */
#define CC1101_SELECT() gpio_set_level(CONFIG_CSN_GPIO, 0)  
/* Deselect CC1101 (via CSn to high) */         
#define CC1101_DESELECT() gpio_set_level(CONFIG_CSN_GPIO, 1)
/* Wait until SPI MISO line goes low */
#define WAIT_MISO_LOW() while (gpio_get_level(CONFIG_MISO_GPIO) > 0)
/* Wait until GDO0 line goes high */
#define WAIT_GDO0_HIGH() while (!gpio_get_level(CONFIG_GDO0_GPIO))
/* Wait until GDO0 line goes low */
#define WAIT_GDO0_LOW() while (gpio_get_level(CONFIG_GDO0_GPIO))
/* Read CC1101 configuration register value */
#define READ_CONFIG_REG(regAddr, result) cc1101_read_reg(regAddr, CC1101_CONFIG_REGISTER, result)
/* Read CC1101 status register */
#define READ_STATUS_REG(regAddr, result) cc1101_read_reg(regAddr, CC1101_STATUS_REGISTER, result)

static spi_device_handle_t _handle;

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
 * writeBurstReg
 * 
 * Write multiple registers into the CC1101 IC via SPI
 * 
 * @param regAddr Register address
 * @param buffer Data to be writen
 * @param len Data length
 */
static esp_err_t cc1101_write_burst_reg(uint8_t regAddr, uint8_t* buffer, uint8_t len);

/**
 * readBurstReg
 * 
 * Read burst data from CC1101 via SPI
 * 
 * @param buffer Buffer where to copy the result to
 * @param regAddr Register address
 * @param len Data length
 */
static esp_err_t cc1101_read_burst_reg(uint8_t * buffer, uint8_t regAddr, uint8_t len);

/**
 * cmdStrobe
 * 
 * Send command strobe to the CC1101 IC via SPI
 * 
 * @param cmd Command strobe
 */
static esp_err_t cc1101_cmd_strobe(uint8_t cmd);

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
 * writeReg
 * 
 * Write single register into the CC1101 IC via SPI
 * 
 * @param regAddr Register address
 * @param value Value to be writen
 */
static esp_err_t cc1101_write_reg(uint8_t regAddr, uint8_t value);

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

/**
 * @brief Set CC1101 to receive mode
 */
static inline esp_err_t cc1101_set_rx_state(void);

/**
 * @brief Flush RX FIFO
 */
static inline esp_err_t cc1101_flush_rx_fifo(void);

/*
 * Function definitions
 */

static esp_err_t cc1101_spi_init()
{
    // Configure CSn pin as GPIO for manual CSn-control
    gpio_reset_pin(CONFIG_CSN_GPIO);
    gpio_set_direction(CONFIG_CSN_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_CSN_GPIO, 1);

    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.sclk_io_num = CONFIG_SCK_GPIO;
    buscfg.mosi_io_num = CONFIG_MOSI_GPIO;
    buscfg.miso_io_num = CONFIG_MISO_GPIO;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;

    if (spi_bus_initialize(HOST_ID, &buscfg, SPI_DMA_DISABLED != ESP_OK))   // Not using DMA is faster, but limits the size of transactions
    {
        ESP_LOGE(TAG, "SPI bus initialization failed.");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "SPI bus initialized.");

    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.clock_speed_hz = 5000000; // SPI clock is 5 MHz!
    devcfg.queue_size = 7;
    devcfg.mode = 0;
    devcfg.spics_io_num = -1; // we will use manual CS control
    devcfg.flags = SPI_DEVICE_NO_DUMMY;

    if (spi_bus_add_device(HOST_ID, &devcfg, &_handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI device could not be added.");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "SPI device added.");
    
    return ESP_OK;
}

static esp_err_t cc1101_cmd_strobe(uint8_t cmd)
{
    CC1101_SELECT();
    WAIT_MISO_LOW();

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
    CC1101_SELECT();
    WAIT_MISO_LOW();

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
    CC1101_SELECT();
    WAIT_MISO_LOW();

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
    CC1101_SELECT();
    WAIT_MISO_LOW();

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
    CC1101_SELECT();
    WAIT_MISO_LOW();

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

    WAIT_MISO_LOW();
    /* Send reset strobe command */
    esp_err_t ret = cc1101_cmd_strobe(CC1101_SRES);
    WAIT_MISO_LOW();

    CC1101_DESELECT();

    return ret;
}

esp_err_t cc1101_init(gpio_isr_t gpio_isr_handler)
{
    /* Configuring/initializing SPI */
    if (cc1101_spi_init() == ESP_FAIL)
    {
        ESP_LOGE(TAG, "SPI could not be configured.");
        return ESP_FAIL;
    }

    /* Reset procedure according to CC1101 datasheet */
    if (cc1101_reset() != ESP_OK)
    {
        ESP_LOGE(TAG, "CC1101 could not be reset.");
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
        return ESP_FAIL;
    }

    /* Configuring CC1101 */
    if (cc1101_write_burst_reg(0x00, (uint8_t *)defaultCfg, sizeof(defaultCfg)) != ESP_OK)
    {
        ESP_LOGE(TAG, "CC1101 could not be configured.");
        return ESP_FAIL;
    }

    /* Configure interrupt on GDO0 */
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE, // GPIO interrupt type : falling edge
        .pin_bit_mask = 1ULL << CONFIG_GDO0_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0};
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(CONFIG_GDO0_GPIO, gpio_isr_handler, (void *)CONFIG_GDO0_GPIO);

    /* Setting to RX state */
    if (cc1101_set_rx_state() != ESP_OK)
    {
        ESP_LOGE(TAG, "CC1101 could not be set to RX state.");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static inline esp_err_t get_system_time_us(uint64_t *time_us)
{
    struct timeval now;
    if (gettimeofday(&now, NULL) == -1)
    {
        return ESP_FAIL;
    }
    
    *time_us = (int64_t)now.tv_sec * 1000000L + (int64_t)now.tv_usec;

    return ESP_OK;
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

    if (rxBytes & 0x80) {
        ESP_LOGD(TAG, "RX FIFO overflow.");
        return ESP_FAIL;
    }

    // Read RX FIFO buffer
    if (cc1101_read_burst_reg(packet->rx_buffer, CC1101_RXFIFO, rxBytes) != ESP_OK)
    {
        ESP_LOGD(TAG, "Could not read RX FIFO buffer.");
        return ESP_FAIL;
    }

    // Read data length
    packet->length = packet->rx_buffer[0];             // First byte is the length
    status = packet->rx_buffer[packet->length + 2];    // Second appended status byte (LQI and CRC_OK)

    /* Is packet length ok? */
    if (packet->length > CC1101_PACKET_LEN)
    {
        ESP_LOGD(TAG, "Unexpected packet length: %d (Expected <= %d)", packet->length, CC1101_PACKET_LEN);
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

    /* Hekatron packet data starts after length byte */
    packet->data = &packet->rx_buffer[1];

    /* Set timestamp */
    get_system_time_us(&packet->timestamp);

    return ESP_OK;
}

static inline esp_err_t cc1101_write_tx_fifo(cc1101_packet_t *packet)
{
    return ESP_OK;
}

static inline esp_err_t cc1101_set_rx_state(void)
{
    ESP_LOGD(TAG, "Setting RX state.");
    return cc1101_cmd_strobe(CC1101_SRX);
}

static inline esp_err_t cc1101_set_tx_state(void)
{
    ESP_LOGD(TAG, "Setting TX state.");
    return cc1101_cmd_strobe(CC1101_STX);
}

static inline esp_err_t cc1101_flush_rx_fifo(void)
{
    esp_err_t ret;
    ESP_LOGD(TAG, "Flushing RX FIFO.");
    ret = cc1101_cmd_strobe(CC1101_SIDLE);
    ret &= cc1101_cmd_strobe(CC1101_SFRX);
    
    return ret;
}

static inline esp_err_t cc1101_flush_tx_fifo(void)
{
    esp_err_t ret;
    ESP_LOGD(TAG, "Flushing TX FIFO.");
    ret = cc1101_cmd_strobe(CC1101_SIDLE);
    ret &= cc1101_cmd_strobe(CC1101_SFTX);
    
    return ret;
}

esp_err_t cc1101_receive_data(cc1101_packet_t *packet)
{
    esp_err_t ret = cc1101_read_rx_fifo(packet);
    
    if (ret != ESP_OK)
    {
        cc1101_flush_rx_fifo();
    }

    cc1101_set_rx_state();

    return ret;
}

esp_err_t cc1101_send_data(cc1101_packet_t *packet)
{
    esp_err_t ret = cc1101_write_tx_fifo(packet);
    
    if (ret != ESP_OK)
    {
        cc1101_flush_tx_fifo();
    }

    cc1101_set_rx_state();

    return ret;
}