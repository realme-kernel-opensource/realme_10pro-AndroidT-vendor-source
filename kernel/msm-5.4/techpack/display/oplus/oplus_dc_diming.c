/***************************************************************
** Copyright (C),  2020,  oplus Mobile Comm Corp.,  Ltd
** VENDOR_EDIT
** File : oplus_dc_diming.c
** Description : oplus dc_diming feature
** Version : 1.0
** Date : 2020/04/15
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**   Qianxu         2020/04/15        1.0           Build this moudle
******************************************************************/

#include "oplus_display_private_api.h"
#include "oplus_dc_diming.h"
#include "oplus_onscreenfingerprint.h"
#include "oplus_display_panel_common.h"
#include "oplus_aod.h"
#include "sde_trace.h"
#include <linux/msm_drm_notify.h>
#include <linux/notifier.h>

int oplus_dimlayer_bl = 0;
EXPORT_SYMBOL(oplus_dimlayer_bl);
int oplus_dimlayer_bl_enabled = 0;
EXPORT_SYMBOL(oplus_dimlayer_bl_enabled);
int oplus_datadimming_v3_skip_frame = 2;
int oplus_panel_alpha = 0;
int oplus_underbrightness_alpha = 0;
EXPORT_SYMBOL(oplus_underbrightness_alpha);
static struct dsi_panel_cmd_set oplus_priv_seed_cmd_set;

extern int oplus_dimlayer_bl_on_vblank;
extern int oplus_dimlayer_bl_off_vblank;
extern int oplus_dimlayer_bl_delay;
extern int oplus_dimlayer_bl_delay_after;
extern int oplus_dimlayer_bl_enable_v2;
extern int oplus_dimlayer_bl_enable_v2_real;
extern int oplus_dimlayer_bl_enable_v3;
extern int oplus_dimlayer_bl_enable_v3_real;
extern int oplus_datadimming_vblank_count;
extern atomic_t oplus_datadimming_vblank_ref;
extern int oplus_fod_on_vblank;
extern int oplus_fod_off_vblank;
extern bool oplus_skip_datadimming_sync;
extern int oplus_dimlayer_hbm_vblank_count;
extern atomic_t oplus_dimlayer_hbm_vblank_ref;
extern int oplus_dc2_alpha;
extern int oplus_seed_backlight;
extern int oplus_panel_alpha;
extern int oplus_dimlayer_bl_enable;

extern ktime_t oplus_backlight_time;
extern int oplus_display_mode;

static struct oplus_brightness_alpha brightness_seed_alpha_lut_dc[] = {
	{0, 0xff},
	{1, 0xfc},
	{2, 0xfb},
	{3, 0xfa},
	{4, 0xf9},
	{5, 0xf8},
	{6, 0xf7},
	{8, 0xf6},
	{10, 0xf4},
	{15, 0xf0},
	{20, 0xea},
	{30, 0xe0},
	{45, 0xd0},
	{70, 0xbc},
	{100, 0x98},
	{120, 0x80},
	{140, 0x70},
	{160, 0x58},
	{180, 0x48},
	{200, 0x30},
	{220, 0x20},
	{240, 0x10},
	{260, 0x00},
};

struct hbm_status
{
	bool aod_hbm_status; /*use for hbm aod cmd*/
	bool hbm_status;     /*use for hbm cmd*/
	bool hbm_pvt_status; /*use for hbm other cmds*/
};

struct hbm_status oplus_hbm_status = {0};
int dsi_panel_tx_cmd_hbm_pre_check(struct dsi_panel *panel, enum dsi_cmd_set_type type, const char** prop_map)
{
	int ret = 0;

	DSI_DEBUG("%s cmd=%s", __func__, prop_map[type]);
	if (NULL == panel) {
		return ret;
	}

	switch(type) {
	case DSI_CMD_AOD_HBM_ON:
		if (oplus_hbm_status.aod_hbm_status == 1) {
			DSI_DEBUG("%s skip cmd=%s", __func__, prop_map[type]);
			ret = 1;
		}
		break;
	case DSI_CMD_AOD_HBM_OFF:
		if (oplus_hbm_status.aod_hbm_status == 0) {
			DSI_DEBUG("%s skip cmd=%s", __func__, prop_map[type]);
			ret = 1;
		}
		break;
	default:
		break;
	}
	return ret;
}

void dsi_panel_tx_cmd_hbm_post_check(struct dsi_panel *panel, enum dsi_cmd_set_type type)
{
	if (NULL == panel) {
		return;
	}

	switch(type) {
	case DSI_CMD_AOD_HBM_ON:
		if (oplus_hbm_status.aod_hbm_status == 0) {
			oplus_hbm_status.aod_hbm_status = 1;
		}
		break;
	case DSI_CMD_AOD_HBM_OFF:
		if (oplus_hbm_status.aod_hbm_status == 1) {
			oplus_hbm_status.aod_hbm_status = 0;
		}
		break;
	case DSI_CMD_SET_NOLP:
	case DSI_CMD_SET_OFF:
		oplus_hbm_status.aod_hbm_status = 0;
		break;
	default:
		break;
	}
	DSI_DEBUG("%s [hbm_pvt,hbm,hbm_aod] = [%d %d %d]", __func__,
		oplus_hbm_status.hbm_pvt_status, oplus_hbm_status.hbm_status, oplus_hbm_status.aod_hbm_status);

	return;
}

bool sde_encoder_is_disabled(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;

	sde_enc = to_sde_encoder_virt(drm_enc);
	phys = sde_enc->phys_encs[0];
	return (phys->enable_state == SDE_ENC_DISABLED);
}

int fingerprint_wait_vsync(struct drm_encoder *drm_enc, struct dsi_panel *panel)
{
	SDE_ATRACE_BEGIN("wait_vsync");

	if (!drm_enc || !drm_enc->crtc || !panel) {
		SDE_ERROR("kOFP, %s encoder is disabled", __func__);
		return -ENOLINK;
	}

	if (sde_encoder_is_disabled(drm_enc)) {
		SDE_ERROR("kOFP, %s encoder is disabled", __func__);
		return -EIO;
	}

	mutex_unlock(&panel->panel_lock);
	sde_encoder_wait_for_event(drm_enc,  MSM_ENC_VBLANK);
	mutex_lock(&panel->panel_lock);
	SDE_ATRACE_END("wait_vsync");

	return 0;
}

int sde_connector_update_backlight(struct drm_connector *connector, bool post)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dsi_display *dsi_display;

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		return 0;
	}

	dsi_display = c_conn->display;

	if (!dsi_display || !dsi_display->panel || !dsi_display->panel->cur_mode) {
		SDE_ERROR("Invalid params(s) dsi_display %pK, panel %pK\n",
			  dsi_display,
			  ((dsi_display) ? dsi_display->panel : NULL));
		return -EINVAL;
	}

	if (!connector->state || !connector->state->crtc) {
		return 0;
	}

	if (oplus_dimlayer_bl != oplus_dimlayer_bl_enabled) {
		struct sde_connector *c_conn = to_sde_connector(connector);
		struct drm_crtc *crtc = connector->state->crtc;
		struct dsi_panel *panel = dsi_display->panel;
		int bl_lvl = dsi_display->panel->bl_config.bl_level;
		u32 current_vblank;
		int on_vblank = 0;
		int off_vblank = 0;
		int vblank = 0;
		int ret = 0;
		int vblank_get = -EINVAL;
		int on_delay = 0, on_delay_after = 0;
		int off_delay = 0, off_delay_after = 0;
		int delay = 0, delay_after = 0;

		if (sde_crtc_get_fingerprint_mode(crtc->state)) {
			oplus_dimlayer_bl_enabled = oplus_dimlayer_bl;
			goto done;
		}

		if (panel->cur_mode->timing.refresh_rate == 120) {
			if (bl_lvl < 103) {
				on_vblank = 0;
				off_vblank = 2;

			} else {
				on_vblank = 0;
				off_vblank = 0;
			}

		} else {
			if (bl_lvl < 103) {
				on_vblank = -1;
				off_vblank = 1;
				on_delay = 11000;

			} else {
				on_vblank = -1;
				off_vblank = -1;
				on_delay = 11000;
				off_delay = 11000;
			}
		}

		if (oplus_dimlayer_bl_on_vblank != INT_MAX) {
			on_vblank = oplus_dimlayer_bl_on_vblank;
		}

		if (oplus_dimlayer_bl_off_vblank != INT_MAX) {
			off_vblank = oplus_dimlayer_bl_off_vblank;
		}


		if (oplus_dimlayer_bl) {
			vblank = on_vblank;
			delay = on_delay;
			delay_after = on_delay_after;

		} else {
			vblank = off_vblank;
			delay = off_delay;
			delay_after = off_delay_after;
		}

		if (oplus_dimlayer_bl_delay >= 0) {
			delay = oplus_dimlayer_bl_delay;
		}

		if (oplus_dimlayer_bl_delay_after >= 0) {
			delay_after = oplus_dimlayer_bl_delay_after;
		}

		vblank_get = drm_crtc_vblank_get(crtc);

		if (vblank >= 0) {
			if (!post) {
				oplus_dimlayer_bl_enabled = oplus_dimlayer_bl;
				current_vblank = drm_crtc_vblank_count(crtc);
				ret = wait_event_timeout(*drm_crtc_vblank_waitqueue(crtc),
							 current_vblank != drm_crtc_vblank_count(crtc),
							 msecs_to_jiffies(34));
				current_vblank = drm_crtc_vblank_count(crtc) + vblank;

				if (delay > 0) {
					usleep_range(delay, delay + 100);
				}

				_sde_connector_update_bl_scale_(c_conn);

				if (delay_after) {
					usleep_range(delay_after, delay_after + 100);
				}

				if (vblank > 0) {
					ret = wait_event_timeout(*drm_crtc_vblank_waitqueue(crtc),
								 current_vblank == drm_crtc_vblank_count(crtc),
								 msecs_to_jiffies(17 * 3));
				}
			}

		} else {
			if (!post) {
				current_vblank = drm_crtc_vblank_count(crtc);
				ret = wait_event_timeout(*drm_crtc_vblank_waitqueue(crtc),
							 current_vblank != drm_crtc_vblank_count(crtc),
							 msecs_to_jiffies(34));

			} else {
				if (vblank < -1) {
					current_vblank = drm_crtc_vblank_count(crtc) + 1 - vblank;
					ret = wait_event_timeout(*drm_crtc_vblank_waitqueue(crtc),
								 current_vblank == drm_crtc_vblank_count(crtc),
								 msecs_to_jiffies(17 * 3));
				}

				oplus_dimlayer_bl_enabled = oplus_dimlayer_bl;

				if (delay > 0) {
					usleep_range(delay, delay + 100);
				}

				_sde_connector_update_bl_scale_(c_conn);

				if (delay_after) {
					usleep_range(delay_after, delay_after + 100);
				}
			}
		}

		if (!vblank_get) {
			drm_crtc_vblank_put(crtc);
		}
	}

	if (oplus_dimlayer_bl_enable_v2 != oplus_dimlayer_bl_enable_v2_real) {
		struct sde_connector *c_conn = to_sde_connector(connector);

		oplus_dimlayer_bl_enable_v2_real = oplus_dimlayer_bl_enable_v2;
		_sde_connector_update_bl_scale_(c_conn);
	}

	if (oplus_dimlayer_bl_enable_v3 != oplus_dimlayer_bl_enable_v3_real) {
		struct sde_connector *c_conn = to_sde_connector(connector);

		if (oplus_datadimming_v3_skip_frame > 0) {
			oplus_datadimming_v3_skip_frame--;

		} else {
			oplus_dimlayer_bl_enable_v3_real = oplus_dimlayer_bl_enable_v3;
			_sde_connector_update_bl_scale_(c_conn);
			oplus_datadimming_v3_skip_frame = 2;
		}
	}

done:

	if (post) {
		if (oplus_datadimming_vblank_count > 0) {
			oplus_datadimming_vblank_count--;

		} else {
			while (atomic_read(&oplus_datadimming_vblank_ref) > 0) {
				drm_crtc_vblank_put(connector->state->crtc);
				atomic_dec(&oplus_datadimming_vblank_ref);
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(sde_connector_update_backlight);

extern int oplus_aod_mode;
extern bool oplus_first_vid;
static bool oplus_set_hbm_off_flag;
int sde_connector_update_hbm(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dsi_display *dsi_display;
	struct sde_connector_state *c_state;
	int rc = 0;
	int fingerprint_mode;
	unsigned int refresh_rate = 0;
	unsigned int us_per_frame = 0;
	unsigned int delay_us = 0;

	if (!c_conn) {
		SDE_ERROR("Invalid params sde_connector null\n");
		return -EINVAL;
	}

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		return 0;
	}

	c_state = to_sde_connector_state(connector->state);

	dsi_display = c_conn->display;

	if (!dsi_display || !dsi_display->panel) {
		SDE_ERROR("Invalid params(s) dsi_display %pK, panel %pK\n",
			  dsi_display,
			  ((dsi_display) ? dsi_display->panel : NULL));
		return -EINVAL;
	}

	if (!c_conn->encoder || !c_conn->encoder->crtc ||
			!c_conn->encoder->crtc->state) {
		return 0;
	}

	refresh_rate = dsi_display->panel->cur_mode->timing.refresh_rate;
	us_per_frame = 1000000/refresh_rate;
	delay_us = (us_per_frame >> 1) + 500;

	fingerprint_mode = sde_crtc_get_fingerprint_mode(c_conn->encoder->crtc->state);

	if (OPLUS_DISPLAY_AOD_SCENE == get_oplus_display_scene()) {
		if (sde_crtc_get_fingerprint_pressed(c_conn->encoder->crtc->state)) {
			sde_crtc_set_onscreenfinger_defer_sync(c_conn->encoder->crtc->state, true);

		} else {
			sde_crtc_set_onscreenfinger_defer_sync(c_conn->encoder->crtc->state, false);
			fingerprint_mode = false;
		}

	} else {
		sde_crtc_set_onscreenfinger_defer_sync(c_conn->encoder->crtc->state, false);
	}

	if (fingerprint_mode != dsi_display->panel->is_hbm_enabled) {
		struct drm_crtc *crtc = c_conn->encoder->crtc;
		struct dsi_panel *panel = dsi_display->panel;
		int vblank = 0;
		u32 target_vblank, current_vblank;
		int ret;

		if (oplus_fod_on_vblank >= 0) {
			panel->cur_mode->priv_info->fod_on_vblank = oplus_fod_on_vblank;
		}

		if (oplus_fod_off_vblank >= 0) {
			panel->cur_mode->priv_info->fod_off_vblank = oplus_fod_off_vblank;
		}

		pr_err("OnscreenFingerprint mode: %s",
		       fingerprint_mode ? "Enter" : "Exit");

		dsi_display->panel->is_hbm_enabled = fingerprint_mode;

		if (fingerprint_mode) {
			if (!dsi_display->panel->oplus_priv.is_aod_ramless || oplus_display_mode) {
				mutex_lock(&dsi_display->panel->panel_lock);

			if (!dsi_display->panel->panel_initialized) {
				dsi_display->panel->is_hbm_enabled = false;
				pr_err("panel not initialized, failed to Enter OnscreenFingerprint\n");
				mutex_unlock(&dsi_display->panel->panel_lock);
				return 0;
			}

			dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
					     DSI_CORE_CLK, DSI_CLK_ON);

			if (oplus_seed_backlight) {
				int frame_time_us;

				frame_time_us = mult_frac(1000, 1000, panel->cur_mode->timing.refresh_rate);
				oplus_panel_process_dimming_v2(panel, panel->bl_config.bl_level, true);
				oplus_mipi_dsi_dcs_set_display_brightness(&panel->mipi_device,
								    panel->bl_config.bl_level);
				oplus_panel_process_dimming_v2_post(panel, true);
				usleep_range(frame_time_us, frame_time_us + 100);
			}

#ifdef OPLUS_FEATURE_AOD_RAMLESS
				if (dsi_display->panel->oplus_priv.is_aod_ramless) {
					ktime_t delta = ktime_sub(ktime_get(), oplus_backlight_time);
					s64 delta_us = ktime_to_us(delta);
					if (delta_us < 34000 && delta_us >= 0)
						usleep_range(34000 - delta_us, 34000 - delta_us + 100);
				}
#endif /* OPLUS_FEATURE_AOD_RAMLESS */

				if (OPLUS_DISPLAY_AOD_SCENE != get_oplus_display_scene() &&
						dsi_display->panel->bl_config.bl_level) {
				if (!is_nonsupport_ramless(dsi_display->panel->oplus_priv.vendor_name)) {
					if (dsi_display->config.panel_mode != DSI_OP_VIDEO_MODE) {
						current_vblank = drm_crtc_vblank_count(crtc);
						ret = wait_event_timeout(*drm_crtc_vblank_waitqueue(crtc),
									 current_vblank != drm_crtc_vblank_count(crtc),
									 msecs_to_jiffies(17));
					}

					vblank = panel->cur_mode->priv_info->fod_on_vblank;
					target_vblank = drm_crtc_vblank_count(crtc) + vblank;
					SDE_ATRACE_BEGIN("DSI_CMD_HBM_ON");
					rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_HBM_ON);
					SDE_ATRACE_END("DSI_CMD_HBM_ON");

					if (vblank) {
						ret = wait_event_timeout(*drm_crtc_vblank_waitqueue(crtc),
									target_vblank == drm_crtc_vblank_count(crtc),
									msecs_to_jiffies((vblank + 1) * 17));

						if (!ret) {
							pr_err("OnscreenFingerprint failed to wait vblank timeout target_vblank=%d current_vblank=%d\n",
								target_vblank, drm_crtc_vblank_count(crtc));
						}
					}
				} else {
					if (OPLUS_DISPLAY_AOD_HBM_SCENE == get_oplus_display_scene()) {
						rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_AOD_HBM_ON);
					} else {
						fingerprint_wait_vsync(c_conn->encoder, dsi_display->panel);
						usleep_range(delay_us, delay_us + 100);
						SDE_ATRACE_BEGIN("DSI_CMD_HBM_ON");
						rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_HBM_ON);
						SDE_ATRACE_END("DSI_CMD_HBM_ON");
					}
				}
			} else {
				if (is_nonsupport_ramless(dsi_display->panel->oplus_priv.vendor_name)) {
					if (OPLUS_DISPLAY_AOD_SCENE == get_oplus_display_scene()) {
						rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_AOD_HBM_ON);
					} else {
						rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_HBM_ON);
					}
				} else {
					rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_AOD_HBM_ON);
				}
			}

			dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
					     DSI_CORE_CLK, DSI_CLK_OFF);

			mutex_unlock(&dsi_display->panel->panel_lock);

			if (rc) {
				pr_err("failed to send DSI_CMD_HBM_ON cmds, rc=%d\n", rc);
				return rc;
			}
			}
		} else {
			mutex_lock(&dsi_display->panel->panel_lock);

			if (!dsi_display->panel->panel_initialized) {
				dsi_display->panel->is_hbm_enabled = true;
				pr_err("panel not initialized, failed to Exit OnscreenFingerprint\n");
				mutex_unlock(&dsi_display->panel->panel_lock);
				return 0;
			}

			if (!is_nonsupport_ramless(dsi_display->panel->oplus_priv.vendor_name)) {
				oplus_skip_datadimming_sync = true;
				oplus_panel_update_backlight_unlock(panel);
				oplus_skip_datadimming_sync = false;
			}

			vblank = panel->cur_mode->priv_info->fod_off_vblank;
			target_vblank = drm_crtc_vblank_count(crtc) + vblank;

			dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
					     DSI_CORE_CLK, DSI_CLK_ON);

			if (dsi_display->config.panel_mode == DSI_OP_VIDEO_MODE) {
				panel->oplus_priv.skip_mipi_last_cmd = true;
			}

			if (dsi_display->config.panel_mode == DSI_OP_VIDEO_MODE) {
				panel->oplus_priv.skip_mipi_last_cmd = false;
			}

			if (OPLUS_DISPLAY_AOD_HBM_SCENE == get_oplus_display_scene()) {
				if (OPLUS_DISPLAY_POWER_DOZE_SUSPEND == get_oplus_display_power_status() ||
						OPLUS_DISPLAY_POWER_DOZE == get_oplus_display_power_status()) {
					rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_AOD_HBM_OFF);
					mutex_unlock(&dsi_display->panel->panel_lock);
					/* Update aod light mode and fix 3658965*/
					mutex_lock(&dsi_display->panel->panel_lock);
					oplus_update_aod_light_mode_unlock(panel);
					set_oplus_display_scene(OPLUS_DISPLAY_AOD_SCENE);
				} else {
					if (!is_nonsupport_ramless(dsi_display->panel->oplus_priv.vendor_name))
						rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_SET_NOLP);

					if (dsi_display->panel->oplus_priv.is_aod_ramless) {
						oplus_set_hbm_off_flag = true;
					} else {
						SDE_ATRACE_BEGIN("DSI_CMD_HBM_OFF");
						rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_HBM_OFF);
						SDE_ATRACE_END("DSI_CMD_HBM_OFF");
					}
					if (is_nonsupport_ramless(dsi_display->panel->oplus_priv.vendor_name)) {
						oplus_panel_update_backlight_unlock(dsi_display->panel);
						set_oplus_display_scene(OPLUS_DISPLAY_NORMAL_SCENE);
					}
					oplus_first_vid = false;
					/* set nolp would exit hbm, restore when panel status on hbm */
					if (panel->bl_config.bl_level > panel->bl_config.bl_normal_max_level) {
						oplus_panel_update_backlight_unlock(panel);
					}

					if (oplus_display_get_hbm_mode()) {
						rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_AOD_HBM_ON);
					}

					set_oplus_display_scene(OPLUS_DISPLAY_NORMAL_SCENE);
				}

			} else if (oplus_display_get_hbm_mode()) {
				/* Do nothing to skip hbm off */
			} else if (OPLUS_DISPLAY_AOD_SCENE == get_oplus_display_scene()) {
				rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_AOD_HBM_OFF);
				mutex_unlock(&dsi_display->panel->panel_lock);
				/* Update aod light mode and fix 3658965*/
				mutex_lock(&dsi_display->panel->panel_lock);
				oplus_update_aod_light_mode_unlock(panel);
			} else {
				if (dsi_display->panel->oplus_priv.is_aod_ramless) {
					oplus_set_hbm_off_flag = true;
				} else {
					SDE_ATRACE_BEGIN("DSI_CMD_HBM_OFF");
					rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_HBM_OFF);
					SDE_ATRACE_END("DSI_CMD_HBM_OFF");
				}
				if (is_nonsupport_ramless(dsi_display->panel->oplus_priv.vendor_name))
					oplus_panel_update_backlight_unlock(dsi_display->panel);
				oplus_first_vid = false;
			}

			dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
					     DSI_CORE_CLK, DSI_CLK_OFF);
			mutex_unlock(&dsi_display->panel->panel_lock);
		}
	} else if (!dsi_display->panel->is_hbm_enabled \
		&& oplus_first_vid && dsi_display->panel->oplus_priv.is_aod_ramless && !oplus_aod_mode) {
		mutex_lock(&dsi_display->panel->panel_lock);

		if (!dsi_display->panel->panel_initialized) {
			pr_err("panel not initialized, failed to Enter OnscreenFingerprint\n");
			mutex_unlock(&dsi_display->panel->panel_lock);
			return 0;
		}

		dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);

		rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_HBM_OFF);
		pr_err("aod out to send DSI_CMD_HBM_OFF\n");

		oplus_first_vid = false;

		dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);

		mutex_unlock(&dsi_display->panel->panel_lock);
		if (rc) {
			pr_err("failed to send DSI_CMD_HBM_OFF cmds, rc=%d\n", rc);
			return rc;
		}

		oplus_first_vid = false;
	}

	if (oplus_dimlayer_hbm_vblank_count > 0) {
		oplus_dimlayer_hbm_vblank_count--;

	} else {
		while (atomic_read(&oplus_dimlayer_hbm_vblank_ref) > 0) {
			drm_crtc_vblank_put(connector->state->crtc);
			atomic_dec(&oplus_dimlayer_hbm_vblank_ref);
		}
	}

	return 0;
}
EXPORT_SYMBOL(sde_connector_update_hbm);

int oplus_seed_bright_to_alpha(int brightness)
{
	int level = ARRAY_SIZE(brightness_seed_alpha_lut_dc);
	int i = 0;
	int alpha;

	if (oplus_panel_alpha) {
		return oplus_panel_alpha;
	}

	for (i = 0; i < ARRAY_SIZE(brightness_seed_alpha_lut_dc); i++) {
		if (brightness_seed_alpha_lut_dc[i].brightness >= brightness) {
			break;
		}
	}

	if (i == 0) {
		alpha = brightness_seed_alpha_lut_dc[0].alpha;
	} else if (i == level) {
		alpha = brightness_seed_alpha_lut_dc[level - 1].alpha;
	} else
		alpha = interpolate(brightness,
				    brightness_seed_alpha_lut_dc[i - 1].brightness,
				    brightness_seed_alpha_lut_dc[i].brightness,
				    brightness_seed_alpha_lut_dc[i - 1].alpha,
				    brightness_seed_alpha_lut_dc[i].alpha);

	return alpha;
}

struct dsi_panel_cmd_set *
oplus_dsi_update_seed_backlight(struct dsi_panel *panel, int brightness,
			       enum dsi_cmd_set_type type)
{
	enum dsi_cmd_set_state state;
	struct dsi_cmd_desc *cmds;
	struct dsi_cmd_desc *oplus_cmd;
	u8 *tx_buf;
	int count, rc = 0;
	int i = 0;
	int k = 0;
	int alpha = oplus_seed_bright_to_alpha(brightness);

	if (type != DSI_CMD_SEED_MODE0 &&
			type != DSI_CMD_SEED_MODE1 &&
			type != DSI_CMD_SEED_MODE2 &&
			type != DSI_CMD_SEED_MODE3 &&
			type != DSI_CMD_SEED_MODE4 &&
			type != DSI_CMD_SEED_OFF) {
		return NULL;
	}

	if (type == DSI_CMD_SEED_OFF) {
		type = DSI_CMD_SEED_MODE0;
	}

	cmds = panel->cur_mode->priv_info->cmd_sets[type].cmds;
	count = panel->cur_mode->priv_info->cmd_sets[type].count;
	state = panel->cur_mode->priv_info->cmd_sets[type].state;

	oplus_cmd = kmemdup(cmds, sizeof(*cmds) * count, GFP_KERNEL);

	if (!oplus_cmd) {
		rc = -ENOMEM;
		goto error;
	}

	for (i = 0; i < count; i++) {
		oplus_cmd[i].msg.tx_buf = NULL;
	}

	for (i = 0; i < count; i++) {
		u32 size;

		size = oplus_cmd[i].msg.tx_len * sizeof(u8);

		oplus_cmd[i].msg.tx_buf = kmemdup(cmds[i].msg.tx_buf, size, GFP_KERNEL);

		if (!oplus_cmd[i].msg.tx_buf) {
			rc = -ENOMEM;
			goto error;
		}
	}

	for (i = 0; i < count; i++) {
		if (oplus_cmd[i].msg.tx_len != 0x16) {
			continue;
		}

		tx_buf = (u8 *)oplus_cmd[i].msg.tx_buf;

		for (k = 0; k < oplus_cmd[i].msg.tx_len; k++) {
			if (k == 0) {
				continue;
			}

			tx_buf[k] = tx_buf[k] * (255 - alpha) / 255;
		}
	}

	if (oplus_priv_seed_cmd_set.cmds) {
		for (i = 0; i < oplus_priv_seed_cmd_set.count; i++) {
			kfree(oplus_priv_seed_cmd_set.cmds[i].msg.tx_buf);
		}

		kfree(oplus_priv_seed_cmd_set.cmds);
	}

	oplus_priv_seed_cmd_set.cmds = oplus_cmd;
	oplus_priv_seed_cmd_set.count = count;
	oplus_priv_seed_cmd_set.state = state;
	oplus_dc2_alpha = alpha;

	return &oplus_priv_seed_cmd_set;

error:

	if (oplus_cmd) {
		for (i = 0; i < count; i++) {
			kfree(oplus_cmd[i].msg.tx_buf);
		}

		kfree(oplus_cmd);
	}

	return ERR_PTR(rc);
}
EXPORT_SYMBOL(oplus_dsi_update_seed_backlight);

int oplus_display_panel_get_dim_alpha(void *buf)
{
	unsigned int *temp_alpha = buf;
	struct dsi_display *display = get_main_display();

	if (!display->panel->is_hbm_enabled ||
			(get_oplus_display_power_status() != OPLUS_DISPLAY_POWER_ON)) {
		(*temp_alpha) = 0;
		return 0;
	}

	(*temp_alpha) = oplus_underbrightness_alpha;
	return 0;
}

int oplus_display_panel_set_dim_alpha(void *buf)
{
	unsigned int *temp_alpha = buf;

	(*temp_alpha) = oplus_panel_alpha;

	return 0;
}

int oplus_display_panel_get_dim_dc_alpha(void *buf)
{
	int ret = 0;
	unsigned int *temp_dim_alpha = buf;
	struct dsi_display *display = get_main_display();

	if (display->panel->is_hbm_enabled ||
			get_oplus_display_power_status() != OPLUS_DISPLAY_POWER_ON) {
		ret = 0;
	}

	if (oplus_dc2_alpha != 0) {
		ret = oplus_dc2_alpha;

	} else if (oplus_underbrightness_alpha != 0) {
		ret = oplus_underbrightness_alpha;

	} else if (oplus_dimlayer_bl_enable_v3_real) {
		ret = 1;
	}

	(*temp_dim_alpha) = ret;
	return 0;
}

int oplus_display_panel_set_dimlayer_enable(void *data)
{
	struct dsi_display *display = NULL;
	struct drm_connector *dsi_connector = NULL;
	uint32_t *dimlayer_enable = data;

	display = get_main_display();
	if (!display) {
		return -EINVAL;
	}

	dsi_connector = display->drm_conn;
	if (display && display->name) {
		int enable = (*dimlayer_enable);
		int err = 0;

		mutex_lock(&display->display_lock);
		if (!dsi_connector || !dsi_connector->state || !dsi_connector->state->crtc) {
			pr_err("[%s]: display not ready\n", __func__);
		} else {
			err = drm_crtc_vblank_get(dsi_connector->state->crtc);
			if (err) {
				pr_err("failed to get crtc vblank, error=%d\n", err);
			} else {
				/* do vblank put after 7 frames */
				oplus_datadimming_vblank_count = 7;
				atomic_inc(&oplus_datadimming_vblank_ref);
			}
		}

		usleep_range(17000, 17100);
		if (!strcmp(display->panel->oplus_priv.vendor_name, "ANA6706")) {
			oplus_dimlayer_bl_enable = enable;
		} else {
			if (!strcmp(display->name, "qcom,mdss_dsi_oplus19101boe_nt37800_1080_2400_cmd"))
				oplus_dimlayer_bl_enable_v3 = enable;
			else
				oplus_dimlayer_bl_enable_v2 = enable;
		}
		mutex_unlock(&display->display_lock);
	}

	return 0;
}

int oplus_display_panel_get_dimlayer_enable(void *data)
{
	uint32_t *dimlayer_bl_enable = data;

	(*dimlayer_bl_enable) = oplus_dimlayer_bl_enable_v2;

	return 0;
}
static struct task_struct *press_event_notify_task;
static wait_queue_head_t press_event_notify_task_wq;
static atomic_t press_event_task_task_wakeup = ATOMIC_INIT(0);
static bool press_event_delay_off = false;
extern int msm_drm_notifier_call_chain(unsigned long val, void *v);
static int press_event_delay_time = 0;

void dsi_press_event_delay(int delay)
{
	if (!press_event_delay_off)
		return;

	pr_err("fingerprint status: pressed2");
	usleep_range(delay*1000, delay*1000 + 100);
	oplus_notify_fingerprint_press_event(true);
	press_event_delay_off = false;
	return;
}

static int press_event_worker_kthread(void *data)
{
	int ret = 0;

	while (1) {
		ret = wait_event_interruptible(press_event_notify_task_wq, atomic_read(&press_event_task_task_wakeup));
		atomic_set(&press_event_task_task_wakeup, 0);
		dsi_press_event_delay(press_event_delay_time);
		if (kthread_should_stop())
			break;
	}
	return 0;
}

void press_event_notify_init(void)
{
	if (!press_event_notify_task) {
		press_event_notify_task = kthread_create(press_event_worker_kthread, NULL, "press_event_notify");
		init_waitqueue_head(&press_event_notify_task_wq);
		wake_up_process(press_event_notify_task);
		pr_info("[press_event notify]  init CREATE\n");
	}
}

void press_event_notify(int delay)
{
	if (press_event_notify_task != NULL) {
		press_event_delay_off = true;
		press_event_delay_time = delay;
		atomic_set(&press_event_task_task_wakeup, 1);
		wake_up_interruptible(&press_event_notify_task_wq);
	} else {
		pr_info("[press_event notify] notify is NULL\n");
	}
}

void oplus_notify_fingerprint_press_event(bool press)
{
	bool blank = press;
	struct msm_drm_notifier notifier_data;
	notifier_data.data = &blank;

	SDE_ATRACE_BEGIN("MSM_DRM_ONSCREENFINGERPRINT_EVENT");
	msm_drm_notifier_call_chain(MSM_DRM_ONSCREENFINGERPRINT_EVENT, &notifier_data);
	SDE_ATRACE_END("MSM_DRM_ONSCREENFINGERPRINT_EVENT");
}

void oplus_notify_hbm_off(void)
{
	if (oplus_set_hbm_off_flag) {
		int rc = 0;
		struct dsi_display *display = get_main_display();

		if (!display || !display->panel) {
			SDE_ERROR("failed to find display\n");
			return;
		}

		SDE_ATRACE_BEGIN("DSI_CMD_HBM_OFF");
		mutex_lock(&display->panel->panel_lock);

		/* enable the clk vote for CMD mode panels */
		if (display->config.panel_mode == DSI_OP_CMD_MODE) {
			dsi_display_clk_ctrl(display->dsi_clk_handle,
					DSI_CORE_CLK, DSI_CLK_ON);
		}

		if (display->panel->bl_config.bl_level <= 2047)
			rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_HBM_OFF);
		else
			rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_HBM_OFF_HIGHLIGHT);

		if (display->config.panel_mode == DSI_OP_CMD_MODE) {
			rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
					DSI_CORE_CLK, DSI_CLK_OFF);
		}

		mutex_unlock(&display->panel->panel_lock);
		SDE_ATRACE_END("DSI_CMD_HBM_OFF");
		oplus_set_hbm_off_flag = false;
	}
}

