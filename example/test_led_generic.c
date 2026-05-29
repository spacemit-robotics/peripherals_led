/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/led.h"

static void print_usage(const char *prog)
{
    printf("Usage: %s [sysfs_name] [active_level]\n", prog);
    printf("  sysfs_name   LED node name in /sys/class/leds (default: sys-led)\n");
    printf("  active_level high|low|1|0 (default: high)\n");
}

static int parse_active_level(const char *s, int *ok)
{
    if (!s || !*s) {
        if (ok) *ok = 0;
        return 1;
    }
    if (strcmp(s, "high") == 0 || strcmp(s, "1") == 0) {
        if (ok) *ok = 1;
        return 1;
    }
    if (strcmp(s, "low") == 0 || strcmp(s, "0") == 0) {
        if (ok) *ok = 1;
        return 0;
    }
    if (ok) *ok = 0;
    return 1;
}

static int expected_raw_value(int active_level, bool on)
{
    int raw_on = on ? 1 : 0;

    if (active_level == 0)
        raw_on = raw_on ? 0 : 1;

    return raw_on;
}

static int read_brightness(const char *sysfs_name, int *value)
{
    char path[160];
    FILE *fp;
    int parsed;

    if (!sysfs_name || !value)
        return -1;

    snprintf(path, sizeof(path), "/sys/class/leds/%s/brightness", sysfs_name);
    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to read %s: %s\n", path, strerror(errno));
        return -1;
    }

    parsed = fscanf(fp, "%d", value);
    fclose(fp);
    if (parsed != 1) {
        fprintf(stderr, "Failed to parse brightness from %s\n", path);
        return -1;
    }

    return 0;
}

static int expect_brightness(const char *step, const char *sysfs_name,
    int active_level, bool on)
{
    int actual = -1;
    int expected = expected_raw_value(active_level, on);

    if (read_brightness(sysfs_name, &actual) != 0)
        return -1;

    printf("[assert] %s brightness=%d expected=%d\n", step, actual, expected);
    if (actual != expected) {
        fprintf(stderr, "Unexpected brightness during %s: got %d expected %d\n",
            step, actual, expected);
        return -1;
    }

    return 0;
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
    const char *sysfs_name = "sys-led_1";
    int active_level = 0;

    struct led_dev *dev;
    int ok = 1;

    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc > 1)
        sysfs_name = argv[1];
    if (argc > 2)
        active_level = parse_active_level(argv[2], &ok);
    if (!ok) {
        print_usage(argv[0]);
        return 1;
    }

    struct led_sysfs_args {
        const char *sysfs_name;           /* e.g. "sys-led" */
        int active_level;
    }args = {
        .sysfs_name = sysfs_name,
        .active_level = active_level,
    };

    printf("=== LED GENERIC Test ===\n\n");
    printf("[1] Creating LED device: %s (active %s)\n",
        sysfs_name,
        (active_level == 0) ? "low" : "high");
    dev = led_alloc_generic(sysfs_name, &args);
    if (!dev) {
        fprintf(stderr, "Failed to create LED device\n");
        return 1;
    }

    printf("[2] ON for 1s\n");
    led_set_state(dev, true);
    sleep(1);
    if (expect_brightness("ON", sysfs_name, active_level, true) != 0)
        goto fail;

    printf("[3] OFF for 1s\n");
    led_set_state(dev, false);
    sleep(1);
    if (expect_brightness("OFF", sysfs_name, active_level, false) != 0)
        goto fail;

    printf("[4] Toggle twice\n");
    led_toggle(dev);
    sleep(1);
    if (expect_brightness("toggle-on", sysfs_name, active_level, true) != 0)
        goto fail;
    led_toggle(dev);
    sleep(1);
    if (expect_brightness("toggle-off", sysfs_name, active_level, false) != 0)
        goto fail;

    printf("[5] Blink: 5 times, period=1000ms, on=200ms\n");
    {
        struct led_blink_param blink = {
            .period_ms = 1000,
            .on_ms = 200,
            .count = 5,
        };
        led_blink(dev, &blink);
        if (expect_brightness("blink-on", sysfs_name, active_level, true) != 0)
            goto fail;
        led_tick(dev, 250);
        if (expect_brightness("blink-off", sysfs_name, active_level, false) != 0)
            goto fail;
        usleep(250 * 1000U);
        run_ticks(dev, 5250, 50);
        if (expect_brightness("blink-complete", sysfs_name, active_level, false) != 0)
            goto fail;
    }

    printf("[6] Breath: period=2000ms for 6s\n");
    led_breath(dev, 2000);
    if (expect_brightness("breath-start", sysfs_name, active_level, false) != 0)
        goto fail;
    led_tick(dev, 500);
    if (expect_brightness("breath-rise", sysfs_name, active_level, true) != 0)
        goto fail;
    usleep(500 * 1000U);
    run_ticks(dev, 5500, 50);
    if (expect_brightness("breath-complete", sysfs_name, active_level, false) != 0)
        goto fail;

    printf("[7] OFF\n");
    led_set_state(dev, false);
    if (expect_brightness("final-off", sysfs_name, active_level, false) != 0)
        goto fail;

    printf("[assert] sysfs brightness assertions PASSED\n");
    led_free(dev);
    return 0;

fail:
    led_free(dev);
    return 1;
}
