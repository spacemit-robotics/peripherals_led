/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/led.h"

static volatile sig_atomic_t interrupted;

static void stop_test(int signal_number)
{
    (void)signal_number;
    interrupted = 1;
}

static void print_usage(const char *program)
{
    printf("Usage: %s <device> <channel> <period_ns> <duty_ns> [<device> <channel> <period_ns> <duty_ns> ...]\n",
        program);
    printf("  device: PWM platform device name; configure up to %u channels.\n", LED_MAX_CHANNELS);
    printf("  Applies the requested duty for two seconds, then disables all channels.\n");
}

static int parse_number(const char *text, uint64_t maximum, uint64_t *value)
{
    char *end;
    uintmax_t parsed;

    if (!text || text[0] < '0' || text[0] > '9')
        return -1;
    errno = 0;
    parsed = strtoumax(text, &end, 10);
    if (errno || *end || parsed > maximum)
        return -1;
    *value = (uint64_t)parsed;
    return 0;
}

int main(int argc, char **argv)
{
    struct led_pwm_config config = {0};
    uint64_t duty_ns[LED_MAX_CHANNELS] = {0};
    struct led_dev *dev;
    size_t i;
    int result;

    if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        print_usage(argv[0]);
        return 0;
    }
    if (argc < 5 || (argc - 1) % 4 || (argc - 1) / 4 > (int)LED_MAX_CHANNELS) {
        print_usage(argv[0]);
        return 2;
    }
    config.channel_count = (size_t)(argc - 1) / 4;
    for (i = 0; i < config.channel_count; ++i) {
        struct led_pwm_channel *channel = &config.channels[i];
        char **args = &argv[1 + 4 * i];
        uint64_t index;

        channel->device = args[0];
        if (parse_number(args[1], UINT32_MAX, &index) ||
            parse_number(args[2], UINT64_MAX, &channel->period_ns) || !channel->period_ns ||
            parse_number(args[3], channel->period_ns, &duty_ns[i])) {
            fprintf(stderr, "Invalid PWM channel, period or duty in group %zu\n", i + 1);
            return 2;
        }
        channel->channel = (uint32_t)index;
    }
    signal(SIGINT, stop_test);
    signal(SIGTERM, stop_test);
    dev = led_alloc_pwm("drv_pwm_sysfs", "pwm-test", &config);
    if (!dev) {
        fprintf(stderr, "Failed to allocate PWM device; check enabled driver and channel configuration\n");
        return 1;
    }
    result = led_init(dev);
    if (result != 0)
        goto done;
    if (!interrupted) {
        result = led_set_duty_ns(dev, duty_ns, config.channel_count);
        if (result != 0)
            goto done;
        for (i = 0; i < config.channel_count; ++i) {
            printf("%s channel=%u period=%" PRIu64 "ns duty=%" PRIu64 "ns\n",
                config.channels[i].device, config.channels[i].channel,
                config.channels[i].period_ns, duty_ns[i]);
        }
        sleep(2);
    }
    memset(duty_ns, 0, sizeof(duty_ns));
    result = led_set_duty_ns(dev, duty_ns, config.channel_count);
done:
    led_free(dev);
    if (result != 0) {
        fprintf(stderr, "PWM test failed: %s (%d)\n", strerror(-result), result);
        return 1;
    }
    puts("PWM test completed; channels disabled");
    return 0;
}
