/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "led_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static struct led_driver_info *g_driver_list = NULL;

static int led_apply_state(struct led_dev *dev, bool on)
{
    if (!dev || !dev->ops)
        return -1;

    if (dev->ops->set_state)
        return dev->ops->set_state(dev, on);
    if (dev->ops->set_brightness)
        return dev->ops->set_brightness(dev, on ? 255 : 0);
    if (dev->ops->set_color)
        return dev->ops->set_color(dev, on ? &LED_COLOR_WHITE : &LED_COLOR_BLACK);

    return -1;
}

static int led_apply_brightness(struct led_dev *dev, uint8_t brightness)
{
    if (!dev || !dev->ops)
        return -1;

    if (dev->ops->set_brightness)
        return dev->ops->set_brightness(dev, brightness);
    if (dev->ops->set_state)
        return dev->ops->set_state(dev, brightness > 0);

    return -1;
}

static int led_apply_color(struct led_dev *dev, const struct led_color *color)
{
    if (!dev || !dev->ops || !color)
        return -1;

    if (dev->ops->set_color)
        return dev->ops->set_color(dev, color);
    if (dev->ops->set_brightness) {
        uint8_t maxc = color->r;
        if (color->g > maxc) maxc = color->g;
        if (color->b > maxc) maxc = color->b;
        return dev->ops->set_brightness(dev, maxc);
    }
    if (dev->ops->set_state)
        return dev->ops->set_state(dev, (color->r | color->g | color->b) != 0);

    return -1;
}

struct led_dev *led_dev_alloc(const char *name, size_t priv_size)
{
    struct led_dev *dev;
    void *priv = NULL;
    char *name_copy = NULL;

    dev = calloc(1, sizeof(*dev));
    if (!dev)
        return NULL;

    if (priv_size) {
        priv = calloc(1, priv_size);
        if (!priv) {
            free(dev);
            return NULL;
        }
        dev->priv_data = priv;
    }

    if (name) {
        size_t n = strlen(name);
        name_copy = calloc(1, n + 1);
        if (!name_copy) {
            free(priv);
            free(dev);
            return NULL;
        }
        memcpy(name_copy, name, n);
        name_copy[n] = '\0';
        dev->name = name_copy;
    }

    dev->state_on = false;
    dev->brightness = 0;
    dev->color = LED_COLOR_BLACK;
    dev->effect = LED_EFFECT_NONE;

    return dev;
}

void led_driver_register(struct led_driver_info *info)
{
    if (!info)
        return;
    info->next = g_driver_list;
    g_driver_list = info;
}

static struct led_driver_info *find_driver(const char *name, enum led_driver_type type)
{
    struct led_driver_info *curr = g_driver_list;
    while (curr) {
        if (curr->name && name && strcmp(curr->name, name) == 0) {
            if (curr->type == type)
                return curr;
            printf("[LED] driver '%s' type mismatch (expected %d got %d)\n",
                name, (int)type, (int)curr->type);
            return NULL;
        }
        curr = curr->next;
    }
    printf("[LED] driver '%s' not found\n", name ? name : "(null)");
    return NULL;
}

static int split_driver_instance(const char *name,
    char *driver, size_t driver_sz,
    const char **instance)
{
    const char *sep;
    size_t len;

    if (!name || !driver || !driver_sz || !instance)
        return -1;

    sep = strchr(name, ':');
    if (!sep)
        return 0;

    len = (size_t)(sep - name);
    if (len == 0 || len + 1 > driver_sz || !*(sep + 1))
        return -1;

    memcpy(driver, name, len);
    driver[len] = '\0';
    *instance = sep + 1;
    return 1;
}

struct led_dev *led_alloc_generic(const char *name, void *args)
{
    struct led_driver_info *drv;
    struct led_args wrap;
    char driver[32];
    const char *instance = NULL;
    int r;

    if (!name)
        return NULL;

    strncpy(driver, "generic", sizeof(driver) - 1);
    driver[sizeof(driver) - 1] = '\0';
    instance = name;

    r = split_driver_instance(name, driver, sizeof(driver), &instance);
    if (r < 0)
        return NULL;

    drv = find_driver(driver, LED_DRV_GENERIC);
    if (!drv || !drv->factory)
        return NULL;

    wrap.instance = instance;
    wrap.args = args;
    return drv->factory(&wrap);
}

struct led_dev *led_alloc_spi(const char *name, void *args)
{
    struct led_driver_info *drv;
    struct led_args wrap;
    char driver[32];
    const char *instance = NULL;
    int r;

    if (!name)
        return NULL;

    strncpy(driver, "spi", sizeof(driver) - 1);
    driver[sizeof(driver) - 1] = '\0';
    instance = name;

    r = split_driver_instance(name, driver, sizeof(driver), &instance);
    if (r < 0)
        return NULL;

    drv = find_driver(driver, LED_DRV_SPI);
    if (!drv || !drv->factory)
        return NULL;

    wrap.instance = instance;
    wrap.args = args;
    return drv->factory(&wrap);
}

void led_set_state(struct led_dev *dev, bool on)
{
    if (!dev)
        return;

    dev->effect = LED_EFFECT_NONE;
    dev->state_on = on;
    dev->brightness = on ? 255 : 0;
    dev->color = on ? LED_COLOR_WHITE : LED_COLOR_BLACK;
    (void)led_apply_state(dev, on);
}

void led_toggle(struct led_dev *dev)
{
    if (!dev)
        return;
    led_set_state(dev, !dev->state_on);
}

void led_set_brightness(struct led_dev *dev, uint8_t brightness)
{
    if (!dev)
        return;

    dev->effect = LED_EFFECT_NONE;
    dev->brightness = brightness;
    dev->state_on = (brightness > 0);
    (void)led_apply_brightness(dev, brightness);
}

void led_set_color(struct led_dev *dev, const struct led_color *color)
{
    uint8_t maxc;

    if (!dev || !color)
        return;

    dev->effect = LED_EFFECT_NONE;
    dev->color = *color;
    maxc = color->r;
    if (color->g > maxc) maxc = color->g;
    if (color->b > maxc) maxc = color->b;
    dev->brightness = maxc;
    dev->state_on = (maxc > 0);
    (void)led_apply_color(dev, color);
}

void led_blink(struct led_dev *dev, const struct led_blink_param *param)
{
    if (!dev || !param || param->period_ms == 0)
        return;

    dev->effect = LED_EFFECT_BLINK;
    dev->blink = *param;
    if (dev->blink.on_ms > dev->blink.period_ms)
        dev->blink.on_ms = dev->blink.period_ms;
    dev->blink_elapsed_ms = 0;
    dev->blink_done = 0;
    dev->blink_on = (dev->blink.on_ms > 0);

    dev->state_on = dev->blink_on;
    dev->brightness = dev->blink_on ? 255 : 0;
    (void)led_apply_state(dev, dev->blink_on);
}

void led_breath(struct led_dev *dev, uint16_t period_ms)
{
    if (!dev || period_ms == 0)
        return;

    dev->effect = LED_EFFECT_BREATH;
    dev->breath_period_ms = period_ms;
    dev->breath_elapsed_ms = 0;
    dev->brightness = 0;
    dev->state_on = false;
    (void)led_apply_brightness(dev, 0);
}

void led_tick(struct led_dev *dev, uint16_t dt_ms)
{
    if (!dev || dev->effect == LED_EFFECT_NONE || dt_ms == 0)
        return;

    if (dev->effect == LED_EFFECT_BLINK) {
        uint16_t period = dev->blink.period_ms;
        uint16_t on_ms = dev->blink.on_ms;
        uint32_t total;
        uint32_t cycles;
        bool on;

        if (period == 0) {
            dev->effect = LED_EFFECT_NONE;
            return;
        }

        total = (uint32_t)dev->blink_elapsed_ms + (uint32_t)dt_ms;
        cycles = total / period;
        dev->blink_elapsed_ms = (uint16_t)(total % period);

        if (dev->blink.count != 0 && cycles > 0) {
            if ((uint32_t)dev->blink_done + cycles >= dev->blink.count) {
                dev->blink_done = dev->blink.count;
                dev->effect = LED_EFFECT_NONE;
                dev->state_on = false;
                dev->brightness = 0;
                dev->blink_on = false;
                (void)led_apply_state(dev, false);
                return;
            }
            dev->blink_done = (uint8_t)(dev->blink_done + cycles);
        }

        on = (dev->blink_elapsed_ms < on_ms);
        if (on != dev->blink_on) {
            dev->blink_on = on;
            dev->state_on = on;
            dev->brightness = on ? 255 : 0;
            (void)led_apply_state(dev, on);
        }
        return;
    }

    if (dev->effect == LED_EFFECT_BREATH) {
        uint16_t period = dev->breath_period_ms;
        uint16_t half;
        uint32_t t;
        uint8_t brightness;

        if (period == 0) {
            dev->effect = LED_EFFECT_NONE;
            return;
        }

        t = (uint32_t)dev->breath_elapsed_ms + (uint32_t)dt_ms;
        if (t >= period)
            t = t % period;
        dev->breath_elapsed_ms = (uint16_t)t;

        half = (uint16_t)(period / 2);
        if (half == 0) {
            brightness = 255;
        } else if (t < half) {
            brightness = (uint8_t)((uint32_t)255 * t / half);
        } else {
            brightness = (uint8_t)((uint32_t)255 * (period - t) / half);
        }

        if (brightness != dev->brightness) {
            dev->brightness = brightness;
            dev->state_on = (brightness > 0);
            (void)led_apply_brightness(dev, brightness);
        }
    }
}

void led_free(struct led_dev *dev)
{
    if (!dev)
        return;

    if (dev->ops && dev->ops->free) {
        dev->ops->free(dev);
        return;
    }

    if (dev->priv_data)
        free(dev->priv_data);
    if (dev->name)
        free((void *)dev->name);
    free(dev);
}
