/*
 * Copyright (c) 2026 Vincent Franco
 * SPDX-License-Identifier: MIT
 *
 * Keypress behavior that cycles the active pipeline of a
 * zmk,input-processor-pipeline-switch processor.
 *
 * Locality is BEHAVIOR_LOCALITY_EVENT_SOURCE: the behavior runs on whichever
 * half the key was pressed on, and switches that half's processor instance.
 * Place the binding on the half whose listener runs the pipelines. Node
 * names must fit the split transport's 16-byte behavior_dev field.
 */
#define DT_DRV_COMPAT zmk_behavior_pipeline_switch

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include "zip_pipeline_switch.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_pipeline_switch_config {
    const struct device *processor;
};

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param_values[] = {
    {
        .display_name = "Next",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = 1,
    },
    {
        .display_name = "Previous",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = -1,
    },
};

static const struct behavior_parameter_metadata_set param_metadata_set[] = {{
    .param1_values = param_values,
    .param1_values_len = ARRAY_SIZE(param_values),
}};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = ARRAY_SIZE(param_metadata_set),
    .sets = param_metadata_set,
};

#endif

static int behavior_pipeline_switch_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_pipeline_switch_config *config = dev->config;

    int ret = zip_pipeline_switch_cycle(config->processor, binding->param1);
    if (ret < 0) {
        LOG_ERR("Failed to cycle pipeline on %s (err %d)", config->processor->name, ret);
        return ret;
    }
    return 0;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return 0;
}

static const struct behavior_driver_api behavior_pipeline_switch_driver_api = {
    .locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif
};

/* Inits at POST_KERNEL/96: after the pipeline-switch processor (95) this
 * behavior references via DEVICE_DT_GET. */
#define PIPELINE_SWITCH_INST(n)                                                                    \
    static const struct behavior_pipeline_switch_config behavior_pipeline_switch_config_##n = {    \
        .processor = DEVICE_DT_GET(DT_INST_PHANDLE(n, processor)),                                 \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, &behavior_pipeline_switch_init, NULL, NULL,                         \
                            &behavior_pipeline_switch_config_##n,                                  \
                            POST_KERNEL, 96, &behavior_pipeline_switch_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PIPELINE_SWITCH_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
