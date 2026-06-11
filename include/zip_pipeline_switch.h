/*
 * Copyright (c) 2026 Vincent Franco
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <zephyr/device.h>

/* Cycle the active pipeline by delta (wraps), persisting if configured.
 * Returns the new active index, or a negative error code. */
int zip_pipeline_switch_cycle(const struct device *dev, int32_t delta);
