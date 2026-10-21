/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LED_H
#define LED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RGB color */
struct led_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

#define LED_COLOR_RED    ((struct led_color){255, 0, 0})
#define LED_COLOR_GREEN  ((struct led_color){0, 255, 0})
#define LED_COLOR_BLUE   ((struct led_color){0, 0, 255})
#define LED_COLOR_WHITE  ((struct led_color){255, 255, 255})
#define LED_COLOR_BLACK  ((struct led_color){0, 0, 0})

/* blink parameters */
struct led_blink_param {
    uint16_t period_ms;  /* blink period, e.g. 1000ms */
    uint16_t on_ms;      /* on duration, e.g. 100ms */
    uint8_t count;       /* repeat count, 0=infinite */
};

struct led_dev; /* opaque handle */

#define LED_MAX_CHANNELS 8U

struct led_pwm_channel {
    const char *device;  /* PWM platform device name, independent of pwmchip numbering. */
    uint32_t channel;
    uint64_t period_ns;
};

struct led_pwm_config {
    const char *sysfs_root;  /* NULL selects /sys/class/pwm. */
    struct led_pwm_channel channels[LED_MAX_CHANNELS];
    size_t channel_count;
};

/* allocate LED device */
struct led_dev *led_alloc_generic(const char *name, void *args);
struct led_dev *led_alloc_spi(const char *name, void *args);
/* PWM allocation performs no sysfs I/O; led_init() acquires the channels, initially off. */
struct led_dev *led_alloc_pwm(const char *driver, const char *instance,
    const struct led_pwm_config *config);
int led_init(struct led_dev *dev);
size_t led_channel_count(const struct led_dev *dev);
/* Set per-channel high time in nanoseconds; zero duty disables the channel. */
int led_set_duty_ns(struct led_dev *dev, const uint64_t *duty_ns, size_t count);


/* basic control */
void led_set_state(struct led_dev *dev, bool on);
void led_toggle(struct led_dev *dev);
void led_set_brightness(struct led_dev *dev, uint8_t brightness);
void led_set_color(struct led_dev *dev, const struct led_color *color);

/* effects */
void led_blink(struct led_dev *dev, const struct led_blink_param *param);
void led_breath(struct led_dev *dev, uint16_t period_ms);

/* state machine tick (call at 10Hz~50Hz) */
void led_tick(struct led_dev *dev, uint16_t dt_ms);

/* free device */
void led_free(struct led_dev *dev);

#ifdef __cplusplus
}
#endif

#endif /* LED_H */
