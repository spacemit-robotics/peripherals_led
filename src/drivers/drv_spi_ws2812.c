/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * SPI WS2812/SK6812 LED strip driver (userspace).
 * Encodes GRB data into SPI bit patterns and writes via spidev.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "led_core.h"

#define WS2812_BIT_0        0xC0    /* 11000000 */
#define WS2812_BIT_1        0xF8    /* 11111000 */

#define BYTES_PER_COLOR     8
#define BYTES_PER_LED       (3 * BYTES_PER_COLOR)
#define DEFAULT_SPI_SPEED   6400000U
#define DEFAULT_RESET_BYTES 80U
#define DEFAULT_NUM_LEDS    1U

struct led_ws2812_spi_args {
    const char *dev_path;                   /* e.g. "/dev/spidev0.0" */
    uint32_t num_leds;                      /* number of LEDs in strip */
    uint32_t spi_speed_hz;                  /* default 6400000 */
    uint32_t reset_bytes;                   /* default 80 */
};

struct ws2812_spi_priv {
    char dev_path[64];
    uint32_t num_leds;
    uint32_t spi_speed;
    uint32_t reset_bytes;
    int fd;
    uint8_t *tx_buf;
    size_t tx_len;
};

static void ws2812_byte_to_spi(uint8_t byte, uint8_t *out)
{
    int i;

    for (i = 7; i >= 0; i--) {
        *out++ = (byte & (1U << i)) ? WS2812_BIT_1 : WS2812_BIT_0;
    }
}

static void ws2812_led_to_spi(const struct led_color *color, uint8_t brightness, uint8_t *out)
{
    uint8_t r, g, b;

    r = (uint8_t)((uint32_t)color->r * brightness / 255U);
    g = (uint8_t)((uint32_t)color->g * brightness / 255U);
    b = (uint8_t)((uint32_t)color->b * brightness / 255U);

    ws2812_byte_to_spi(g, out);
    ws2812_byte_to_spi(r, out + BYTES_PER_COLOR);
    ws2812_byte_to_spi(b, out + 2 * BYTES_PER_COLOR);
}

static int ws2812_spi_xfer(struct ws2812_spi_priv *priv)
{
    struct spi_ioc_transfer xfer;
    int ret;

    memset(&xfer, 0, sizeof(xfer));
    xfer.tx_buf = (uint64_t)(uintptr_t)priv->tx_buf;
    xfer.len = priv->tx_len;
    xfer.speed_hz = priv->spi_speed;
    xfer.bits_per_word = 8;

    ret = ioctl(priv->fd, SPI_IOC_MESSAGE(1), &xfer);
    if (ret < 0)
        return -1;
    return 0;
}

static int ws2812_update(struct led_dev *dev)
{
    struct ws2812_spi_priv *priv;
    size_t i;
    size_t data_len;
    uint8_t *ptr;

    if (!dev || !dev->priv_data)
        return -1;

    priv = (struct ws2812_spi_priv *)dev->priv_data;
    if (priv->fd < 0 || !priv->tx_buf)
        return -1;

    data_len = (size_t)priv->num_leds * BYTES_PER_LED;
    ptr = priv->tx_buf;
    for (i = 0; i < priv->num_leds; i++) {
        ws2812_led_to_spi(&dev->color, dev->brightness, ptr);
        ptr += BYTES_PER_LED;
    }

    if (priv->reset_bytes > 0)
        memset(priv->tx_buf + data_len, 0, priv->reset_bytes);

    return ws2812_spi_xfer(priv);
}

static int ws2812_set_brightness(struct led_dev *dev, uint8_t brightness)
{
    (void)brightness;
    return ws2812_update(dev);
}

static int ws2812_set_color(struct led_dev *dev, const struct led_color *color)
{
    (void)color;
    return ws2812_update(dev);
}

static int ws2812_set_state(struct led_dev *dev, bool on)
{
    (void)on;
    return ws2812_update(dev);
}

static void ws2812_free(struct led_dev *dev)
{
    struct ws2812_spi_priv *priv;

    if (!dev)
        return;

    priv = (struct ws2812_spi_priv *)dev->priv_data;
    if (priv) {
        if (priv->fd >= 0) {
            close(priv->fd);
            priv->fd = -1;
        }
        free(priv->tx_buf);
        free(priv);
    }

    if (dev->name)
        free((void *)dev->name);

    free(dev);
}

static const struct led_ops ws2812_spi_ops = {
    .set_state = ws2812_set_state,
    .set_brightness = ws2812_set_brightness,
    .set_color = ws2812_set_color,
    .free = ws2812_free,
};

static int ws2812_spi_setup(int fd, uint32_t speed_hz)
{
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = speed_hz;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0)
        return -1;
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
        return -1;
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
        return -1;

    return 0;
}

static struct led_dev *ws2812_spi_factory(void *args)
{
    struct led_args *wrap = (struct led_args *)args;
    struct led_ws2812_spi_args *cfg = NULL;
    struct led_dev *dev;
    struct ws2812_spi_priv *priv;
    uint32_t num_leds;
    uint32_t reset_bytes;
    size_t data_len;
    size_t tx_len;
    int fd;

    if (!wrap || !wrap->args)
        goto err_args;

    cfg = (struct led_ws2812_spi_args *)wrap->args;
    if (!cfg->dev_path || !*cfg->dev_path)
        goto err_args;

    num_leds = (cfg->num_leds > 0) ? cfg->num_leds : DEFAULT_NUM_LEDS;
    reset_bytes = (cfg->reset_bytes > 0) ? cfg->reset_bytes : DEFAULT_RESET_BYTES;

    if (num_leds > (SIZE_MAX - reset_bytes) / BYTES_PER_LED)
        goto err_args;

    data_len = (size_t)num_leds * BYTES_PER_LED;
    tx_len = data_len + reset_bytes;

    dev = led_dev_alloc(wrap->instance, sizeof(*priv));
    if (!dev)
        goto err_alloc;

    priv = (struct ws2812_spi_priv *)dev->priv_data;
    strncpy(priv->dev_path, cfg->dev_path, sizeof(priv->dev_path) - 1);
    priv->dev_path[sizeof(priv->dev_path) - 1] = '\0';
    priv->num_leds = num_leds;
    priv->spi_speed = (cfg->spi_speed_hz > 0) ? cfg->spi_speed_hz : DEFAULT_SPI_SPEED;
    priv->reset_bytes = reset_bytes;
    priv->fd = -1;
    priv->tx_buf = NULL;
    priv->tx_len = tx_len;

    fd = open(priv->dev_path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "[LED-WS2812] open %s failed: %s\n",
            priv->dev_path, strerror(errno));
        ws2812_free(dev);
        return NULL;
    }

    if (ws2812_spi_setup(fd, priv->spi_speed) < 0) {
        fprintf(stderr, "[LED-WS2812] spi setup failed on %s: %s\n",
            priv->dev_path, strerror(errno));
        close(fd);
        ws2812_free(dev);
        return NULL;
    }

    priv->fd = fd;
    priv->tx_buf = (uint8_t *)calloc(1, priv->tx_len);
    if (!priv->tx_buf) {
        fprintf(stderr, "[LED-WS2812] alloc tx buffer failed (len=%zu)\n", priv->tx_len);
        ws2812_free(dev);
        return NULL;
    }

    dev->ops = &ws2812_spi_ops;

    dev->color = LED_COLOR_BLACK;
    dev->brightness = 0;
    (void)ws2812_update(dev);

    return dev;

err_args:
    fprintf(stderr, "[LED-WS2812] invalid args\n");
err_alloc:
    return NULL;
}

REGISTER_LED_DRIVER("spi-ws2812", LED_DRV_SPI, ws2812_spi_factory);
