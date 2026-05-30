/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "led_core.h"

struct mock_led_priv {
    int state_calls;
    int brightness_calls;
    int color_calls;
    bool last_on;
    uint8_t last_brightness;
    struct led_color last_color;
};

static int g_failures;
static int g_free_count;

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL:%s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); \
        g_failures++; \
    } \
} while (0)

#define CHECK_INT_EQ(actual, expected) do { \
    int _actual = (int)(actual); \
    int _expected = (int)(expected); \
    if (_actual != _expected) { \
        printf("FAIL:%s:%d: expected %s == %d, got %d\n", \
            __FILE__, __LINE__, #actual, _expected, _actual); \
        g_failures++; \
    } \
} while (0)

static void reset_test_state(void)
{
    g_failures = 0;
    g_free_count = 0;
}

static int mock_set_state(struct led_dev *dev, bool on)
{
    struct mock_led_priv *priv;

    if (!dev || !dev->priv_data)
        return -1;

    priv = dev->priv_data;
    priv->state_calls++;
    priv->last_on = on;
    return 0;
}

static int mock_set_brightness(struct led_dev *dev, uint8_t brightness)
{
    struct mock_led_priv *priv;

    if (!dev || !dev->priv_data)
        return -1;

    priv = dev->priv_data;
    priv->brightness_calls++;
    priv->last_brightness = brightness;
    return 0;
}

static int mock_set_color(struct led_dev *dev, const struct led_color *color)
{
    struct mock_led_priv *priv;

    if (!dev || !dev->priv_data || !color)
        return -1;

    priv = dev->priv_data;
    priv->color_calls++;
    priv->last_color = *color;
    return 0;
}

static void mock_free(struct led_dev *dev)
{
    if (!dev)
        return;

    g_free_count++;
    free(dev->priv_data);
    free((void *)dev->name);
    free(dev);
}

static const struct led_ops mock_ops = {
    .set_state = mock_set_state,
    .set_brightness = mock_set_brightness,
    .set_color = mock_set_color,
    .free = mock_free,
};

static struct led_dev *mock_factory(void *args)
{
    struct led_args *led_args = args;
    struct led_dev *dev;

    if (!led_args || !led_args->instance)
        return NULL;

    dev = led_dev_alloc(led_args->instance, sizeof(struct mock_led_priv));
    if (!dev)
        return NULL;

    dev->ops = &mock_ops;
    return dev;
}

REGISTER_LED_DRIVER("MOCK", LED_DRV_GENERIC, mock_factory);

static void test_error_paths(void)
{
    struct led_color color = LED_COLOR_RED;
    struct led_blink_param blink = {
        .period_ms = 0,
        .on_ms = 10,
        .count = 1,
    };

    led_set_state(NULL, true);
    led_toggle(NULL);
    led_set_brightness(NULL, 1);
    led_set_color(NULL, &color);
    led_set_color(NULL, NULL);
    led_blink(NULL, &blink);
    led_blink(NULL, NULL);
    led_breath(NULL, 100);
    led_tick(NULL, 10);
    led_free(NULL);

    CHECK_TRUE(led_alloc_generic(NULL, NULL) == NULL);
    CHECK_TRUE(led_alloc_generic("MOCK:", NULL) == NULL);
    CHECK_TRUE(led_alloc_generic(":led0", NULL) == NULL);
    CHECK_TRUE(led_alloc_generic("MISSING:led0", NULL) == NULL);
    CHECK_TRUE(led_alloc_spi("MOCK:led0", NULL) == NULL);
}

static void test_functional(void)
{
    struct led_dev *dev;
    struct mock_led_priv *priv;
    struct led_color color = {
        .r = 3,
        .g = 9,
        .b = 1,
    };
    struct led_blink_param blink = {
        .period_ms = 100,
        .on_ms = 40,
        .count = 2,
    };

    dev = led_alloc_generic("MOCK:led0", NULL);
    CHECK_TRUE(dev != NULL);
    if (!dev)
        return;
    priv = dev->priv_data;

    led_set_state(dev, true);
    CHECK_TRUE(dev->state_on);
    CHECK_INT_EQ(dev->brightness, 255);
    CHECK_TRUE(priv->last_on);

    led_toggle(dev);
    CHECK_TRUE(!dev->state_on);
    CHECK_INT_EQ(dev->brightness, 0);
    CHECK_TRUE(!priv->last_on);

    led_set_brightness(dev, 17);
    CHECK_TRUE(dev->state_on);
    CHECK_INT_EQ(dev->brightness, 17);
    CHECK_INT_EQ(priv->last_brightness, 17);

    led_set_color(dev, &color);
    CHECK_INT_EQ(dev->brightness, 9);
    CHECK_INT_EQ(dev->color.r, 3);
    CHECK_INT_EQ(priv->last_color.g, 9);

    led_blink(dev, &blink);
    CHECK_INT_EQ(dev->effect, LED_EFFECT_BLINK);
    CHECK_TRUE(dev->blink_on);
    led_tick(dev, 50);
    CHECK_TRUE(!dev->blink_on);
    led_tick(dev, 50);
    CHECK_TRUE(dev->blink_on);
    led_tick(dev, 100);
    CHECK_INT_EQ(dev->effect, LED_EFFECT_NONE);
    CHECK_TRUE(!dev->state_on);

    led_breath(dev, 100);
    CHECK_INT_EQ(dev->effect, LED_EFFECT_BREATH);
    led_tick(dev, 25);
    CHECK_INT_EQ(dev->brightness, 127);
    CHECK_INT_EQ(priv->last_brightness, 127);

    led_free(dev);
    CHECK_INT_EQ(g_free_count, 1);
}

static int finish_test(const char *name)
{
    if (g_failures != 0) {
        printf("%s FAILED: %d failure(s)\n", name, g_failures);
        return 1;
    }
    printf("%s PASSED\n", name);
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "all";

    if (strcmp(mode, "functional") == 0) {
        reset_test_state();
        test_functional();
        return finish_test("led api functional test");
    }
    if (strcmp(mode, "error-paths") == 0) {
        reset_test_state();
        test_error_paths();
        return finish_test("led api error paths test");
    }
    if (strcmp(mode, "all") == 0) {
        reset_test_state();
        test_functional();
        if (finish_test("led api functional test") != 0)
            return 1;
        reset_test_state();
        test_error_paths();
        if (finish_test("led api error paths test") != 0)
            return 1;
        printf("led api contract test PASSED\n");
        return 0;
    }

    fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}
