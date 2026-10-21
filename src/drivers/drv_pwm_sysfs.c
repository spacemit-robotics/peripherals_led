#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "led_core.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct pwm_channel {
    char device[128];
    char chip[PATH_MAX];
    uint32_t channel;
    uint64_t period_ns;
    uint64_t duty_ns;
    int acquired;
    int exported;
};

struct pwm_priv {
    char root[PATH_MAX];
    struct pwm_channel channels[LED_MAX_CHANNELS];
    int initialized;
};

static int write_value(const char *chip, const char *attribute, const char *value)
{
    char path[PATH_MAX];
    size_t length = strlen(value);
    ssize_t written;
    int fd;
    int result;

    if (snprintf(path, sizeof(path), "%s/%s", chip, attribute) >= (int)sizeof(path))
        return -ENAMETOOLONG;
    fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    do {
        written = write(fd, value, length);
    } while (written < 0 && errno == EINTR);
    result = written < 0 ? -errno : ((size_t)written == length ? 0 : -EIO);
    close(fd);
    return result;
}

static int write_channel(const struct pwm_channel *channel, const char *attribute, uint64_t value)
{
    char name[128];
    char text[32];

    snprintf(name, sizeof(name), "pwm%u/%s", channel->channel, attribute);
    snprintf(text, sizeof(text), "%" PRIu64 "\n", value);
    return write_value(channel->chip, name, text);
}

static int find_chip(const char *root, struct pwm_channel *channel)
{
    struct dirent *entry;
    DIR *directory = opendir(root);
    int result = -ENODEV;

    if (!directory)
        return -errno;
    while ((entry = readdir(directory)) != NULL) {
        char path[PATH_MAX];
        char resolved[PATH_MAX];
        const char *name;

        if (strncmp(entry->d_name, "pwmchip", 7) != 0)
            continue;
        if (snprintf(path, sizeof(path), "%s/%s/device", root, entry->d_name) >= (int)sizeof(path))
            continue;
        if (!realpath(path, resolved))
            continue;
        name = strrchr(resolved, '/');
        if (!name || strcmp(name + 1, channel->device) != 0)
            continue;
        if (snprintf(channel->chip, sizeof(channel->chip), "%s/%s", root, entry->d_name) >=
            (int)sizeof(channel->chip)) {
            result = -ENAMETOOLONG;
            break;
        }
        result = 0;
        break;
    }
    closedir(directory);
    return result;
}

static void release_channels(struct led_dev *dev)
{
    struct pwm_priv *priv = dev->priv_data;
    size_t i;

    for (i = 0; i < dev->channel_count; ++i) {
        struct pwm_channel *channel = &priv->channels[i];
        char number[32];

        if (!channel->acquired)
            continue;
        (void)write_channel(channel, "enable", 0);
        (void)write_channel(channel, "duty_cycle", 0);
        if (channel->exported) {
            snprintf(number, sizeof(number), "%u\n", channel->channel);
            (void)write_value(channel->chip, "unexport", number);
        }
        channel->acquired = channel->exported = 0;
        channel->duty_ns = 0;
    }
    priv->initialized = 0;
}

static int pwm_init(struct led_dev *dev)
{
    struct pwm_priv *priv = dev->priv_data;
    size_t i;
    int result = 0;

    if (priv->initialized)
        return 0;
    for (i = 0; i < dev->channel_count; ++i) {
        struct pwm_channel *channel = &priv->channels[i];
        char path[PATH_MAX];
        char number[32];
        char polarity[128];

        result = find_chip(priv->root, channel);
        if (result != 0)
            break;
        if (snprintf(path, sizeof(path), "%s/pwm%u", channel->chip, channel->channel) >= (int)sizeof(path)) {
            result = -ENAMETOOLONG;
            break;
        }
        if (access(path, F_OK) != 0) {
            snprintf(number, sizeof(number), "%u\n", channel->channel);
            result = write_value(channel->chip, "export", number);
            if (result != 0)
                break;
            channel->exported = 1;
        }
        channel->acquired = 1;
        result = write_channel(channel, "enable", 0);
        if (result == 0)
            result = write_channel(channel, "duty_cycle", 0);
        if (result == 0)
            result = write_channel(channel, "period", channel->period_ns);
        snprintf(polarity, sizeof(polarity), "pwm%u/polarity", channel->channel);
        if (result == 0)
            result = write_value(channel->chip, polarity, "normal\n");
        if (result != 0)
            break;
        channel->duty_ns = 0;
    }
    if (result != 0) {
        release_channels(dev);
        return result;
    }
    priv->initialized = 1;
    return 0;
}

static int pwm_set_duty(struct led_dev *dev, const uint64_t *duty_ns, size_t count)
{
    struct pwm_priv *priv = dev->priv_data;
    size_t i;

    if (!priv->initialized || count != dev->channel_count)
        return -ENODEV;
    for (i = 0; i < count; ++i) {
        if (duty_ns[i] > priv->channels[i].period_ns)
            return -ERANGE;
    }
    /* Disable channels requesting zero duty before enabling other channels. */
    for (i = 0; i < count; ++i) {
        struct pwm_channel *channel = &priv->channels[i];
        int result;

        if (duty_ns[i] != 0 || channel->duty_ns == 0)
            continue;
        result = write_channel(channel, "enable", 0);
        if (result == 0)
            result = write_channel(channel, "duty_cycle", 0);
        if (result != 0)
            return result;
        channel->duty_ns = 0;
    }
    for (i = 0; i < count; ++i) {
        struct pwm_channel *channel = &priv->channels[i];
        int result;

        if (duty_ns[i] == channel->duty_ns)
            continue;
        result = write_channel(channel, "duty_cycle", duty_ns[i]);
        if (result == 0 && channel->duty_ns == 0)
            result = write_channel(channel, "enable", 1);
        if (result != 0)
            return result;
        channel->duty_ns = duty_ns[i];
    }
    return 0;
}

static int pwm_set_brightness(struct led_dev *dev, uint8_t brightness)
{
    struct pwm_priv *priv = dev->priv_data;
    uint64_t duty_ns[LED_MAX_CHANNELS];
    size_t i;

    for (i = 0; i < dev->channel_count; ++i) {
        uint64_t period = priv->channels[i].period_ns;

        duty_ns[i] = (period / 255U) * brightness + (period % 255U) * brightness / 255U;
    }
    return pwm_set_duty(dev, duty_ns, dev->channel_count);
}

static int pwm_set_state(struct led_dev *dev, bool on)
{
    return pwm_set_brightness(dev, on ? 255U : 0U);
}

static void pwm_free(struct led_dev *dev)
{
    release_channels(dev);
    free(dev->priv_data);
    free((void *)dev->name);
    free(dev);
}

static const struct led_ops g_pwm_ops = {
    .init = pwm_init,
    .set_state = pwm_set_state,
    .set_brightness = pwm_set_brightness,
    .set_duty_ns = pwm_set_duty,
    .free = pwm_free,
};

static struct led_dev *pwm_factory(void *args)
{
    const struct led_args_pwm *pwm_args = args;
    const struct led_pwm_config *config;
    struct pwm_priv *priv;
    struct led_dev *dev;
    const char *root;
    size_t i;

    if (!pwm_args || !pwm_args->config)
        return NULL;
    config = pwm_args->config;
    root = config->sysfs_root ? config->sysfs_root : "/sys/class/pwm";
    if (!*root || strlen(root) >= sizeof(priv->root) ||
        config->channel_count == 0 || config->channel_count > LED_MAX_CHANNELS)
        return NULL;
    for (i = 0; i < config->channel_count; ++i) {
        const struct led_pwm_channel *channel = &config->channels[i];
        size_t j;

        if (!channel->device || !*channel->device || strchr(channel->device, '/') ||
            strlen(channel->device) >= sizeof(priv->channels[i].device) || channel->period_ns == 0)
            return NULL;
        for (j = 0; j < i; ++j) {
            if (channel->channel == config->channels[j].channel &&
                strcmp(channel->device, config->channels[j].device) == 0)
                return NULL;
        }
    }
    dev = led_dev_alloc(pwm_args->instance, sizeof(*priv));
    if (!dev)
        return NULL;
    priv = dev->priv_data;
    snprintf(priv->root, sizeof(priv->root), "%s", root);
    for (i = 0; i < config->channel_count; ++i) {
        snprintf(priv->channels[i].device, sizeof(priv->channels[i].device), "%s", config->channels[i].device);
        priv->channels[i].channel = config->channels[i].channel;
        priv->channels[i].period_ns = config->channels[i].period_ns;
    }
    dev->channel_count = config->channel_count;
    dev->ops = &g_pwm_ops;
    return dev;
}

REGISTER_LED_DRIVER("drv_pwm_sysfs", LED_DRV_PWM, pwm_factory);
