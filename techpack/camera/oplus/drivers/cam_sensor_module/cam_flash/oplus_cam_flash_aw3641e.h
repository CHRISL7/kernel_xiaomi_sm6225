#ifndef OPLUS_CAM_FLASH_AW3641E_H
#define OPLUS_CAM_FLASH_AW3641E_H

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/timer.h>

#include "cam_sensor_cmn_header.h"
#include "../../../../drivers/cam_sensor_module/cam_flash/cam_flash_core.h"
#include "cam_res_mgr_api.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"

struct board_id_soc_info {
	int hw_id1_gpio;
	int hw_id2_gpio;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_hw1_high;
	struct pinctrl_state *pin_hw1_low;
	struct pinctrl_state *pin_hw2_high;
	struct pinctrl_state *pin_hw2_low;

};

struct cam_flash_timer {
	struct cam_sensor_power_ctrl_t *power_info;
	struct timer_list flash_timer;
};

int cam_flash_timer_init(struct cam_sensor_power_ctrl_t *power_info);
void cam_flash_timer_callback(struct timer_list *timer_data);
void cam_flash_timer_exit(void);
int cam_flash_pwm_clk_enable(struct cam_flash_ctrl *flash_ctrl, int flash_duty);
void cam_flash_pwm_clk_disable(void);
int cam_flash_pwm_selected(struct cam_flash_ctrl *flash_ctrl, bool is_pwm_selected);
int cam_flash_gpio_get_level(int flash_current);
int cam_get_hw_id1_status(int gpio_name);
int cam_get_hw_id1_status(int gpio_name);
int cam_get_hw_board_id(void);
int cam_flash_gpio_off(struct cam_flash_ctrl *flash_ctrl);
int cam_flash_gpio_high(struct cam_flash_ctrl *flash_ctrl, struct cam_flash_frame_setting *flash_data);
int cam_flash_gpio_low(struct cam_flash_ctrl *flash_ctrl, struct cam_flash_frame_setting *flash_data);
int cam_flash_gpio_flush_request(struct cam_flash_ctrl *fctrl,enum cam_flash_flush_type type, uint64_t req_id);
int cam_flash_gpio_power_on(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info);
int cam_flash_gpio_power_down(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info);
int cam_flash_gpio_power_ops(struct cam_flash_ctrl *fctrl,bool regulator_enable);
int cam_flash_gpio_pkt_parser(struct cam_flash_ctrl *fctrl, void *arg);
int cam_flash_gpio_delete_req(struct cam_flash_ctrl *fctrl,
	uint64_t req_id);
int cam_flash_gpio_apply_setting(struct cam_flash_ctrl *fctrl,
		uint64_t req_id);

#endif //OPLUS_CAM_FLASH_AW3641E_H
