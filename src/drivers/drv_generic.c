/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * Generic sysfs LED driver.
 * Controls /sys/class/leds/<name>/brightness with 0 or 1.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "led_core.h"

struct led_sysfs_args {
    const char *sysfs_name;           /* e.g. "sys-led" */
    int active_level;
};

struct led_sysfs_priv {
    char name[64];
    char brightness_path[128];
    int fd;
    int active_level;
};

static int sysfs_write_value(struct led_sysfs_priv *priv, int value)
{
    char buf[4];
    int len;
    ssize_t wr;

    if (!priv || priv->fd < 0)
        return -1;

    len = snprintf(buf, sizeof(buf), "%d", value ? 1 : 0);
    if (len <= 0)
        return -1;

    if (lseek(priv->fd, 0, SEEK_SET) < 0)
        return -1;

    wr = write(priv->fd, buf, (size_t)len);
    return (wr == (ssize_t)len) ? 0 : -1;
}

static int sysfs_set_brightness(struct led_dev *dev, uint8_t brightness)
{
    struct led_sysfs_priv *priv;
    int raw_on;

    if (!dev || !dev->priv_data)
        return -1;

    priv = (struct led_sysfs_priv *)dev->priv_data;
    raw_on = (brightness > 0) ? 1 : 0;
    if (priv->active_level == 0)
        raw_on = raw_on ? 0 : 1;

    return sysfs_write_value(priv, raw_on);
}

static int sysfs_set_state(struct led_dev *dev, bool on)
{
    return sysfs_set_brightness(dev, on ? 255 : 0);
}

static void sysfs_free(struct led_dev *dev)
{
    struct led_sysfs_priv *priv;

    if (!dev)
        return;

    priv = (struct led_sysfs_priv *)dev->priv_data;
    if (priv) {
        if (priv->fd >= 0) {
            close(priv->fd);
            priv->fd = -1;
        }
        free(priv);
    }

    if (dev->name)
        free((void *)dev->name);

    free(dev);
}

static const struct led_ops sysfs_ops = {
    .set_state = sysfs_set_state,
    .set_brightness = sysfs_set_brightness,
    .set_color = NULL,
    .free = sysfs_free,
};

static struct led_dev *sysfs_factory(void *args)
{
    struct led_args *wrap = (struct led_args *)args;
    struct led_sysfs_args *cfg = NULL;
    struct led_dev *dev;
    struct led_sysfs_priv *priv;
    const char *name = NULL;
    int active = 1;
    int fd;

    if (!wrap)
        return NULL;

    if (wrap->args)
        cfg = (struct led_sysfs_args *)wrap->args;

    if (cfg && cfg->sysfs_name)
        name = cfg->sysfs_name;
    if (!name || !*name)
        name = wrap->instance;
    if (!name || !*name)
        return NULL;

    if (cfg && (cfg->active_level == 0 ||
        cfg->active_level == 1)) {
        active = cfg->active_level;
    }

    dev = led_dev_alloc(wrap->instance, sizeof(*priv));
    if (!dev)
        return NULL;

    priv = (struct led_sysfs_priv *)dev->priv_data;
    strncpy(priv->name, name, sizeof(priv->name) - 1);
    priv->name[sizeof(priv->name) - 1] = '\0';
    snprintf(priv->brightness_path, sizeof(priv->brightness_path),
        "/sys/class/leds/%s/brightness", priv->name);
    priv->active_level = active;
    priv->fd = -1;

    fd = open(priv->brightness_path, O_WRONLY);
    if (fd < 0) {
        free(priv);
        if (dev->name)
            free((void *)dev->name);
        free(dev);
        return NULL;
    }

    priv->fd = fd;
    dev->ops = &sysfs_ops;
    return dev;
}

REGISTER_LED_DRIVER("generic", LED_DRV_GENERIC, sysfs_factory);
