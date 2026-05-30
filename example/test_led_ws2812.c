/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/led.h"

static void print_usage(const char *prog)
{
    printf("Usage: %s [spidev] [num_leds] [order] [speed_hz] [reset_bytes]\n", prog);
    printf("  spidev      SPI device path (default: /dev/spidev0.0)\n");
    printf("  num_leds    LED count (default: 1)\n");
    printf("  order       grb|rgb|bgr|brg|rbg|gbr (default: grb)\n");
    printf("  speed_hz    SPI speed (default: 6400000)\n");
    printf("  reset_bytes WS2812 reset bytes (default: 80)\n");
}

static void run_ticks(struct led_dev *dev, uint32_t duration_ms, uint16_t tick_ms)
{
    uint32_t elapsed = 0;
    while (elapsed < duration_ms) {
        led_tick(dev, tick_ms);
        usleep(tick_ms * 1000U);
        elapsed += tick_ms;
    }
}

int main(int argc, char **argv)
{
    const char *dev_path = "/dev/spidev2.0";
    uint32_t num_leds = 23;
    uint32_t speed_hz = 6400000;
    uint32_t reset_bytes = 80;
    struct led_dev *dev;
    int ok = 1;

    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc > 1)
        dev_path = argv[1];
    if (argc > 2)
        num_leds = (uint32_t)strtoul(argv[2], NULL, 0);
    if (argc > 3)
        speed_hz = (uint32_t)strtoul(argv[4], NULL, 0);
    if (argc > 4)
        reset_bytes = (uint32_t)strtoul(argv[5], NULL, 0);
    if (!ok) {
        print_usage(argv[0]);
        return 1;
    }


    struct led_ws2812_spi_args {
        const char *dev_path;                   /* e.g. "/dev/spidev0.0" */
        uint32_t num_leds;                      /* number of LEDs in strip */
        uint32_t spi_speed_hz;                  /* default 6400000 */
        uint32_t reset_bytes;                   /* default 80 */
    } args = {
        .dev_path = dev_path,
        .num_leds = num_leds,
        .spi_speed_hz = speed_hz,
        .reset_bytes = reset_bytes
    };

    printf("=== LED WS2812 SPI Test ===\n\n");
    printf("[1] Creating LED device: %s, leds=%u, speed=%u, reset=%u\n",
        dev_path, num_leds, speed_hz, reset_bytes);
    dev = led_alloc_spi("spi-ws2812:strip0", &args);
    if (!dev) {
        fprintf(stderr, "Failed to create WS2812 device\n");
        return 1;
    }

    printf("[2] Red at 50%% brightness\n");
    led_set_color(dev, &LED_COLOR_RED);
    led_set_brightness(dev, 128);
    sleep(1);
    led_set_brightness(dev, 72);
    sleep(1);
    led_set_brightness(dev, 30);
    sleep(1);

    printf("[3] Green full brightness\n");
    led_set_color(dev, &LED_COLOR_GREEN);
    led_set_brightness(dev, 100);
    sleep(1);

    printf("[4] Blue on\n");
    led_set_color(dev, &LED_COLOR_BLUE);
    led_set_brightness(dev, 80);
    sleep(1);
    printf("led off\n");
    led_set_state(dev, false);
    sleep(1);

    printf("[5] Blink: 10 times, period=800ms, on=200ms\n");
    {
        struct led_blink_param blink = {
            .period_ms = 800,
            .on_ms = 200,
            .count = 10,
        };
        led_set_color(dev, &LED_COLOR_WHITE);
        led_blink(dev, &blink);
        run_ticks(dev, 10000, 50);
    }

    printf("[6] Breath define color: period=2000ms for 6s\n");
    struct led_color difine_color = {130, 200, 30};
    led_set_color(dev, &difine_color);
    led_set_brightness(dev, 80);
    led_breath(dev, 2000);
    run_ticks(dev, 6000, 50);

    printf("[7] Off\n");
    led_set_state(dev, false);

    led_free(dev);
    return 0;
}
