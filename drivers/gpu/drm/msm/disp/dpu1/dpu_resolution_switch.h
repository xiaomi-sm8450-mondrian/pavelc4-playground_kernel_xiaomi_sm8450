/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024, Xiaomi Inc.
 * Resolution switching support for DPU display driver
 */

#ifndef _DPU_RESOLUTION_SWITCH_H_
#define _DPU_RESOLUTION_SWITCH_H_

#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_connector.h>

/* Resolution modes */
#define DPU_RESOLUTION_WQHD_PLUS	0	/* 1440x3200 */
#define DPU_RESOLUTION_FHD_PLUS		1	/* 1080x2400 */

/* Timing indices in device tree */
#define DPU_TIMING_WQHD_60HZ		0
#define DPU_TIMING_WQHD_90HZ		1
#define DPU_TIMING_WQHD_120HZ		2
#define DPU_TIMING_FHD_120HZ		3

struct dpu_resolution_config {
	int width;
	int height;
	int timing_index;
	const char *name;
};

/* Resolution configurations */
static const struct dpu_resolution_config dpu_resolutions[] = {
	[DPU_RESOLUTION_WQHD_PLUS] = {
		.width = 1440,
		.height = 3200,
		.timing_index = DPU_TIMING_WQHD_120HZ,
		.name = "WQHD+ (1440x3200@120Hz)",
	},
	[DPU_RESOLUTION_FHD_PLUS] = {
		.width = 1080,
		.height = 2400,
		.timing_index = DPU_TIMING_FHD_120HZ,
		.name = "FHD+ (1080x2400@120Hz)",
	},
};

/**
 * dpu_resolution_switch_mode() - Switch display resolution
 * @ddev: DRM device
 * @new_mode: New resolution mode (0=WQHD+, 1=FHD+)
 *
 * Returns: 0 on success, negative error code on failure
 */
int dpu_resolution_switch_mode(struct drm_device *ddev, int new_mode);

/**
 * dpu_resolution_get_current_mode() - Get current resolution mode
 *
 * Returns: Current resolution mode (0=WQHD+, 1=FHD+)
 */
int dpu_resolution_get_current_mode(void);

/**
 * dpu_resolution_init() - Initialize resolution switching
 * @dev: Device for sysfs registration
 *
 * Returns: 0 on success, negative error code on failure
 */
int dpu_resolution_init(struct device *dev);

/**
 * dpu_resolution_cleanup() - Cleanup resolution switching
 * @dev: Device for sysfs removal
 */
void dpu_resolution_cleanup(struct device *dev);

/**
 * dpu_resolution_set_kms() - Set DPU KMS pointer
 * @dpu_kms: DPU KMS structure
 */
void dpu_resolution_set_kms(struct dpu_kms *dpu_kms);

/**
 * dpu_resolution_clear_kms() - Clear DPU KMS pointer
 */
void dpu_resolution_clear_kms(void);

#endif /* _DPU_RESOLUTION_SWITCH_H_ */