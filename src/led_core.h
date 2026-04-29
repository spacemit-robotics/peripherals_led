/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LED_CORE_H
#define LED_CORE_H

/*
 * Private header for LED component (nfc-like minimal style).
 */

#include "../include/led.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* 1. Driver type enum */
enum led_driver_type {
    LED_DRV_GENERIC = 0,
    LED_DRV_SPI,
};

/* 2. Ops table (driver implementation) */
struct led_ops {
    int (*set_state)(struct led_dev *dev, bool on);
    int (*set_brightness)(struct led_dev *dev, uint8_t brightness);
    int (*set_color)(struct led_dev *dev, const struct led_color *color);
    void (*free)(struct led_dev *dev);
};

/* 3. Device object (private implementation) */
enum led_effect_type {
    LED_EFFECT_NONE = 0,
    LED_EFFECT_BLINK,
    LED_EFFECT_BREATH,
};

struct led_dev {
    const char *name; /* instance name */
    const struct led_ops *ops;
    void *priv_data;

    bool state_on;
    uint8_t brightness;
    struct led_color color;

    enum led_effect_type effect;
    struct led_blink_param blink;
    uint16_t blink_elapsed_ms;
    uint8_t blink_done;
    bool blink_on;

    uint16_t breath_period_ms;
    uint16_t breath_elapsed_ms;
};

/* 4. Generic factory function type */
typedef struct led_dev *(*led_factory_t)(void *args);

/* 5. Driver registry node */
struct led_driver_info {
    const char *name;             /* driver name */
    enum led_driver_type type;
    led_factory_t factory;
    struct led_driver_info *next;
};

void led_driver_register(struct led_driver_info *info);

#define REGISTER_LED_DRIVER(_name, _type, _factory) \
    static struct led_driver_info __drv_info_##_factory = { \
        .name = _name, \
        .type = _type, \
        .factory = _factory, \
        .next = 0 \
    }; \
    __attribute__((constructor)) \
    static void __auto_reg_##_factory(void) { \
        led_driver_register(&__drv_info_##_factory); \
    }

/* 6. Common wrapper passed to factory */
struct led_args {
    const char *instance;
    void *args;
};

struct led_dev *led_dev_alloc(const char *instance, size_t priv_size);

#endif /* LED_CORE_H */
