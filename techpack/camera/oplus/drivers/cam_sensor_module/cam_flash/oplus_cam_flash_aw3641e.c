// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */
#include "oplus_cam_flash_aw3641e.h"

#define __FIXED_FLASH_T0__ 1
#define PWM_FRQ 48000
#define PWM_DEN 100
#define AW3641E_FLASH_STATE_ON 1
#define AW3641E_FLASH_STATE_OFF 0
static struct clk *pwm_clk = NULL;
static int aw3641e_timer_flag = AW3641E_FLASH_STATE_OFF;
struct cam_flash_timer *flash_aw3641e_timer = NULL;

#ifdef __FIXED_FLASH_T0__
#define EVT_LOT1B_STATUS  0
#define T0_LOT1_STATUS  1
#define NOT_FOUND_BOARD_STATUS -1

static int mboard_id_status = NOT_FOUND_BOARD_STATUS;

struct board_id_soc_info mBoard_id_soc_info = {
	.hw_id1_gpio = 0,
	.hw_id2_gpio = 0,
	.pinctrl = NULL,
	.pin_hw1_high = NULL,
	.pin_hw1_low = NULL,
	.pin_hw2_high = NULL,
	.pin_hw2_low = NULL,
};

int cam_get_hw_id1_status(int gpio_name)
{
	int ret = 0, active_val = 0, sleep_val = 0, hw_flag = 0;
	if (gpio_name == 0)
		return -1;
	if ((NULL == mBoard_id_soc_info.pinctrl) || (NULL == mBoard_id_soc_info.pin_hw1_high) || (NULL == mBoard_id_soc_info.pin_hw1_low))
	{
		CAM_ERR(CAM_FLASH,"Pinctrl get failed");
		return -1;
	}

	ret = pinctrl_select_state(mBoard_id_soc_info.pinctrl, mBoard_id_soc_info.pin_hw1_high);
	if (ret < 0) {
		CAM_ERR(CAM_FLASH,"Set hwid1 high pin state error:%d", ret);
	}
	active_val = gpio_get_value(gpio_name);

	ret = pinctrl_select_state(mBoard_id_soc_info.pinctrl, mBoard_id_soc_info.pin_hw1_low);
	if (ret < 0) {
		CAM_ERR(CAM_FLASH,"Set hwid1 low pin state error:%d", ret);
	}
	sleep_val = gpio_get_value(gpio_name);
	/**
	 * hw_flag = 0  high res 0
	 * hw_flag = 2  pull up 2
	 * hw_flag = 1  pull down 1
	 */
	if (active_val == 1 && sleep_val == 0) {
		hw_flag = 0;
	} else if (active_val == 1 && sleep_val == 1) {
		hw_flag = 2;
	} else if (active_val == 0 && sleep_val == 0) {
		hw_flag = 1;
	}
	gpio_free(gpio_name);
	return hw_flag;
}

int cam_get_hw_id2_status(int gpio_name)
{
	int ret = 0, active_val = 0, sleep_val = 0, hw_flag = 0;
	if (gpio_name == 0)
	{
		return -1;
	}
	if ((NULL == mBoard_id_soc_info.pinctrl) || (NULL == mBoard_id_soc_info.pin_hw2_high) || (NULL == mBoard_id_soc_info.pin_hw2_low))
	{
		CAM_ERR(CAM_FLASH,"Pinctrl get failed");
		return -1;
	}
	ret = pinctrl_select_state(mBoard_id_soc_info.pinctrl, mBoard_id_soc_info.pin_hw2_high);
	if (ret < 0) {
		CAM_ERR(CAM_FLASH,"Set hwid2 active pin state error:%d", ret);
	}
	active_val = gpio_get_value(gpio_name);

	ret = pinctrl_select_state(mBoard_id_soc_info.pinctrl, mBoard_id_soc_info.pin_hw2_low);
	if (ret < 0) {
		CAM_ERR(CAM_FLASH,"Set hwid2 sleep pin state error:%d", ret);
	}
	sleep_val = gpio_get_value(gpio_name);
	/**
	 * hw_flag = 0  high res 0
	 * hw_flag = 2  pull up 2
	 * hw_flag = 1  pull down 1
	 */
	if (active_val == 1 && sleep_val == 0) {
		hw_flag = 0;
	} else if (active_val == 1 && sleep_val == 1) {
		hw_flag = 2;
	} else if (active_val == 0 && sleep_val == 0) {
		hw_flag = 1;
	}
	gpio_free(gpio_name);
	return hw_flag;
}

int cam_get_hw_board_id()
{
	int ret1 = 0, ret2 = 0;

	ret1 = cam_get_hw_id1_status(mBoard_id_soc_info.hw_id1_gpio);
	ret2 = cam_get_hw_id2_status(mBoard_id_soc_info.hw_id2_gpio);

	CAM_DBG(CAM_FLASH,"%s, ret1 = %d, ret2 = %d", __func__, ret1, ret2);
	if ((ret1 == 1 && ret2 == 1) || (ret1 == 1 && ret2 == 2)) {
		mboard_id_status = T0_LOT1_STATUS;	/*use gpio 100*/
	} else {
		mboard_id_status = EVT_LOT1B_STATUS;	/*use gpio 86*/
	}
	CAM_DBG(CAM_FLASH,"mboard_id_status = %d",mboard_id_status);
	return mboard_id_status;
}
#endif

int cam_flash_timer_init(struct cam_sensor_power_ctrl_t *power_info)
{
	int ret = 0;
	flash_aw3641e_timer = kzalloc(sizeof(*flash_aw3641e_timer), GFP_KERNEL);
	if (!flash_aw3641e_timer) {
		return -ENOMEM;
	}
	flash_aw3641e_timer->power_info = power_info;
	timer_setup(&flash_aw3641e_timer->flash_timer,
				cam_flash_timer_callback, 0);

	return ret;
}

void cam_flash_timer_callback(struct timer_list *timer_data)
{
	struct msm_camera_gpio_num_info *gpio_num_info = NULL;
	if (!flash_aw3641e_timer) {
		CAM_ERR(CAM_FLASH, "flash_aw3641e_timer is null");
		return;
	}
	gpio_num_info = flash_aw3641e_timer->power_info->gpio_num_info;

	CAM_DBG(CAM_FLASH,"cam_flash_timer_callback");
	if (aw3641e_timer_flag == AW3641E_FLASH_STATE_ON) {
		cam_res_mgr_gpio_set_value(gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1], 0);
		cam_res_mgr_gpio_set_value(gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2], 0);
	}
}

void cam_flash_timer_exit()
{
	CAM_DBG(CAM_FLASH, "destroy flash_aw3641e_timer %pK", flash_aw3641e_timer);
	if (!flash_aw3641e_timer) {
		CAM_ERR(CAM_FLASH, "flash_aw3641e_timer is null");
		return;
	} else {
		del_timer_sync(&flash_aw3641e_timer->flash_timer);
		kfree(flash_aw3641e_timer);
		flash_aw3641e_timer = NULL;
		CAM_DBG(CAM_FLASH, "flash_aw3641e_timer free successful");
	}
}

int cam_flash_pwm_clk_enable(struct cam_flash_ctrl *flash_ctrl, int flash_duty)
{
	int ret = 0;
	struct cam_hw_soc_info  soc_info = flash_ctrl->soc_info;

	pwm_clk = devm_clk_get(soc_info.dev, soc_info.clk_name[0]);
	if (NULL == pwm_clk) {
		CAM_ERR(CAM_FLASH, "get pclk(gpio-pwm-clk) error");
		return -EINVAL;
	}

	ret = cam_soc_util_clk_enable(pwm_clk, soc_info.clk_name[0], PWM_FRQ);
	if (ret) {
		CAM_ERR(CAM_FLASH, "clk prepare and enable fail");
		return ret;
	} else {
		CAM_DBG(CAM_FLASH, "clk prepare and enable success");
	}

	ret = cam_soc_util_set_clk_duty_cycle(pwm_clk, soc_info.clk_name[0], flash_duty, PWM_DEN);
	if (ret) {
		CAM_ERR(CAM_FLASH, "clk duty cycle set fail");
		return ret;
	}
	return ret;
}

void cam_flash_pwm_clk_disable()
{
	if (pwm_clk == NULL) {
		return;
	} else {
		clk_disable_unprepare(pwm_clk);
	}
}

int cam_flash_pwm_selected(struct cam_flash_ctrl *flash_ctrl, bool is_pwm_selected)
{
	int ret;

	if (is_pwm_selected) {
		ret = pinctrl_select_state(flash_ctrl->power_info.pinctrl_info.pinctrl,
			flash_ctrl->power_info.pinctrl_info.pwm_state_active);
		if (ret)
			CAM_ERR(CAM_FLASH, "cannot set pin to pwm active state");
	} else {
		cam_flash_pwm_clk_disable();
		ret = pinctrl_select_state(flash_ctrl->power_info.pinctrl_info.pinctrl,
			flash_ctrl->power_info.pinctrl_info.gpio_state_active);
		if (ret)
			CAM_ERR(CAM_FLASH, "cannot set pin to gpio active state");
	}
	pr_info("is_pwm_selected:%d ret:%d\n", is_pwm_selected, ret);
	return ret;
}

int cam_flash_gpio_get_level(int flash_current)
{
	int flash_level;

	if (flash_current < 400)
		flash_level = 7;
	else if (flash_current >= 400 && flash_current < 500)
		flash_level = 6;
	else if (flash_current >= 500 && flash_current < 600)
		flash_level = 5;
	else if (flash_current >= 600 && flash_current < 700)
		flash_level = 4;
	else if (flash_current >= 700 && flash_current < 800)
		flash_level = 3;
	else if (flash_current >= 800 && flash_current < 900)
		flash_level = 2;
	else if (flash_current >= 900 && flash_current < 1000)
		flash_level = 1;
	else if (flash_current >= 1000)
		flash_level = 0;

	return flash_level;
}

int cam_flash_gpio_off(struct cam_flash_ctrl *flash_ctrl)
{
	struct cam_hw_soc_info  soc_info = flash_ctrl->soc_info;
	struct msm_camera_gpio_num_info *gpio_num_info = NULL;
	gpio_num_info = flash_ctrl->power_info.gpio_num_info;

	if (!flash_ctrl || !gpio_num_info) {
		CAM_ERR(CAM_FLASH, "Flash control Null");
		return -EINVAL;
	}

	flash_ctrl->flash_state = CAM_FLASH_STATE_START;
	aw3641e_timer_flag = AW3641E_FLASH_STATE_OFF;
#ifdef __FIXED_FLASH_T0__
	if (mboard_id_status == T0_LOT1_STATUS) {
		CAM_ERR(CAM_FLASH, "Flash T0 board");
		return 0;
	}
#endif
	CAM_INFO(CAM_FLASH,"cam_flash_gpio_off");
	if (gpio_num_info->valid[SENSOR_CUSTOM_GPIO1]) {
		cam_res_mgr_gpio_set_value(gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1], 0);
		cam_res_mgr_gpio_free(soc_info.dev, gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1]);
	}

	if (gpio_num_info->valid[SENSOR_CUSTOM_GPIO2]) {
		cam_res_mgr_gpio_set_value(gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2], 0);
		cam_res_mgr_gpio_free(soc_info.dev, gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2]);
	}
	cam_flash_pwm_clk_disable();
	return 0;
}

int cam_flash_gpio_high(struct cam_flash_ctrl *flash_ctrl,
	struct cam_flash_frame_setting *flash_data)
{
	int rc = 0;
	int i, level = 0;
	struct cam_hw_soc_info  soc_info = flash_ctrl->soc_info;
	struct msm_camera_gpio_num_info *gpio_num_info = NULL;
	gpio_num_info = flash_ctrl->power_info.gpio_num_info;
	aw3641e_timer_flag = AW3641E_FLASH_STATE_ON;

	if (!flash_data || !gpio_num_info) {
		CAM_ERR(CAM_FLASH, "Flash Data Null");
		return -EINVAL;
	}
#ifdef __FIXED_FLASH_T0__
	if (mboard_id_status == T0_LOT1_STATUS) {
		CAM_ERR(CAM_FLASH, "Flash T0 board");
		return 0;
	}
#endif
	CAM_INFO(CAM_FLASH, "Flash high Triggered");
	rc = cam_flash_pwm_selected(flash_ctrl, false);
	if (rc) {
		CAM_ERR(CAM_FLASH, "flash select gpio failed");
		return rc;
	}

	if (gpio_num_info->valid[SENSOR_CUSTOM_GPIO2]) {
		rc = cam_res_mgr_gpio_request(soc_info.dev, gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2], 0, "CUSTOM_GPIO2");
		if(rc) {
			CAM_ERR(CAM_FLASH, "flash_torch_gpio %d request fails", gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2]);
			return rc;
		}
		cam_res_mgr_gpio_set_value(gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2], 1);
	}

	if (gpio_num_info->valid[SENSOR_CUSTOM_GPIO1]) {
		rc = cam_res_mgr_gpio_request(soc_info.dev, gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1], 0, "CUSTOM_GPIO1");
		if(rc) {
			CAM_ERR(CAM_FLASH, "flash_en_gpio %d request fails", gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1]);
			return rc;
		}
		level = cam_flash_gpio_get_level(flash_data->led_current_ma[0]);
		CAM_DBG(CAM_FLASH, "Flash level %d, led_current_ma %d", level, flash_data->led_current_ma[0]);
		for (i = 0; i < level; ++i) {
			cam_res_mgr_gpio_set_value(gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1], 1);
			cam_res_mgr_gpio_set_value(gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1], 0);
		}
		cam_res_mgr_gpio_set_value(gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1], 1);
	}
	mod_timer(&flash_aw3641e_timer->flash_timer,(jiffies + msecs_to_jiffies(210)));
	CAM_DBG(CAM_FLASH, "flash high add timer successful");

	return rc;
}

int cam_flash_gpio_low(struct cam_flash_ctrl *flash_ctrl,struct cam_flash_frame_setting *flash_data)
{
	int rc = 0;
	int retry = 3;
	int duty = 0;
	int flash_current = 0;
	struct cam_hw_soc_info soc_info = flash_ctrl->soc_info;
	struct msm_camera_gpio_num_info *gpio_num_info = NULL;
	gpio_num_info = flash_ctrl->power_info.gpio_num_info;
	if (!flash_data || !gpio_num_info) {
		CAM_ERR(CAM_FLASH, "Flash Data  is Null");
		return -EINVAL;
	}
#ifdef __FIXED_FLASH_T0__
	if (mboard_id_status == T0_LOT1_STATUS) {
		CAM_ERR(CAM_FLASH, "Flash T0 board");
		return 0;
	}
#endif
	flash_current = flash_data->led_current_ma[0];
	if (flash_current > 100) {
		duty = (flash_current - 100)/9;
	} else {
		duty = 0;
	}
	CAM_DBG(CAM_FLASH,"flash_current %d, duty %d", flash_current, duty);
	CAM_DBG(CAM_FLASH, "Flash low flash_en_gpio %d, valid = %d",gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1],gpio_num_info->valid[SENSOR_CUSTOM_GPIO1]);
	if (gpio_num_info->valid[SENSOR_CUSTOM_GPIO1]) {
		do {
			rc = cam_res_mgr_gpio_request(soc_info.dev, gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1], 0, "CUSTOM_GPIO1");
			if(rc) {
				CAM_ERR(CAM_FLASH, "flash_en_gpio %d request fails", gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1]);
				if (retry) {
					cam_res_mgr_gpio_free(soc_info.dev, gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1]);
					usleep_range(10, 11);
				} else
					return rc;
			} else
				break;
		} while (retry--);
		cam_res_mgr_gpio_set_value(gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1], 1);
	}

	if (duty == 0) {
		rc = cam_flash_pwm_selected(flash_ctrl, false);
		if (rc) {
			CAM_ERR(CAM_FLASH, "flash select gpio failed");
			return rc;
		}
		CAM_DBG(CAM_FLASH, "Flash low en flash_torch_gpio %d, valid = %d",gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2],gpio_num_info->valid[SENSOR_CUSTOM_GPIO2]);
		if (gpio_num_info->valid[SENSOR_CUSTOM_GPIO2]) {
			rc = cam_res_mgr_gpio_request(soc_info.dev, gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2], 0, "CUSTOM_GPIO2");
			if(rc) {
				CAM_ERR(CAM_FLASH, "flash_torch_gpio %d request fails", gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2]);
				return rc;
			}
			cam_res_mgr_gpio_set_value(gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2], 0);
		}
	} else {
		rc = cam_flash_pwm_selected(flash_ctrl, true);
		if (rc) {
			CAM_ERR(CAM_FLASH, "flash select pwm failed");
			return rc;
		}
		// delay >= 2ms as spec for aw3641e
		mdelay(5);
		rc = cam_flash_pwm_clk_enable(flash_ctrl, duty);
		if (rc) {
			CAM_ERR(CAM_FLASH, "pwm clk enable set fail");
			return rc;
		}
	}
	return rc;
}

int cam_flash_gpio_flush_request(struct cam_flash_ctrl *fctrl,enum cam_flash_flush_type type, uint64_t req_id) {
	int rc = 0;
	int i = 0, j = 0;
	int frame_offset = 0;
	bool is_off_needed = false;
	struct cam_flash_frame_setting *flash_data = NULL;
	CAM_DBG(CAM_FLASH, "cam_flash_gpio");
	if (!fctrl) {
		CAM_ERR(CAM_FLASH, "Device data is NULL");
		return -EINVAL;
	}

	if (type == FLUSH_ALL) {
	/* flush all requests*/
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			flash_data =
				&fctrl->per_frame[i];
			if ((flash_data->opcode ==
				CAMERA_SENSOR_FLASH_OP_OFF) &&
				(flash_data->cmn_attr.request_id > 0) &&
				(flash_data->cmn_attr.request_id <= req_id) &&
				flash_data->cmn_attr.is_settings_valid) {
				is_off_needed = true;
				CAM_DBG(CAM_FLASH,
					"FLASH_ALL: Turn off the flash for req %llu",
					flash_data->cmn_attr.request_id);
			}

			flash_data->cmn_attr.request_id = 0;
			flash_data->cmn_attr.is_settings_valid = false;
			flash_data->cmn_attr.count = 0;
			for (j = 0; j < CAM_FLASH_MAX_LED_TRIGGERS; j++)
				flash_data->led_current_ma[j] = 0;
		}

		//cam_flash_pmic_flush_nrt(fctrl);
	} else if ((type == FLUSH_REQ) && (req_id != 0)) {
	/* flush request with req_id*/
		frame_offset = req_id % MAX_PER_FRAME_ARRAY;
		flash_data =
			&fctrl->per_frame[frame_offset];

		if (flash_data->opcode ==
			CAMERA_SENSOR_FLASH_OP_OFF) {
			is_off_needed = true;
			CAM_DBG(CAM_FLASH,
				"FLASH_REQ: Turn off the flash for req %llu",
				flash_data->cmn_attr.request_id);
		}

		flash_data->cmn_attr.request_id = 0;
		flash_data->cmn_attr.is_settings_valid =
			false;
		flash_data->cmn_attr.count = 0;
		for (i = 0; i < CAM_FLASH_MAX_LED_TRIGGERS; i++)
			flash_data->led_current_ma[i] = 0;
	} else if ((type == FLUSH_REQ) && (req_id == 0)) {
		/* Handels NonRealTime usecase */
		//cam_flash_pmic_flush_nrt(fctrl);
	} else {
		CAM_ERR(CAM_FLASH, "Invalid arguments");
		return -EINVAL;
	}

	if (is_off_needed)
		cam_flash_off(fctrl);

	return rc;
}

int cam_flash_gpio_power_on(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info)
{
	int rc = 0;

	CAM_DBG(CAM_FLASH, "Enter");
	if (!ctrl) {
		CAM_ERR(CAM_FLASH, "Invalid ctrl handle");
		return -EINVAL;
	}

	rc = msm_flash_pinctrl_init(&(ctrl->pinctrl_info), ctrl->dev);
	if (rc < 0) {
		/* Some sensor subdev no pinctrl. */
		CAM_ERR(CAM_FLASH, "Initialization of pinctrl failed");
		ctrl->cam_pinctrl_status = 0;
	} else {
		ctrl->cam_pinctrl_status = 1;
	}

	if (ctrl->cam_pinctrl_status) {
		rc = pinctrl_select_state(ctrl->pinctrl_info.pinctrl,ctrl->pinctrl_info.gpio_state_active);
		if (rc)
			CAM_ERR(CAM_FLASH, "cannot set pin to active state");
	}

	return rc;
}

int cam_flash_gpio_power_down(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info)
{
	int  ret = 0;

	CAM_DBG(CAM_FLASH, "Enter");
	if (!ctrl || !soc_info) {
		CAM_ERR(CAM_FLASH, "failed ctrl %pK",  ctrl);
		return -EINVAL;
	}

	if (ctrl->cam_pinctrl_status) {
		ret = pinctrl_select_state(
				ctrl->pinctrl_info.pinctrl,
				ctrl->pinctrl_info.gpio_state_suspend);
		if (ret)
			CAM_ERR(CAM_FLASH, "cannot set pin to suspend state");

		devm_pinctrl_put(ctrl->pinctrl_info.pinctrl);
	}

	//cam_res_mgr_shared_pinctrl_select_state(false);
	//cam_res_mgr_shared_pinctrl_put();
	ctrl->cam_pinctrl_status = 0;
	//cam_sensor_util_request_gpio_table(soc_info, 0);
	return ret;
}

int cam_flash_gpio_power_ops(struct cam_flash_ctrl *fctrl,bool regulator_enable){
	int rc = 0;
	struct cam_hw_soc_info *soc_info = &fctrl->soc_info;
	struct cam_sensor_power_ctrl_t *power_info =&fctrl->power_info;
	#ifdef __FIXED_FLASH_T0__
	struct device_node *tp_node;
	#endif
	static int init_inistance = 0;
	if (init_inistance > 0)
		return rc;
	init_inistance = 1;
	#ifdef __FIXED_FLASH_T0__
	tp_node = of_find_compatible_node(
					NULL, NULL, "focaltech,fts");
	if(!tp_node)
		CAM_ERR(CAM_FLASH, "not tp_node found");
	else {
		struct i2c_client *tp_pdev;
		mBoard_id_soc_info.hw_id1_gpio = of_get_named_gpio(tp_node, "hw-id1-gpio", 0);
		mBoard_id_soc_info.hw_id2_gpio = of_get_named_gpio(tp_node, "hw-id2-gpio", 0);
		tp_pdev = of_find_i2c_device_by_node(tp_node);
		//tp_pdev = of_find_device_by_node(tp_node);
		if (NULL == tp_pdev) {
			CAM_ERR(CAM_FLASH, "not tp_node found");
			return 0;
		}

		mBoard_id_soc_info.pinctrl = devm_pinctrl_get(&tp_pdev->dev);
		if (NULL != mBoard_id_soc_info.pinctrl)
		{
			mBoard_id_soc_info.pin_hw1_high = pinctrl_lookup_state(mBoard_id_soc_info.pinctrl, "pin_hw1_high");
			mBoard_id_soc_info.pin_hw1_low = pinctrl_lookup_state(mBoard_id_soc_info.pinctrl, "pin_hw1_low");
			mBoard_id_soc_info.pin_hw2_high = pinctrl_lookup_state(mBoard_id_soc_info.pinctrl, "pin_hw2_high");
			mBoard_id_soc_info.pin_hw2_low = pinctrl_lookup_state(mBoard_id_soc_info.pinctrl, "pin_hw2_low");
			cam_get_hw_board_id();
		}
		CAM_DBG(CAM_FLASH, "tp_node found hw_id1_gpio = %d,hw_id2_gpio=%d,pin_hw1_high =%p",mBoard_id_soc_info.hw_id1_gpio,mBoard_id_soc_info.hw_id2_gpio,mBoard_id_soc_info.pin_hw1_high);
		CAM_DBG(CAM_FLASH, "tp_node found pin_hw1_low =%p,pin_hw2_high = %p,pin_hw2_low =%p",mBoard_id_soc_info.pin_hw1_low,mBoard_id_soc_info.pin_hw2_high,mBoard_id_soc_info.pin_hw2_low);
		if (mboard_id_status == T0_LOT1_STATUS) {
			CAM_ERR(CAM_FLASH, "Flash T0 board");
			return 0;
		}
	}
	#endif
	if (!power_info || !soc_info) {
		CAM_ERR(CAM_FLASH, "Power Info is NULL");
		return -EINVAL;
	}
	power_info->dev = soc_info->dev;
	CAM_INFO(CAM_FLASH,"regulator_enable = %d,fctrl->is_regulator_enabled=%d",regulator_enable,fctrl->is_regulator_enabled);

	if (regulator_enable && (fctrl->is_regulator_enabled == false)) {
		rc = cam_flash_gpio_power_on(power_info, soc_info);
		if (rc) {
			CAM_ERR(CAM_FLASH, "power up the core is failed:%d",
				rc);
		}
		rc = cam_flash_timer_init(power_info);
		if (rc) {
			CAM_ERR(CAM_FLASH, "flash timer init is failed:%d",
				rc);
		}
		fctrl->is_regulator_enabled = true;
	} else if ((!regulator_enable) && (fctrl->is_regulator_enabled == true)) {
		rc = cam_flash_gpio_power_down(power_info, soc_info);
		if (rc) {
			CAM_ERR(CAM_FLASH, "power down the core is failed:%d",
				rc);
			return rc;
		}
		cam_flash_timer_exit();
		fctrl->is_regulator_enabled = false;
	}
	return rc;
}

int cam_flash_gpio_pkt_parser(struct cam_flash_ctrl *fctrl, void *arg){
	int rc = 0, i = 0;
	uintptr_t generic_ptr, cmd_buf_ptr;
	uint32_t *cmd_buf =  NULL;
	uint32_t *offset = NULL;
	uint32_t frm_offset = 0;
	size_t len_of_buffer;
	size_t remain_len;
	struct cam_control *ioctl_ctrl = NULL;
	struct cam_packet *csl_packet = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct common_header *cmn_hdr;
	struct cam_config_dev_cmd config;
	struct cam_req_mgr_add_request add_req = {0};
	struct cam_flash_init *cam_flash_info = NULL;
	struct cam_flash_set_rer *flash_rer_info = NULL;
	struct cam_flash_set_on_off *flash_operation_info = NULL;
	struct cam_flash_query_curr *flash_query_info = NULL;
	struct cam_flash_frame_setting *flash_data = NULL;
	struct cam_flash_private_soc *soc_private = NULL;
	CAM_DBG(CAM_FLASH, "cam_flash_gpio");
	if (!fctrl || !arg) {
		CAM_ERR(CAM_FLASH, "fctrl/arg is NULL");
		return -EINVAL;
	}

	soc_private = (struct cam_flash_private_soc *)
		fctrl->soc_info.soc_private;

	/* getting CSL Packet */
	ioctl_ctrl = (struct cam_control *)arg;

	if (copy_from_user((&config),
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(config))) {
		CAM_ERR(CAM_FLASH, "Copy cmd handle from user failed");
		rc = -EFAULT;
		return rc;
	}

	rc = cam_mem_get_cpu_buf(config.packet_handle,
		&generic_ptr, &len_of_buffer);
	if (rc) {
		CAM_ERR(CAM_FLASH, "Failed in getting the packet: %d", rc);
		return rc;
	}

	remain_len = len_of_buffer;
	if ((sizeof(struct cam_packet) > len_of_buffer) ||
		((size_t)config.offset >= len_of_buffer -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_FLASH,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), len_of_buffer);
		rc = -EINVAL;
		return rc;
	}

	remain_len -= (size_t)config.offset;
	/* Add offset to the flash csl header */
	csl_packet = (struct cam_packet *)(generic_ptr + config.offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_FLASH, "Invalid packet params");
		rc = -EINVAL;
		return rc;
	}

	if ((csl_packet->header.op_code & 0xFFFFFF) !=
		CAM_FLASH_PACKET_OPCODE_INIT &&
		csl_packet->header.request_id <= fctrl->last_flush_req
		&& fctrl->last_flush_req != 0) {
		CAM_WARN(CAM_FLASH,
			"reject request %lld, last request to flush %d",
			csl_packet->header.request_id, fctrl->last_flush_req);
		rc = -EINVAL;
		return rc;
	}

	if (csl_packet->header.request_id > fctrl->last_flush_req)
		fctrl->last_flush_req = 0;

	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_FLASH_PACKET_OPCODE_INIT: {
		/* INIT packet*/
		offset = (uint32_t *)((uint8_t *)&csl_packet->payload +
			csl_packet->cmd_buf_offset);
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
			&cmd_buf_ptr, &len_of_buffer);
		if (rc) {
			CAM_ERR(CAM_FLASH, "Fail in get buffer: %d", rc);
			return rc;
		}
		if ((len_of_buffer < sizeof(struct cam_flash_init)) ||
			(cmd_desc->offset >
			(len_of_buffer - sizeof(struct cam_flash_init)))) {
			CAM_ERR(CAM_FLASH, "Not enough buffer");
			rc = -EINVAL;
			return rc;
		}
		remain_len = len_of_buffer - cmd_desc->offset;
		cmd_buf = (uint32_t *)((uint8_t *)cmd_buf_ptr +
			cmd_desc->offset);
		cam_flash_info = (struct cam_flash_init *)cmd_buf;

		switch (cam_flash_info->cmd_type) {
		case CAMERA_SENSOR_FLASH_CMD_TYPE_INIT_INFO: {
			CAM_DBG(CAM_FLASH, "INIT_INFO CMD CALLED");
			fctrl->flash_init_setting.cmn_attr.request_id = 0;
			fctrl->flash_init_setting.cmn_attr.is_settings_valid =
				true;
			fctrl->flash_type = cam_flash_info->flash_type;
			fctrl->is_regulator_enabled = false;
			fctrl->nrt_info.cmn_attr.cmd_type =
				CAMERA_SENSOR_FLASH_CMD_TYPE_INIT_INFO;

			rc = fctrl->func_tbl.power_ops(fctrl, true);
			if (rc) {
				CAM_ERR(CAM_FLASH,
					"Enable Regulator Failed rc = %d", rc);
				return rc;
			}

			fctrl->flash_state =
				CAM_FLASH_STATE_CONFIG;
			break;
		}
		case CAMERA_SENSOR_FLASH_CMD_TYPE_INIT_FIRE: {
			CAM_DBG(CAM_FLASH, "INIT_FIRE Operation");

			if (remain_len < sizeof(struct cam_flash_set_on_off)) {
				CAM_ERR(CAM_FLASH, "Not enough buffer");
				rc = -EINVAL;
				return rc;
			}

			flash_operation_info =
				(struct cam_flash_set_on_off *) cmd_buf;
			if (!flash_operation_info) {
				CAM_ERR(CAM_FLASH,
					"flash_operation_info Null");
				rc = -EINVAL;
				return rc;
			}
			if (flash_operation_info->count >
				CAM_FLASH_MAX_LED_TRIGGERS) {
				CAM_ERR(CAM_FLASH, "led count out of limit");
				rc = -EINVAL;
				return rc;
			}
			fctrl->nrt_info.cmn_attr.count =
				flash_operation_info->count;
			fctrl->nrt_info.cmn_attr.request_id = 0;
			fctrl->nrt_info.opcode =
				flash_operation_info->opcode;
			fctrl->nrt_info.cmn_attr.cmd_type =
				CAMERA_SENSOR_FLASH_CMD_TYPE_INIT_FIRE;
			for (i = 0;
				i < flash_operation_info->count; i++)
				fctrl->nrt_info.led_current_ma[i] =
				flash_operation_info->led_current_ma[i];

			rc = fctrl->func_tbl.apply_setting(fctrl, 0);
			if (rc)
				CAM_ERR(CAM_FLASH,
					"Apply setting failed: %d",
					rc);

			fctrl->flash_state = CAM_FLASH_STATE_CONFIG;
			break;
		}
		default:
			CAM_ERR(CAM_FLASH, "Wrong cmd_type = %d",
				cam_flash_info->cmd_type);
			rc = -EINVAL;
			return rc;
		}
		break;
	}
	case CAM_FLASH_PACKET_OPCODE_SET_OPS: {
		offset = (uint32_t *)((uint8_t *)&csl_packet->payload +
			csl_packet->cmd_buf_offset);
		frm_offset = csl_packet->header.request_id %
			MAX_PER_FRAME_ARRAY;
		flash_data = &fctrl->per_frame[frm_offset];

		if (flash_data->cmn_attr.is_settings_valid == true) {
			flash_data->cmn_attr.request_id = 0;
			flash_data->cmn_attr.is_settings_valid = false;
			for (i = 0; i < flash_data->cmn_attr.count; i++)
				flash_data->led_current_ma[i] = 0;
		}

		flash_data->cmn_attr.request_id = csl_packet->header.request_id;
		flash_data->cmn_attr.is_settings_valid = true;
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
			&cmd_buf_ptr, &len_of_buffer);
		if (rc) {
			CAM_ERR(CAM_FLASH, "Fail in get buffer: 0x%x",
				cmd_desc->mem_handle);
			return rc;
		}

		if ((len_of_buffer < sizeof(struct common_header)) ||
			(cmd_desc->offset >
			(len_of_buffer - sizeof(struct common_header)))) {
			CAM_ERR(CAM_FLASH, "not enough buffer");
			rc = -EINVAL;
			return rc;
		}
		remain_len = len_of_buffer - cmd_desc->offset;

		cmd_buf = (uint32_t *)((uint8_t *)cmd_buf_ptr +
			cmd_desc->offset);
		if (!cmd_buf) {
			rc = -EINVAL;
			return rc;
		}
		cmn_hdr = (struct common_header *)cmd_buf;

		switch (cmn_hdr->cmd_type) {
		case CAMERA_SENSOR_FLASH_CMD_TYPE_FIRE: {
			CAM_DBG(CAM_FLASH,
				"CAMERA_SENSOR_FLASH_CMD_TYPE_FIRE cmd called");
			if ((fctrl->flash_state == CAM_FLASH_STATE_INIT) ||
				(fctrl->flash_state ==
					CAM_FLASH_STATE_ACQUIRE)) {
				CAM_WARN(CAM_FLASH,
					"Rxed Flash fire ops without linking");
				flash_data->cmn_attr.is_settings_valid = false;
				return -EINVAL;
			}
			if (remain_len < sizeof(struct cam_flash_set_on_off)) {
				CAM_ERR(CAM_FLASH, "Not enough buffer");
				rc = -EINVAL;
				return rc;
			}

			flash_operation_info =
				(struct cam_flash_set_on_off *) cmd_buf;
			if (!flash_operation_info) {
				CAM_ERR(CAM_FLASH,
					"flash_operation_info Null");
				rc = -EINVAL;
				return rc;
			}
			if (flash_operation_info->count >
				CAM_FLASH_MAX_LED_TRIGGERS) {
				CAM_ERR(CAM_FLASH, "led count out of limit");
				rc = -EINVAL;
				return rc;
			}

			flash_data->opcode = flash_operation_info->opcode;
			flash_data->cmn_attr.count =
				flash_operation_info->count;
			for (i = 0; i < flash_operation_info->count; i++)
				flash_data->led_current_ma[i]
				= flash_operation_info->led_current_ma[i];

			if (flash_data->opcode == CAMERA_SENSOR_FLASH_OP_OFF)
				add_req.skip_before_applying |= SKIP_NEXT_FRAME;
		}
		break;
		default:
			CAM_ERR(CAM_FLASH, "Wrong cmd_type = %d",
				cmn_hdr->cmd_type);
			rc = -EINVAL;
			return rc;
		}
		break;
	}
	case CAM_FLASH_PACKET_OPCODE_NON_REALTIME_SET_OPS: {
		offset = (uint32_t *)((uint8_t *)&csl_packet->payload +
			csl_packet->cmd_buf_offset);
		fctrl->nrt_info.cmn_attr.is_settings_valid = true;
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
			&cmd_buf_ptr, &len_of_buffer);
		if (rc) {
			CAM_ERR(CAM_FLASH, "Fail in get buffer: %d", rc);
			return rc;
		}

		if ((len_of_buffer < sizeof(struct common_header)) ||
			(cmd_desc->offset >
			(len_of_buffer - sizeof(struct common_header)))) {
			CAM_ERR(CAM_FLASH, "Not enough buffer");
			rc = -EINVAL;
			return rc;
		}
		remain_len = len_of_buffer - cmd_desc->offset;
		cmd_buf = (uint32_t *)((uint8_t *)cmd_buf_ptr +
			cmd_desc->offset);
		cmn_hdr = (struct common_header *)cmd_buf;

		switch (cmn_hdr->cmd_type) {
		case CAMERA_SENSOR_FLASH_CMD_TYPE_WIDGET: {
			CAM_DBG(CAM_FLASH, "Widget Flash Operation");
			if (remain_len < sizeof(struct cam_flash_set_on_off)) {
				CAM_ERR(CAM_FLASH, "Not enough buffer");
				rc = -EINVAL;
				return rc;
			}
			flash_operation_info =
				(struct cam_flash_set_on_off *) cmd_buf;
			if (!flash_operation_info) {
				CAM_ERR(CAM_FLASH,
					"flash_operation_info Null");
				rc = -EINVAL;
				return rc;
			}
			if (flash_operation_info->count >
				CAM_FLASH_MAX_LED_TRIGGERS) {
				CAM_ERR(CAM_FLASH, "led count out of limit");
				rc = -EINVAL;
				return rc;
			}

			fctrl->nrt_info.cmn_attr.count =
				flash_operation_info->count;
			fctrl->nrt_info.cmn_attr.request_id = 0;
			fctrl->nrt_info.opcode =
				flash_operation_info->opcode;
			fctrl->nrt_info.cmn_attr.cmd_type =
				CAMERA_SENSOR_FLASH_CMD_TYPE_WIDGET;

			for (i = 0; i < flash_operation_info->count; i++)
				fctrl->nrt_info.led_current_ma[i] =
					flash_operation_info->led_current_ma[i];

			rc = fctrl->func_tbl.apply_setting(fctrl, 0);
			if (rc)
				CAM_ERR(CAM_FLASH, "Apply setting failed: %d",
					rc);
			return rc;
		}
		case CAMERA_SENSOR_FLASH_CMD_TYPE_QUERYCURR: {
			int query_curr_ma = 0;

			if (remain_len < sizeof(struct cam_flash_query_curr)) {
				CAM_ERR(CAM_FLASH, "Not enough buffer");
				rc = -EINVAL;
				return rc;
			}
			flash_query_info =
				(struct cam_flash_query_curr *)cmd_buf;

			if (soc_private->is_wled_flash)
				rc = wled_flash_led_prepare(
					fctrl->switch_trigger,
					QUERY_MAX_AVAIL_CURRENT,
					&query_curr_ma);
			else
				rc = qpnp_flash_led_prepare(
					fctrl->switch_trigger,
					QUERY_MAX_AVAIL_CURRENT,
					&query_curr_ma);

			CAM_DBG(CAM_FLASH, "query_curr_ma = %d",
				query_curr_ma);
			if (rc) {
				CAM_ERR(CAM_FLASH,
				"Query current failed with rc=%d", rc);
				return rc;
			}
			flash_query_info->query_current_ma = query_curr_ma;
			break;
		}
		case CAMERA_SENSOR_FLASH_CMD_TYPE_RER: {
			rc = 0;
			if (remain_len < sizeof(struct cam_flash_set_rer)) {
				CAM_ERR(CAM_FLASH, "Not enough buffer");
				rc = -EINVAL;
				return rc;
			}
			flash_rer_info = (struct cam_flash_set_rer *)cmd_buf;
			if (!flash_rer_info) {
				CAM_ERR(CAM_FLASH,
					"flash_rer_info Null");
				rc = -EINVAL;
				return rc;
			}
			if (flash_rer_info->count >
				CAM_FLASH_MAX_LED_TRIGGERS) {
				CAM_ERR(CAM_FLASH, "led count out of limit");
				rc = -EINVAL;
				return rc;
			}

			fctrl->nrt_info.cmn_attr.cmd_type =
				CAMERA_SENSOR_FLASH_CMD_TYPE_RER;
			fctrl->nrt_info.opcode = flash_rer_info->opcode;
			fctrl->nrt_info.cmn_attr.count = flash_rer_info->count;
			fctrl->nrt_info.cmn_attr.request_id = 0;
			fctrl->nrt_info.num_iterations =
				flash_rer_info->num_iteration;
			fctrl->nrt_info.led_on_delay_ms =
				flash_rer_info->led_on_delay_ms;
			fctrl->nrt_info.led_off_delay_ms =
				flash_rer_info->led_off_delay_ms;

			for (i = 0; i < flash_rer_info->count; i++)
				fctrl->nrt_info.led_current_ma[i] =
					flash_rer_info->led_current_ma[i];

			rc = fctrl->func_tbl.apply_setting(fctrl, 0);
			if (rc)
				CAM_ERR(CAM_FLASH, "apply_setting failed: %d",
					rc);
			return rc;
		}
		default:
			CAM_ERR(CAM_FLASH, "Wrong cmd_type : %d",
				cmn_hdr->cmd_type);
			rc = -EINVAL;
			return rc;
		}

		break;
	}
	case CAM_PKT_NOP_OPCODE: {
		frm_offset = csl_packet->header.request_id %
			MAX_PER_FRAME_ARRAY;
		if ((fctrl->flash_state == CAM_FLASH_STATE_INIT) ||
			(fctrl->flash_state == CAM_FLASH_STATE_ACQUIRE)) {
			CAM_WARN(CAM_FLASH,
				"Rxed NOP packets without linking");
			fctrl->per_frame[frm_offset].cmn_attr.is_settings_valid
				= false;
			return -EINVAL;
		}

		fctrl->per_frame[frm_offset].cmn_attr.is_settings_valid = false;
		fctrl->per_frame[frm_offset].cmn_attr.request_id = 0;
		fctrl->per_frame[frm_offset].opcode = CAM_PKT_NOP_OPCODE;
		CAM_DBG(CAM_FLASH, "NOP Packet is Received: req_id: %llu",
			csl_packet->header.request_id);
		break;
	}
	default:
		CAM_ERR(CAM_FLASH, "Wrong Opcode : %d",
			(csl_packet->header.op_code & 0xFFFFFF));
		rc = -EINVAL;
		return rc;
	}

	if (((csl_packet->header.op_code  & 0xFFFFF) ==
		CAM_PKT_NOP_OPCODE) ||
		((csl_packet->header.op_code & 0xFFFFF) ==
		CAM_FLASH_PACKET_OPCODE_SET_OPS)) {
		add_req.link_hdl = fctrl->bridge_intf.link_hdl;
		add_req.req_id = csl_packet->header.request_id;
		add_req.dev_hdl = fctrl->bridge_intf.device_hdl;

		if ((csl_packet->header.op_code & 0xFFFFF) ==
			CAM_FLASH_PACKET_OPCODE_SET_OPS)
			add_req.skip_before_applying |= 1;
		else
			add_req.skip_before_applying = 0;

		if (fctrl->bridge_intf.crm_cb &&
			fctrl->bridge_intf.crm_cb->add_req)
			fctrl->bridge_intf.crm_cb->add_req(&add_req);
		CAM_DBG(CAM_FLASH, "add req to req_mgr= %lld", add_req.req_id);
	}

	return rc;
}

int cam_flash_gpio_delete_req(struct cam_flash_ctrl *fctrl,
	uint64_t req_id)
{
	int i = 0;
	struct cam_flash_frame_setting *flash_data = NULL;
	uint64_t top = 0, del_req_id = 0;
	int frame_offset = 0;

	if (req_id != 0) {
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			flash_data = &fctrl->per_frame[i];
			if (req_id >= flash_data->cmn_attr.request_id &&
				flash_data->cmn_attr.is_settings_valid
				== 1) {
				if (top < flash_data->cmn_attr.request_id) {
					del_req_id = top;
					top = flash_data->cmn_attr.request_id;
				} else if (top >
					flash_data->cmn_attr.request_id &&
					del_req_id <
					flash_data->cmn_attr.request_id) {
					del_req_id =
						flash_data->cmn_attr.request_id;
				}
			}
		}

		if (top < req_id) {
			if ((((top % MAX_PER_FRAME_ARRAY) - (req_id %
				MAX_PER_FRAME_ARRAY)) >= BATCH_SIZE_MAX) ||
				(((top % MAX_PER_FRAME_ARRAY) - (req_id %
				MAX_PER_FRAME_ARRAY)) <= -BATCH_SIZE_MAX))
				del_req_id = req_id;
		}

		if (!del_req_id)
			return 0;

		CAM_DBG(CAM_FLASH, "top: %llu, del_req_id:%llu",
			top, del_req_id);
	}

	/* delete the request */
	frame_offset = del_req_id % MAX_PER_FRAME_ARRAY;
	flash_data = &fctrl->per_frame[frame_offset];
	flash_data->cmn_attr.request_id = 0;
	flash_data->cmn_attr.is_settings_valid = false;
	flash_data->cmn_attr.count = 0;

	for (i = 0; i < CAM_FLASH_MAX_LED_TRIGGERS; i++)
		flash_data->led_current_ma[i] = 0;

	return 0;
}

int cam_flash_gpio_apply_setting(struct cam_flash_ctrl *fctrl,
		uint64_t req_id)
{
		int rc = 0, i = 0;
		int frame_offset = 0;
		uint16_t num_iterations;
		struct cam_flash_frame_setting *flash_data = NULL;
		CAM_DBG(CAM_FLASH, "cam_flash_gpio req_id = %d",req_id);
		if (req_id == 0) {
			if (fctrl->nrt_info.cmn_attr.cmd_type ==
				CAMERA_SENSOR_FLASH_CMD_TYPE_INIT_FIRE) {
				flash_data = &fctrl->nrt_info;
				CAM_DBG(CAM_REQ,
					"FLASH_INIT_FIRE req_id: %u flash_opcode: %d",
					req_id, flash_data->opcode);

				if (flash_data->opcode ==
					CAMERA_SENSOR_FLASH_OP_FIREHIGH) {
					if (fctrl->flash_state ==
						CAM_FLASH_STATE_START) {
						CAM_WARN(CAM_FLASH,
						"Wrong state :Prev state: %d",
						fctrl->flash_state);
						return -EINVAL;
					}

					rc = cam_flash_gpio_high(fctrl, flash_data);
					if (rc)
						CAM_ERR(CAM_FLASH,
							"FLASH ON failed : %d", rc);
				}
				if (flash_data->opcode ==
					CAMERA_SENSOR_FLASH_OP_FIRELOW) {
					if (fctrl->flash_state ==
						CAM_FLASH_STATE_START) {
						CAM_WARN(CAM_FLASH,
						"Wrong state :Prev state: %d",
						fctrl->flash_state);
						return -EINVAL;
					}

					rc = cam_flash_gpio_low(fctrl, flash_data);
					if (rc)
						CAM_ERR(CAM_FLASH,
							"TORCH ON failed : %d", rc);
				}
				if (flash_data->opcode ==
					CAMERA_SENSOR_FLASH_OP_OFF) {
					rc = cam_flash_off(fctrl);
					if (rc) {
						CAM_ERR(CAM_FLASH,
						"LED OFF FAILED: %d",
						rc);
						return rc;
					}
				}
			} else if (fctrl->nrt_info.cmn_attr.cmd_type ==
				CAMERA_SENSOR_FLASH_CMD_TYPE_WIDGET) {
				flash_data = &fctrl->nrt_info;
				CAM_DBG(CAM_REQ,
					"FLASH_WIDGET req_id: %u flash_opcode: %d",
					req_id, flash_data->opcode);

				if (flash_data->opcode ==
					CAMERA_SENSOR_FLASH_OP_FIRELOW) {
					rc = cam_flash_gpio_low(fctrl, flash_data);
					if (rc) {
						CAM_ERR(CAM_FLASH,
							"Torch ON failed : %d",
							rc);
						goto nrt_del_req;
					}
				} else if (flash_data->opcode ==
					CAMERA_SENSOR_FLASH_OP_OFF) {
					rc = cam_flash_off(fctrl);
					if (rc)
						CAM_ERR(CAM_FLASH,
						"LED off failed: %d",
						rc);
				}
			} else if (fctrl->nrt_info.cmn_attr.cmd_type ==
				CAMERA_SENSOR_FLASH_CMD_TYPE_RER) {
				flash_data = &fctrl->nrt_info;
				if (fctrl->flash_state != CAM_FLASH_STATE_START) {
					rc = cam_flash_off(fctrl);
					if (rc) {
						CAM_ERR(CAM_FLASH,
							"Flash off failed: %d",
							rc);
						goto nrt_del_req;
					}
				}
				CAM_DBG(CAM_REQ, "FLASH_RER req_id: %u", req_id);

				num_iterations = flash_data->num_iterations;
				for (i = 0; i < num_iterations; i++) {
					/* Turn On Torch */
					if (fctrl->flash_state ==
						CAM_FLASH_STATE_START) {
						rc = cam_flash_gpio_low(fctrl, flash_data);
						if (rc) {
							CAM_ERR(CAM_FLASH,
								"Fire Torch Failed");
							goto nrt_del_req;
						}

						usleep_range(
						flash_data->led_on_delay_ms * 1000,
						flash_data->led_on_delay_ms * 1000 +
							100);
					}
					/* Turn Off Torch */
					rc = cam_flash_off(fctrl);
					if (rc) {
						CAM_ERR(CAM_FLASH,
							"Flash off failed: %d", rc);
						continue;
					}
					fctrl->flash_state = CAM_FLASH_STATE_START;
					usleep_range(
					flash_data->led_off_delay_ms * 1000,
					flash_data->led_off_delay_ms * 1000 + 100);
				}
			}
		} else {
			frame_offset = req_id % MAX_PER_FRAME_ARRAY;
			flash_data = &fctrl->per_frame[frame_offset];
			CAM_DBG(CAM_REQ, "FLASH_RT req_id: %u flash_opcode: %d",
				req_id, flash_data->opcode);

			if ((flash_data->opcode == CAMERA_SENSOR_FLASH_OP_FIREHIGH) &&
				(flash_data->cmn_attr.is_settings_valid) &&
				(flash_data->cmn_attr.request_id == req_id)) {
				/* Turn On Flash */
				if (fctrl->flash_state == CAM_FLASH_STATE_START) {
					rc = cam_flash_gpio_high(fctrl, flash_data);
					if (rc) {
						CAM_ERR(CAM_FLASH,
							"Flash ON failed: rc= %d",
							rc);
						goto apply_setting_err;
					}
				}
			} else if ((flash_data->opcode ==
				CAMERA_SENSOR_FLASH_OP_FIRELOW) &&
				(flash_data->cmn_attr.is_settings_valid) &&
				(flash_data->cmn_attr.request_id == req_id)) {
				/* Turn On Torch */
				if (fctrl->flash_state == CAM_FLASH_STATE_START) {
					rc = cam_flash_gpio_low(fctrl, flash_data);
					if (rc) {
						CAM_ERR(CAM_FLASH,
							"Torch ON failed: rc= %d",
							rc);
						goto apply_setting_err;
					}
				}
			} else if ((flash_data->opcode == CAMERA_SENSOR_FLASH_OP_OFF) &&
				(flash_data->cmn_attr.is_settings_valid) &&
				(flash_data->cmn_attr.request_id == req_id)) {
				rc = cam_flash_off(fctrl);
				if (rc) {
					CAM_ERR(CAM_FLASH,
						"Flash off failed %d", rc);
					goto apply_setting_err;
				}
			} else if (flash_data->opcode == CAM_PKT_NOP_OPCODE) {
				CAM_DBG(CAM_FLASH, "NOP Packet");
			} else {
				rc = -EINVAL;
				CAM_ERR(CAM_FLASH, "Invalid opcode: %d req_id: %llu",
					flash_data->opcode, req_id);
				goto apply_setting_err;
			}
		}

	nrt_del_req:
		cam_flash_gpio_delete_req(fctrl, req_id);
	apply_setting_err:
		return rc;
}

