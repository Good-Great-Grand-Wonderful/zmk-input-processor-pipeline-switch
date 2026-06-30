/*
 * Copyright (c) 2026 Vincent Franco
 * SPDX-License-Identifier: MIT
 *
 * Behavior that sets the active pipeline of a
 * zmk,input-processor-pipeline-switch processor to an absolute index.
 *   param1: target pipeline index (0..N-1, declaration order).
 *   param2: persist flag - 0 = apply in RAM only (live preview), non-zero =
 *           apply and save to flash (when the processor is `persistent`).
 *
 * Locality is BEHAVIOR_LOCALITY_EVENT_SOURCE: the behavior runs on whichever
 * half the binding's event source points at, and switches that half's
 * processor instance. From a keymap that is the half the key is pressed on;
 * the central can also drive a specific peripheral's instance by invoking the
 * binding with zmk_behavior_invoke_binding() and event.source set to the
 * peripheral index (255 / UINT8_MAX = local/central). Node names must be 8
 * chars or fewer: the BLE split relay truncates behavior names to
 * ZMK_SPLIT_RUN_BEHAVIOR_DEV_LEN (9 bytes incl. NUL) and the peripheral
 * resolves behaviors by that name.
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

static int behavior_pipeline_switch_init(const struct device *dev) { return 0; }

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_pipeline_switch_config *config = dev->config;

    bool persist = (binding->param2 != 0);
    int ret = zip_pipeline_switch_set(config->processor, (uint8_t)binding->param1, persist);
    if (ret < 0) {
        LOG_ERR("Failed to set pipeline on %s to %u (err %d)", config->processor->name,
                (unsigned int)binding->param1, ret);
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
