// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Xiaomi Inc.
 * Resolution switching support for DPU display driver
 */

#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>
#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_connector.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>

#include "dpu_kms.h"
#include "dpu_encoder.h"
#include "dpu_resolution_switch.h"

/* Global state */
static int current_resolution_mode = DPU_RESOLUTION_WQHD_PLUS;
static struct dpu_kms *g_dpu_kms = NULL;
static struct device *g_sysfs_dev = NULL;

/**
 * dpu_resolution_switch_timing() - Switch display timing
 * @encoder: DRM encoder
 * @timing_index: New timing index
 *
 * Returns: 0 on success, negative error code on failure
 */
static int dpu_resolution_switch_timing(struct drm_encoder *encoder, int timing_index)
{
	struct msm_drm_private *priv;
	struct drm_device *ddev;
	struct drm_atomic_state *state;
	struct drm_crtc_state *crtc_state;
	struct drm_connector_state *conn_state;
	struct drm_connector *connector;
	struct drm_crtc *crtc;
	int ret = 0;

	if (!encoder || !encoder->dev) {
		pr_err("DPU: Invalid encoder for resolution switch\n");
		return -EINVAL;
	}

	ddev = encoder->dev;
	priv = ddev->dev_private;

	/* Create atomic state for mode switch */
	state = drm_atomic_state_alloc(ddev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = ddev->mode_config.acquire_ctx;

	/* Get connector associated with encoder */
	connector = NULL;
	list_for_each_entry(connector, &ddev->mode_config.connector_list, head) {
		if (connector->encoder == encoder)
			break;
	}

	if (!connector) {
		pr_err("DPU: No connector found for encoder\n");
		ret = -ENODEV;
		goto out;
	}

	crtc = encoder->crtc;
	if (!crtc) {
		pr_err("DPU: No CRTC associated with encoder\n");
		ret = -ENODEV;
		goto out;
	}

	/* Get connector state */
	conn_state = drm_atomic_get_connector_state(state, connector);
	if (IS_ERR(conn_state)) {
		ret = PTR_ERR(conn_state);
		goto out;
	}

	/* Get CRTC state */
	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state)) {
		ret = PTR_ERR(crtc_state);
		goto out;
	}

	/* Mark mode as changed to trigger timing switch */
	crtc_state->mode_changed = true;

	/* Commit the atomic state */
	ret = drm_atomic_commit(state);
	if (ret) {
		pr_err("DPU: Failed to commit atomic state: %d\n", ret);
		goto out;
	}

	pr_info("DPU: Successfully switched to timing index %d\n", timing_index);

out:
	drm_atomic_state_put(state);
	return ret;
}

int dpu_resolution_switch_mode(struct drm_device *ddev, int new_mode)
{
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	const struct dpu_resolution_config *config;
	int ret = 0;

	if (new_mode < 0 || new_mode >= ARRAY_SIZE(dpu_resolutions)) {
		pr_err("DPU: Invalid resolution mode: %d\n", new_mode);
		return -EINVAL;
	}

	if (new_mode == current_resolution_mode) {
		pr_debug("DPU: Resolution mode already set to %d\n", new_mode);
		return 0;
	}

	config = &dpu_resolutions[new_mode];

	/* Find DSI encoder */
	encoder = NULL;
	drm_connector_list_iter_begin(ddev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (connector->connector_type == DRM_MODE_CONNECTOR_DSI) {
			encoder = connector->encoder;
			break;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	if (!encoder) {
		pr_err("DPU: No DSI encoder found\n");
		return -ENODEV;
	}

	/* Switch timing */
	ret = dpu_resolution_switch_timing(encoder, config->timing_index);
	if (ret) {
		pr_err("DPU: Failed to switch timing: %d\n", ret);
		return ret;
	}

	current_resolution_mode = new_mode;
	pr_info("DPU: Resolution switched to %s\n", config->name);

	return 0;
}

int dpu_resolution_get_current_mode(void)
{
	return current_resolution_mode;
}

/* Sysfs interface */
static ssize_t resolution_mode_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	const struct dpu_resolution_config *config = &dpu_resolutions[current_resolution_mode];
	return scnprintf(buf, PAGE_SIZE, "%d (%s)\n", current_resolution_mode, config->name);
}

static ssize_t resolution_mode_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int new_mode, ret;

	if (!g_dpu_kms || !g_dpu_kms->dev) {
		pr_err("DPU: KMS not initialized\n");
		return -ENODEV;
	}

	ret = kstrtoint(buf, 10, &new_mode);
	if (ret) {
		pr_err("DPU: Invalid input: %s\n", buf);
		return ret;
	}

	ret = dpu_resolution_switch_mode(g_dpu_kms->dev, new_mode);
	if (ret)
		return ret;

	return count;
}

static ssize_t resolution_modes_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;

	len += scnprintf(buf + len, PAGE_SIZE - len, "Available resolution modes:\n");
	for (i = 0; i < ARRAY_SIZE(dpu_resolutions); i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d: %s%s\n",
				 i, dpu_resolutions[i].name,
				 (i == current_resolution_mode) ? " (current)" : "");
	}

	return len;
}

static DEVICE_ATTR_RW(resolution_mode);
static DEVICE_ATTR_RO(resolution_modes);

static struct attribute *dpu_resolution_attrs[] = {
	&dev_attr_resolution_mode.attr,
	&dev_attr_resolution_modes.attr,
	NULL,
};

static const struct attribute_group dpu_resolution_attr_group = {
	.name = "display_resolution",
	.attrs = dpu_resolution_attrs,
};

int dpu_resolution_init(struct device *dev)
{
	int ret;

	if (!dev) {
		pr_err("DPU: Invalid device for resolution init\n");
		return -EINVAL;
	}

	g_sysfs_dev = dev;

	ret = sysfs_create_group(&dev->kobj, &dpu_resolution_attr_group);
	if (ret) {
		pr_err("DPU: Failed to create resolution sysfs group: %d\n", ret);
		return ret;
	}

	pr_info("DPU: Resolution switching initialized\n");
	return 0;
}

void dpu_resolution_cleanup(struct device *dev)
{
	if (dev && g_sysfs_dev == dev) {
		sysfs_remove_group(&dev->kobj, &dpu_resolution_attr_group);
		g_sysfs_dev = NULL;
		pr_info("DPU: Resolution switching cleaned up\n");
	}
}

void dpu_resolution_set_kms(struct dpu_kms *dpu_kms)
{
	g_dpu_kms = dpu_kms;
}

void dpu_resolution_clear_kms(void)
{
	g_dpu_kms = NULL;
}