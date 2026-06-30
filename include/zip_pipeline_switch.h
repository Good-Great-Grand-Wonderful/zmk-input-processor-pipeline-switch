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
