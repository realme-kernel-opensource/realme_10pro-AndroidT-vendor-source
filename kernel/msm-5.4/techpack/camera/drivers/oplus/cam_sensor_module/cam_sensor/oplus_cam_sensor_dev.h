/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _OPLUS_CAM_SENSOR_DEV_H_
#define _OPLUS_CAM_SENSOR_DEV_H_

#include "cam_sensor_dev.h"

struct cam_sensor_i2c_reg_setting_array {
	struct cam_sensor_i2c_reg_array reg_setting[4600];
	unsigned short size;
	enum camera_sensor_i2c_type addr_type;
	enum camera_sensor_i2c_type data_type;
	unsigned short delay;
};

struct cam_sensor_settings {
	struct cam_sensor_i2c_reg_setting_array s5kgm1st_setting;
	struct cam_sensor_i2c_reg_setting_array hi846_setting;
	struct cam_sensor_i2c_reg_setting_array ov8856_setting;
	struct cam_sensor_i2c_reg_setting_array ov02b10_setting;
	struct cam_sensor_i2c_reg_setting_array gc02k0_setting;
	struct cam_sensor_i2c_reg_setting_array gc02m1b_setting;
	struct cam_sensor_i2c_reg_setting_array ov48b_setting;
	struct cam_sensor_i2c_reg_setting_array imx471_setting;
	struct cam_sensor_i2c_reg_setting_array ov13b10_setting;
	struct cam_sensor_i2c_reg_setting_array ov64b_setting;
	struct cam_sensor_i2c_reg_setting_array imx615_setting;
	struct cam_sensor_i2c_reg_setting_array ov08d10_setting;
	struct cam_sensor_i2c_reg_setting_array ov16a1q_setting;
	struct cam_sensor_i2c_reg_setting_array imx581_setting;
	struct cam_sensor_i2c_reg_setting_array s5kjn1sq_setting;
	struct cam_sensor_i2c_reg_setting_array hi1336_setting;
	struct cam_sensor_i2c_reg_setting_array sc800cs_setting;
	struct cam_sensor_i2c_reg_setting_array sc1300cs_setting;
	struct cam_sensor_i2c_reg_setting_array s5k3p9sp_setting;
	struct cam_sensor_i2c_reg_setting_array sc201cs_depth_setting;
	struct cam_sensor_i2c_reg_setting_array sc201cs_micro_setting;
	struct cam_sensor_i2c_reg_setting_array s5khm6_setting;
	struct cam_sensor_i2c_reg_setting_array s5khm6_a303_setting;
	struct cam_sensor_i2c_reg_setting_array ov32c_setting;
};

long oplus_cam_sensor_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg, unsigned int *is_ftm_current_test);

int cam_sensor_stop(struct cam_sensor_ctrl_t *s_ctrl);
int cam_sensor_start(struct cam_sensor_ctrl_t *s_ctrl);
#endif /* _OPLUS_CAM_SENSOR_DEV_H_ */
