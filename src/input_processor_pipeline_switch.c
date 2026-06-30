/*
 * Copyright (c) 2026 Vincent Franco
 * SPDX-License-Identifier: MIT
 *
 * Routes input events through one of several child pipelines of input
 * processors. Pipeline iteration, parameter extraction and remainder
 * tracking mirror ZMK's input listener (app/src/pointing/input_listener.c)
 * so that sub-processors behave exactly as they would directly under a
 * zmk,input-listener.
 */
#define DT_DRV_COMPAT zmk_input_processor_pipeline_switch

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include <drivers/input_processor.h>

#include "zip_pipeline_switch.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define SETTINGS_PREFIX "zip_ps"

/* Same shape as the input listener's per-processor remainder storage. */
struct zip_ps_remainders {
    int16_t x, y, wheel, h_wheel;
};

struct zip_ps_pipeline {
    const char *label;
    size_t processors_len;
    const struct zmk_input_processor_entry *processors;
    size_t remainders_len;
    struct zip_ps_remainders *remainders;
};

struct zip_ps_state {
    uint8_t active;
};

struct zip_ps_config {
    const char *settings_key;
    int32_t save_delay;
    bool persistent;
    uint8_t default_index;
    uint8_t pipelines_len;
    const struct zip_ps_pipeline *pipelines;
};

struct zip_ps_data {
    const struct device *dev;
#if IS_ENABLED(CONFIG_SETTINGS)
    struct k_work_delayable save_work;
#endif
    struct zip_ps_state state;
};

static int zip_ps_handle_event(const struct device *dev, struct input_event *event,
                               uint32_t param1, uint32_t param2,
                               struct zmk_input_processor_state *state) {
    struct zip_ps_data *data = dev->data;
    const struct zip_ps_config *config = dev->config;
    const struct zip_ps_pipeline *pipeline = &config->pipelines[data->state.active];

    size_t remainder_index = 0;
    for (size_t p = 0; p < pipeline->processors_len; p++) {
        const struct zmk_input_processor_entry *proc_e = &pipeline->processors[p];
        struct zip_ps_remainders *remainders = NULL;
        if (proc_e->track_remainders) {
            remainders = &pipeline->remainders[remainder_index++];
        }

        int16_t *remainder = NULL;
        if (remainders && event->type == INPUT_EV_REL) {
            switch (event->code) {
            case INPUT_REL_X:
                remainder = &remainders->x;
                break;
            case INPUT_REL_Y:
                remainder = &remainders->y;
                break;
            case INPUT_REL_WHEEL:
                remainder = &remainders->wheel;
                break;
            case INPUT_REL_HWHEEL:
                remainder = &remainders->h_wheel;
                break;
            }
        }

        struct zmk_input_processor_state sub_state = {
            .input_device_index = state ? state->input_device_index : 0,
            .remainder = remainder,
        };

        int ret = zmk_input_processor_handle_event(proc_e->dev, event, proc_e->param1,
                                                   proc_e->param2, &sub_state);
        switch (ret) {
        case ZMK_INPUT_PROC_CONTINUE:
            continue;
        default:
            return ret;
        }
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

#if IS_ENABLED(CONFIG_SETTINGS)

static void save_work_callback(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct zip_ps_data *data = CONTAINER_OF(dwork, struct zip_ps_data, save_work);
    const struct zip_ps_config *config = data->dev->config;

    int err = settings_save_one(config->settings_key, &data->state, sizeof(struct zip_ps_state));
    if (err < 0) {
        LOG_ERR("Failed to save settings %d", err);
    }
}

#endif

/* Shared by cycle and set: activate `index`, reset the incoming pipeline's
 * remainders, and (when `persist`) schedule a debounced save. */
static int zip_ps_apply(const struct device *dev, uint8_t index, bool persist) {
    struct zip_ps_data *data = dev->data;
    const struct zip_ps_config *config = dev->config;

    if (index >= config->pipelines_len) {
        LOG_WRN("%s: pipeline index %d out of range (have %d)", dev->name, index,
                config->pipelines_len);
        return -EINVAL;
    }

    data->state.active = index;

    // Stale fractional remainders from a previous activation are meaningless
    // for the new gesture, so start the incoming pipeline clean.
    const struct zip_ps_pipeline *pipeline = &config->pipelines[data->state.active];
    memset(pipeline->remainders, 0, pipeline->remainders_len * sizeof(struct zip_ps_remainders));

    LOG_INF("%s: active pipeline now %d", dev->name, data->state.active);

#if IS_ENABLED(CONFIG_SETTINGS)
    if (persist && config->persistent) {
        k_work_reschedule(&data->save_work, K_MSEC(config->save_delay));
    }
#endif
    return data->state.active;
}

int zip_pipeline_switch_cycle(const struct device *dev, int32_t delta) {
    struct zip_ps_data *data = dev->data;
    const struct zip_ps_config *config = dev->config;

    int next = ((int)data->state.active + delta) % (int)config->pipelines_len;
    if (next < 0) {
        next += config->pipelines_len;
    }
    return zip_ps_apply(dev, (uint8_t)next, true);
}

int zip_pipeline_switch_set(const struct device *dev, uint8_t index, bool persist) {
    return zip_ps_apply(dev, index, persist);
}

uint8_t zip_pipeline_switch_count(const struct device *dev) {
    const struct zip_ps_config *config = dev->config;
    return config->pipelines_len;
}

uint8_t zip_pipeline_switch_active(const struct device *dev) {
    struct zip_ps_data *data = dev->data;
    return data->state.active;
}

const char *zip_pipeline_switch_label(const struct device *dev, uint8_t index) {
    const struct zip_ps_config *config = dev->config;
    if (index >= config->pipelines_len) {
        return NULL;
    }
    return config->pipelines[index].label;
}

static int zip_ps_init(const struct device *dev) {
    struct zip_ps_data *data = dev->data;
    const struct zip_ps_config *config = dev->config;
    data->dev = dev;

    // Boot default; a persisted selection (loaded later) overrides this.
    data->state.active = config->default_index < config->pipelines_len ? config->default_index : 0;

#if IS_ENABLED(CONFIG_SETTINGS)
    if (config->persistent) {
        k_work_init_delayable(&data->save_work, save_work_callback);
    }
#endif
    return 0;
}

static const struct zmk_input_processor_driver_api zip_ps_driver_api = {
    .handle_event = zip_ps_handle_event,
};

/* Mirrors the input listener's remainder-slot counting. */
#define ZIP_PS_ONE_FOR_TRACKED(n, elem, idx)                                                       \
    +DT_PROP(DT_PHANDLE_BY_IDX(n, input_processors, idx), track_remainders)
#define ZIP_PS_REM_TRACKERS(n) (0 DT_FOREACH_PROP_ELEM(n, input_processors, ZIP_PS_ONE_FOR_TRACKED))

#define ZIP_PS_CHILD_DEFINE(child)                                                                 \
    static struct zip_ps_remainders _CONCAT(zip_ps_rem_,                                           \
                                            DT_DEP_ORD(child))[ZIP_PS_REM_TRACKERS(child)] =  \
        {};                                                                                        \
    static const struct zmk_input_processor_entry _CONCAT(                                         \
        zip_ps_entries_, DT_DEP_ORD(child))[DT_PROP_LEN(child, input_processors)] = {         \
        LISTIFY(DT_PROP_LEN(child, input_processors), ZMK_INPUT_PROCESSOR_ENTRY_AT_IDX, (, ),      \
                child)};

#define ZIP_PS_PIPELINE(child)                                                                     \
    {                                                                                              \
        .label = DT_PROP_OR(child, label, DT_NODE_FULL_NAME(child)),                               \
        .processors_len = DT_PROP_LEN(child, input_processors),                                    \
        .processors = _CONCAT(zip_ps_entries_, DT_DEP_ORD(child)),                            \
        .remainders_len = ZIP_PS_REM_TRACKERS(child),                                              \
        .remainders = _CONCAT(zip_ps_rem_, DT_DEP_ORD(child)),                                \
    }

/* Inits at POST_KERNEL/95: must be after the wrapped sub-processors, which
 * register at CONFIG_KERNEL_INIT_PRIORITY_DEFAULT. */
#define ZIP_PS_INST(n)                                                                             \
    BUILD_ASSERT(DT_INST_CHILD_NUM(n) > 0,                                                         \
                 "zmk,input-processor-pipeline-switch requires at least one child pipeline");      \
    DT_INST_FOREACH_CHILD(n, ZIP_PS_CHILD_DEFINE)                                                  \
    static const struct zip_ps_pipeline zip_ps_pipelines_##n[] = {                                 \
        DT_INST_FOREACH_CHILD_SEP(n, ZIP_PS_PIPELINE, (, ))};                                      \
    static struct zip_ps_data zip_ps_data_##n = {};                                                \
    static const struct zip_ps_config zip_ps_config_##n = {                                        \
        .settings_key = SETTINGS_PREFIX "/" #n,                                                    \
        .save_delay = DT_INST_PROP(n, save_delay),                                                 \
        .persistent = DT_INST_PROP(n, persistent),                                                 \
        .default_index = DT_INST_PROP_OR(n, default_index, 0),                                     \
        .pipelines_len = ARRAY_SIZE(zip_ps_pipelines_##n),                                         \
        .pipelines = zip_ps_pipelines_##n,                                                         \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, &zip_ps_init, NULL, &zip_ps_data_##n, &zip_ps_config_##n,             \
                          POST_KERNEL, 95, &zip_ps_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ZIP_PS_INST)

#if IS_ENABLED(CONFIG_SETTINGS)

#define ZIP_PS_SETTINGS_INST(n)                                                                    \
    case n: {                                                                                      \
        data = &zip_ps_data_##n;                                                                   \
        config = &zip_ps_config_##n;                                                               \
        break;                                                                                     \
    }

static int zip_ps_settings_load_cb(const char *name, size_t len, settings_read_cb read_cb,
                                   void *cb_arg) {
    struct zip_ps_data *data = NULL;
    const struct zip_ps_config *config = NULL;
    char *endptr;
    long identifier = strtol(name, &endptr, 10);
    int err = 0;

    if (endptr == name) {
        return -ENOENT;
    }

    // The identifier is the devicetree instance index.
    switch (identifier) {
        DT_INST_FOREACH_STATUS_OKAY(ZIP_PS_SETTINGS_INST)
    default:
        return -ENOENT;
    }

    if (config->persistent) {
        err = read_cb(cb_arg, &data->state, sizeof(struct zip_ps_state));
        if (err >= 0 && data->state.active >= config->pipelines_len) {
            data->state.active = 0;
        }
        if (err < 0) {
            LOG_ERR("Failed to load settings %d", err);
        }
    }
    return MIN(err, 0);
}

SETTINGS_STATIC_HANDLER_DEFINE(zip_pipeline_switch, SETTINGS_PREFIX, NULL, zip_ps_settings_load_cb,
                               NULL, NULL);
#endif

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
