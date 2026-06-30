/*
 * Copyright (c) 2026 Vincent Franco
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <zephyr/device.h>

/* Cycle the active pipeline by delta (wraps), persisting if configured.
 * Returns the new active index, or a negative error code. */
int zip_pipeline_switch_cycle(const struct device *dev, int32_t delta);

/* Set the active pipeline to an absolute index (0..N-1, declaration order).
 * When `persist` is true and the node is configured `persistent`, the
 * selection is saved to flash (debounced by save_delay); when false the change
 * is applied in RAM only (live preview, no flash write). Returns the new
 * active index, or -EINVAL if the index is out of range. */
int zip_pipeline_switch_set(const struct device *dev, uint8_t index, bool persist);

/* Introspection, for host-facing config UIs that render from device truth. */

/* Number of selectable pipelines (child nodes) on this processor. */
uint8_t zip_pipeline_switch_count(const struct device *dev);

/* Currently active pipeline index. */
uint8_t zip_pipeline_switch_active(const struct device *dev);

/* Human-readable label for pipeline `index`, or NULL if out of range.
 * Defaults to the child node name when no `label` property is set. */
const char *zip_pipeline_switch_label(const struct device *dev, uint8_t index);
