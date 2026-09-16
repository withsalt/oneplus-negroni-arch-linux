// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Linaro Ltd
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/auxiliary_bus.h>
#include <linux/devm-helpers.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/property.h>
#include <linux/soc/qcom/pdr.h>
#include <linux/soc/qcom/pmic_glink.h>
#include <linux/math.h>
#include <linux/units.h>
#include <linux/delay.h>

#define BATTMGR_CHEMISTRY_LEN	4
#define BATTMGR_STRING_LEN	128

enum qcom_battmgr_variant {
	QCOM_BATTMGR_SC8280XP,
	QCOM_BATTMGR_SM8350,
	QCOM_BATTMGR_SM8550,
	QCOM_BATTMGR_X1E80100,
};

#define BATTMGR_BAT_STATUS		0x1

#define BATTMGR_REQUEST_NOTIFICATION	0x4

#define BATTMGR_NOTIFICATION		0x7
#define NOTIF_BAT_PROPERTY		0x30
#define NOTIF_USB_PROPERTY		0x32
#define NOTIF_WLS_PROPERTY		0x34
#define NOTIF_BAT_STATUS		0x80
#define NOTIF_BAT_INFO			0x81
#define NOTIF_BAT_CHARGING_STATE	0x83

#define BATTMGR_BAT_INFO		0x9

#define BATTMGR_BAT_DISCHARGE_TIME	0xc

#define BATTMGR_BAT_CHARGE_TIME		0xd

#define BATTMGR_BAT_PROPERTY_GET	0x30
#define BATTMGR_BAT_PROPERTY_SET	0x31
#define BATT_STATUS			0
#define BATT_HEALTH			1
#define BATT_PRESENT			2
#define BATT_CHG_TYPE			3
#define BATT_CAPACITY			4
#define BATT_SOH			5
#define BATT_VOLT_OCV			6
#define BATT_VOLT_NOW			7
#define BATT_VOLT_MAX			8
#define BATT_CURR_NOW			9
#define BATT_CHG_CTRL_LIM		10
#define BATT_CHG_CTRL_LIM_MAX		11
#define BATT_TEMP			12
#define BATT_TECHNOLOGY			13
#define BATT_CHG_COUNTER		14
#define BATT_CYCLE_COUNT		15
#define BATT_CHG_FULL_DESIGN		16
#define BATT_CHG_FULL			17
#define BATT_MODEL_NAME			18
#define BATT_TTF_AVG			19
#define BATT_TTE_AVG			20
#define BATT_RESISTANCE			21
#define BATT_POWER_NOW			22
#define BATT_POWER_AVG			23
#define BATT_CHG_CTRL_EN		24
#define BATT_CHG_CTRL_START_THR		25
#define BATT_CHG_CTRL_END_THR		26

/*
 * Oplus-specific battery property IDs.
 * The stock Oplus ADSP firmware extends the standard property enum with
 * vendor-specific entries starting at position 24. BATT_ADSP_GAUGE_INIT
 * is at position 38 in the Oplus enum. Sending property_id=38 with value=1
 * via opcode 0x31 (BATTMGR_BAT_PROPERTY_SET) initializes the ADSP fuel gauge
 * (bq27541) and enables SOC reporting.
 */
#define BATT_OPLUS_CHG_EN                      24
#define BATT_OPLUS_SET_PDO                     25
#define BATT_OPLUS_RTC_SOC                     31
#define BATT_OPLUS_SEND_CHG_STATUS             37
#define BATT_OPLUS_ADSP_GAUGE_INIT             38
#define BATT_OPLUS_UPDATE_SOC_SMOOTH_PARAM     39

#define BATTMGR_USB_PROPERTY_GET	0x32
#define BATTMGR_USB_PROPERTY_SET	0x33

enum usb_property_id {
	USB_ONLINE,
	USB_VOLT_NOW,
	USB_VOLT_MAX,
	USB_CURR_NOW,
	USB_CURR_MAX,
	USB_INPUT_CURR_LIMIT,
	USB_TYPE,
	USB_ADAP_TYPE,
	USB_MOISTURE_DET_EN,
	USB_MOISTURE_DET_STS,
	USB_ADAP_SUBTYPE,//sjc add
	USB_VBUS_COLLAPSE_STATUS,
	USB_VOOCPHY_STATUS,
	USB_VOOCPHY_ENABLE,
	USB_OTG_AP_ENABLE,
	USB_OTG_SWITCH,
	USB_POWER_SUPPLY_RELEASE_FIXED_FREQUENCE,
	USB_TYPEC_CC_ORIENTATION,
	USB_CID_STATUS,
	USB_TYPEC_MODE,
	USB_TYPEC_SINKONLY,
	USB_OTG_VBUS_REGULATOR_ENABLE,
	USB_VOOC_CHG_PARAM_INFO,
	USB_VOOC_FAST_CHG_TYPE,
	USB_DEBUG_REG,
	USB_VOOCPHY_RESET_AGAIN,
	USB_SUSPEND_PMIC,
	USB_OEM_MISC_CTL,
	USB_CCDETECT_HAPPENED,
	USB_GET_PPS_TYPE,
	USB_GET_PPS_STATUS,
	USB_SET_PPS_VOLT,
	USB_SET_PPS_CURR,
	USB_GET_PPS_MAX_CURR,
	USB_PPS_READ_VBAT0_VOLT,
	USB_PPS_CHECK_BTB_TEMP,
	USB_PPS_MOS_CTRL,
	USB_PPS_CP_MODE_INIT,
	USB_PPS_CHECK_AUTHENTICATE,
	USB_PPS_GET_AUTHENTICATE,
	USB_PPS_GET_CP_VBUS,
	USB_PPS_GET_CP_MASTER_IBUS,
	USB_PPS_GET_CP_SLAVE_IBUS,
	USB_PPS_MOS_SLAVE_CTRL,
	USB_PPS_GET_R_COOL_DOWN,
	USB_PPS_GET_DISCONNECT_STATUS,
	USB_PPS_VOOCPHY_ENABLE,
	USB_IN_STATUS,
	USB_GET_BATT_CURR,
	/*USB_ADSP_TRACK_DEBUG,*/
	USB_TEMP,
	USB_REAL_TYPE,
	USB_TYPEC_COMPLIANT,
	USB_PPS_FORCE_SVOOC,
	USB_SCOPE,
	USB_CONNECTOR_TYPE,
	F_ACTIVE,
	USB_PROP_MAX,
};

enum oplus_power_supply_usb_type {
	POWER_SUPPLY_USB_TYPE_PD_SDP = 17,
};

/*
 * Oplus-specific USB property IDs.
 * USB_VOOCPHY_ENABLE (property 13) enables the ADSP VoocPHY subsystem
 * which manages both VOOC fast charging AND the fuel gauge. The stock
 * driver enables this after gauge init during crash recovery and during
 * normal charger setup.
 */
#define USB_OPLUS_VOOCPHY_STATUS       12
#define USB_OPLUS_VOOCPHY_ENABLE       13


#define BATTMGR_WLS_PROPERTY_GET	0x34
#define BATTMGR_WLS_PROPERTY_SET	0x35
#define WLS_ONLINE			0
#define WLS_VOLT_NOW			1
#define WLS_VOLT_MAX			2
#define WLS_CURR_NOW			3
#define WLS_CURR_MAX			4
#define WLS_TYPE			5
#define WLS_BOOST_EN			6

#define BATTMGR_CHG_CTRL_LIMIT_EN	0x48
#define CHARGE_CTRL_START_THR_MIN	50
#define CHARGE_CTRL_START_THR_MAX	95
#define CHARGE_CTRL_END_THR_MIN		55
#define CHARGE_CTRL_END_THR_MAX		100
#define CHARGE_CTRL_DELTA_SOC		5

/* Oplus OEM read buffer opcode and layout */
#define OEM_OPCODE_READ_BUFFER         0x10000
#define OEM_READ_BUF_MAX               128
/* OEM buffer indices (from Oplus ADSP firmware) */
#define OEM_BUF_TEMP                   0
#define OEM_BUF_CURRENT                        1
#define OEM_BUF_VOLTAGE                        2
#define OEM_BUF_SOC                    3
#define OEM_BUF_REMAINING_CAP          4
#define OEM_BUF_CYCLE_COUNT            5
#define OEM_BUF_FCC                    6
#define OEM_BUF_CHARGE_COUNTER         7
#define OEM_BUF_SOH                    8

/* Oplus-specific notification IDs */
#define NOTIF_TYPEC_STATE_CHANGE       0x55
#define NOTIF_PLUGIN_IRQ               0x57
#define NOTIF_CHG_STATUS_GET           0x59  /* BC_CHG_STATUS_GET: ADSP asks AP if charging OK */
#define NOTIF_CHG_STATUS_SET           0x60
#define NOTIF_ADSP_SUSPEND_CHG         0x61  /* BC_ADSP_NOTIFY_AP_SUSPEND_CHG */
#define NOTIF_CP_MOS_DISABLE           0x64

/*
 * Oplus ADSP VOOC notification codes (from stock oplus_battery_sm8450.h).
 * These are sent as BATTMGR_NOTIFICATION opcodes from the ADSP firmware.
 */
#define NOTIF_VOOC_STATUS_GET          0x48  /* BC_VOOC_STATUS_GET */
#define BC_OTG_ENABLE		       0x50
#define BC_OTG_DISABLE		       0x51
#define NOTIF_VOOC_VBUS_ADC_ENABLE     0x52  /* BC_VOOC_VBUS_ADC_ENABLE */
#define NOTIF_PD_SVOOC                 0x56  /* BC_PD_SVOOC */
#define NOTIF_VOOC_CHG_PUMP_0          0x62  /* Charge pump event */
#define NOTIF_VOOC_CHG_PUMP_1          0x63  /* Charge pump MOS event */

/*
 * VOOC fast charge status values (from stock oplus_adsp_voocphy.c).
 * Delivered as (intval & 0xFF) when reading USB_OPLUS_VOOCPHY_STATUS.
 */
enum adsp_voocphy_fast_status {
        ADSP_VPHY_FAST_NOTIFY_UNKNOW            = 0,
        ADSP_VPHY_FAST_NOTIFY_PRESENT           = 1,
        ADSP_VPHY_FAST_NOTIFY_ONGOING           = 2,
        ADSP_VPHY_FAST_NOTIFY_ABSENT            = 3,
        ADSP_VPHY_FAST_NOTIFY_FULL              = 4,
        ADSP_VPHY_FAST_NOTIFY_BAD_CONNECTED     = 5,
        ADSP_VPHY_FAST_NOTIFY_BATT_TEMP_OVER    = 6,
        ADSP_VPHY_FAST_NOTIFY_BTB_TEMP_OVER     = 7,
        ADSP_VPHY_FAST_NOTIFY_DUMMY_START       = 8,
        ADSP_VPHY_FAST_NOTIFY_ADAPTER_COPYCAT   = 9,
        ADSP_VPHY_FAST_NOTIFY_ERR_COMMU         = 10,
        ADSP_VPHY_FAST_NOTIFY_SWITCH_TEMP_RANGE = 11,
        ADSP_VPHY_FAST_NOTIFY_COMMU_TIME_OUT    = 12,
        ADSP_VPHY_FAST_NOTIFY_COMMU_CLK_ERR     = 13,
        ADSP_VPHY_FAST_NOTIFY_COMMU_SEND_ERR    = 14,
        ADSP_VPHY_FAST_NOTIFY_HW_VBATT_HIGH     = 15,
        ADSP_VPHY_FAST_NOTIFY_HW_TBATT_HIGH     = 16,
	ADSP_VPHY_FAST_NOTIFY_CRASH		= 17,
};


struct qcom_battmgr_enable_request {
	struct pmic_glink_hdr hdr;
	__le32 battery_id;
	__le32 power_state;
	__le32 low_capacity;
	__le32 high_capacity;
};

struct qcom_battmgr_property_request {
	struct pmic_glink_hdr hdr;
	__le32 battery;
	__le32 property;
	__le32 value;
};

struct qcom_battmgr_update_request {
	struct pmic_glink_hdr hdr;
	__le32 battery_id;
};

struct qcom_battmgr_charge_time_request {
	struct pmic_glink_hdr hdr;
	__le32 battery_id;
	__le32 percent;
	__le32 reserved;
};

struct qcom_battmgr_discharge_time_request {
	struct pmic_glink_hdr hdr;
	__le32 battery_id;
	__le32 rate; /* 0 for current rate */
	__le32 reserved;
};

struct qcom_battmgr_charge_ctrl_request {
	struct pmic_glink_hdr hdr;
	__le32 enable;
	__le32 target_soc;
	__le32 delta_soc;
};

struct qcom_battmgr_message {
	struct pmic_glink_hdr hdr;
	union {
		struct {
			__le32 property;
			__le32 value;
			__le32 result;
		} intval;
		struct {
			__le32 property;
			char model[BATTMGR_STRING_LEN];
		} strval;
		struct {
			/*
			 * 0: mWh
			 * 1: mAh
			 */
			__le32 power_unit;
			__le32 design_capacity;
			__le32 last_full_capacity;
			/*
			 * 0 nonrechargable
			 * 1 rechargable
			 */
			__le32 battery_tech;
			__le32 design_voltage; /* mV */
			__le32 capacity_low;
			__le32 capacity_warning;
			__le32 cycle_count;
			/* thousandth of percent */
			__le32 accuracy;
			__le32 max_sample_time_ms;
			__le32 min_sample_time_ms;
			__le32 max_average_interval_ms;
			__le32 min_average_interval_ms;
			/* granularity between low and warning */
			__le32 capacity_granularity1;
			/* granularity between warning and full */
			__le32 capacity_granularity2;
			/*
			 * 0: no
			 * 1: cold
			 * 2: hot
			 */
			__le32 swappable;
			__le32 capabilities;
			char model_number[BATTMGR_STRING_LEN];
			char serial_number[BATTMGR_STRING_LEN];
			char battery_type[BATTMGR_STRING_LEN];
			char oem_info[BATTMGR_STRING_LEN];
			char battery_chemistry[BATTMGR_CHEMISTRY_LEN];
			char uid[BATTMGR_STRING_LEN];
			__le32 critical_bias;
			u8 day;
			u8 month;
			__le16 year;
			__le32 battery_id;
		} info;
		struct {
			/*
			 * BIT(0) discharging
			 * BIT(1) charging
			 * BIT(2) critical low
			 */
			__le32 battery_state;
			/* mWh or mAh, based on info->power_unit */
			__le32 capacity;
			__le32 rate;
			/* mv */
			__le32 battery_voltage;
			/*
			 * BIT(0) power online
			 * BIT(1) discharging
			 * BIT(2) charging
			 * BIT(3) battery critical
			 */
			__le32 power_state;
			/*
			 * 1: AC
			 * 2: USB
			 * 3: Wireless
			 */
			__le32 charging_source;
			__le32 temperature;
		} status;
		__le32 time;
		__le32 notification;
	};
};

struct qcom_battmgr_oem_read_req {
        struct pmic_glink_hdr hdr;
        __le32 data_size;
};

struct qcom_battmgr_oem_read_resp {
        struct pmic_glink_hdr hdr;
        __le32 data_buffer[OEM_READ_BUF_MAX];
        __le32 data_size;
};


#define BATTMGR_CHARGING_SOURCE_AC	1
#define BATTMGR_CHARGING_SOURCE_USB	2
#define BATTMGR_CHARGING_SOURCE_WIRELESS 3

enum qcom_battmgr_unit {
	QCOM_BATTMGR_UNIT_mWh = 0,
	QCOM_BATTMGR_UNIT_mAh = 1
};

struct qcom_battmgr_info {
	bool valid;

	bool present;
	unsigned int charge_type;
	unsigned int design_capacity;
	unsigned int last_full_capacity;
	unsigned int voltage_max_design;
	unsigned int voltage_max;
	unsigned int capacity_low;
	unsigned int capacity_warning;
	unsigned int cycle_count;
	unsigned int charge_count;
	unsigned int charge_ctrl_start;
	unsigned int charge_ctrl_end;
	char model_number[BATTMGR_STRING_LEN];
	char serial_number[BATTMGR_STRING_LEN];
	char oem_info[BATTMGR_STRING_LEN];
	unsigned char technology;
	unsigned char day;
	unsigned char month;
	unsigned short year;
};

struct qcom_battmgr_status {
	unsigned int status;
	unsigned int health;
	unsigned int capacity;
	unsigned int percent;
	int current_now;
	int power_now;
	unsigned int voltage_now;
	unsigned int voltage_ocv;
	unsigned int temperature;
	unsigned int resistance;
	unsigned int soh_percent;

	unsigned int discharge_time;
	unsigned int charge_time;
};

struct qcom_battmgr_ac {
	bool online;
};

struct qcom_battmgr_usb {
	bool online;
	unsigned int voltage_now;
	unsigned int voltage_max;
	unsigned int current_now;
	unsigned int current_max;
	unsigned int current_limit;
	unsigned int usb_type;
	unsigned int adap_type;
        unsigned int voocphy_status_raw;
};

struct qcom_battmgr_wireless {
	bool online;
	unsigned int voltage_now;
	unsigned int voltage_max;
	unsigned int current_now;
	unsigned int current_max;
};

struct qcom_battmgr {
	struct device *dev;
	struct pmic_glink_client *client;

	enum qcom_battmgr_variant variant;
	bool invert_current;

	struct power_supply *ac_psy;
	struct power_supply *bat_psy;
	struct power_supply *usb_psy;
	struct power_supply *wls_psy;

	enum qcom_battmgr_unit unit;

	int error;
	struct completion ack;

	bool service_up;

	struct qcom_battmgr_info info;
	struct qcom_battmgr_status status;
	struct qcom_battmgr_ac ac;
	struct qcom_battmgr_usb usb;
	struct qcom_battmgr_wireless wireless;

	struct work_struct enable_work;

        /* Oplus OEM read buffer for fuel gauge data */
        bool oem_read_buf_valid;
        bool oem_read_buf_supported;
        bool oem_read_buf_tried;
        u32 oem_read_buf[OEM_READ_BUF_MAX];
        struct completion oem_read_ack;
        struct mutex oem_read_lock;

        /* Oplus ADSP fuel gauge init tracking */
        bool gauge_init_done;

        /* Periodic VoocPHY re-enable (matches stock 5s polling) */
        struct delayed_work voocphy_recheck_work;

        /* VOOC status read worker (triggered by 0x48 notification) */
        struct work_struct voocphy_status_work;

        /* Set once ICL/FCC have been configured for DCP (avoid repeated sets) */
        bool vooc_limits_set;
        int last_configured_adap_type;  /* adapter type we last configured, -1 = none */
        unsigned int usb_offline_count; /* consecutive USB_ONLINE=0 readings */

        /* ADSP told AP to suspend charging (notification 0x61) */
        bool adsp_suspended_chg;

	bool otg_enabled;

	struct gpio_desc *otg_boost_en_gpio;
	struct gpio_desc *otg_ovp_en_gpio;

	struct pinctrl		*otg_boost_en_pinctrl;
	struct pinctrl_state	*otg_boost_en_active;
	struct pinctrl_state	*otg_boost_en_sleep;
	struct pinctrl		*otg_ovp_en_pinctrl;
	struct pinctrl_state	*otg_ovp_en_active;
	struct pinctrl_state	*otg_ovp_en_sleep;

	struct notifier_block	ssr_nb;
	void			*subsys_handle;

        /* Worker to reply to ADSP charging status queries (notification 0x59) */
        struct work_struct chg_status_reply_work;

	struct delayed_work	otg_init_work;


	/*
	 * @lock is used to prevent concurrent power supply requests to the
	 * firmware, as it then stops responding.
	 */
	struct mutex lock;
};

static int qcom_battmgr_request(struct qcom_battmgr *battmgr, void *data, size_t len)
{
	unsigned long left;
	int ret;

	reinit_completion(&battmgr->ack);

	battmgr->error = 0;

	ret = pmic_glink_send(battmgr->client, data, len);
	if (ret < 0)
		return ret;

	left = wait_for_completion_timeout(&battmgr->ack, HZ);
	if (!left)
		return -ETIMEDOUT;

	return battmgr->error;
}

static int qcom_battmgr_request_property(struct qcom_battmgr *battmgr, int opcode,
					 int property, u32 value)
{
	struct qcom_battmgr_property_request request = {
		.hdr.owner = cpu_to_le32(PMIC_GLINK_OWNER_BATTMGR),
		.hdr.type = cpu_to_le32(PMIC_GLINK_REQ_RESP),
		.hdr.opcode = cpu_to_le32(opcode),
		.battery = cpu_to_le32(0),
		.property = cpu_to_le32(property),
		.value = cpu_to_le32(value),
	};

	return qcom_battmgr_request(battmgr, &request, sizeof(request));
}

static int qcom_battmgr_update_status(struct qcom_battmgr *battmgr)
{
	struct qcom_battmgr_update_request request = {
		.hdr.owner = cpu_to_le32(PMIC_GLINK_OWNER_BATTMGR),
		.hdr.type = cpu_to_le32(PMIC_GLINK_REQ_RESP),
		.hdr.opcode = cpu_to_le32(BATTMGR_BAT_STATUS),
		.battery_id = cpu_to_le32(0),
	};

	return qcom_battmgr_request(battmgr, &request, sizeof(request));
}

static int qcom_battmgr_update_info(struct qcom_battmgr *battmgr)
{
	struct qcom_battmgr_update_request request = {
		.hdr.owner = cpu_to_le32(PMIC_GLINK_OWNER_BATTMGR),
		.hdr.type = cpu_to_le32(PMIC_GLINK_REQ_RESP),
		.hdr.opcode = cpu_to_le32(BATTMGR_BAT_INFO),
		.battery_id = cpu_to_le32(0),
	};

	return qcom_battmgr_request(battmgr, &request, sizeof(request));
}

static int qcom_battmgr_update_charge_time(struct qcom_battmgr *battmgr)
{
	struct qcom_battmgr_charge_time_request request = {
		.hdr.owner = cpu_to_le32(PMIC_GLINK_OWNER_BATTMGR),
		.hdr.type = cpu_to_le32(PMIC_GLINK_REQ_RESP),
		.hdr.opcode = cpu_to_le32(BATTMGR_BAT_CHARGE_TIME),
		.battery_id = cpu_to_le32(0),
		.percent = cpu_to_le32(100),
	};

	return qcom_battmgr_request(battmgr, &request, sizeof(request));
}

static int qcom_battmgr_update_discharge_time(struct qcom_battmgr *battmgr)
{
	struct qcom_battmgr_discharge_time_request request = {
		.hdr.owner = cpu_to_le32(PMIC_GLINK_OWNER_BATTMGR),
		.hdr.type = cpu_to_le32(PMIC_GLINK_REQ_RESP),
		.hdr.opcode = cpu_to_le32(BATTMGR_BAT_DISCHARGE_TIME),
		.battery_id = cpu_to_le32(0),
		.rate = cpu_to_le32(0),
	};

	return qcom_battmgr_request(battmgr, &request, sizeof(request));
}

static const u8 sm8350_bat_prop_map[] = {
	[POWER_SUPPLY_PROP_STATUS] = BATT_STATUS,
	[POWER_SUPPLY_PROP_HEALTH] = BATT_HEALTH,
	[POWER_SUPPLY_PROP_PRESENT] = BATT_PRESENT,
	[POWER_SUPPLY_PROP_CHARGE_TYPE] = BATT_CHG_TYPE,
	[POWER_SUPPLY_PROP_CAPACITY] = BATT_CAPACITY,
	[POWER_SUPPLY_PROP_VOLTAGE_OCV] = BATT_VOLT_OCV,
	[POWER_SUPPLY_PROP_VOLTAGE_NOW] = BATT_VOLT_NOW,
	[POWER_SUPPLY_PROP_VOLTAGE_MAX] = BATT_VOLT_MAX,
	[POWER_SUPPLY_PROP_CURRENT_NOW] = BATT_CURR_NOW,
	[POWER_SUPPLY_PROP_TEMP] = BATT_TEMP,
	[POWER_SUPPLY_PROP_TECHNOLOGY] = BATT_TECHNOLOGY,
	[POWER_SUPPLY_PROP_CHARGE_COUNTER] =  BATT_CHG_COUNTER,
	[POWER_SUPPLY_PROP_CYCLE_COUNT] = BATT_CYCLE_COUNT,
	[POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN] =  BATT_CHG_FULL_DESIGN,
	[POWER_SUPPLY_PROP_CHARGE_FULL] = BATT_CHG_FULL,
	[POWER_SUPPLY_PROP_MODEL_NAME] = BATT_MODEL_NAME,
	[POWER_SUPPLY_PROP_TIME_TO_FULL_AVG] = BATT_TTF_AVG,
	[POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG] = BATT_TTE_AVG,
	[POWER_SUPPLY_PROP_INTERNAL_RESISTANCE] = BATT_RESISTANCE,
	[POWER_SUPPLY_PROP_STATE_OF_HEALTH] = BATT_SOH,
	[POWER_SUPPLY_PROP_POWER_NOW] = BATT_POWER_NOW,
	[POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD] = BATT_CHG_CTRL_START_THR,
	[POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD] = BATT_CHG_CTRL_END_THR,
};

static int qcom_battmgr_oem_read_buffer(struct qcom_battmgr *battmgr);

static void qcom_battmgr_apply_oem_data(struct qcom_battmgr *battmgr)
{
        if (!battmgr->oem_read_buf_valid)
                return;

        battmgr->status.percent = battmgr->oem_read_buf[OEM_BUF_SOC] / 100;
        battmgr->status.temperature = battmgr->oem_read_buf[OEM_BUF_TEMP];
        battmgr->status.current_now = (int)battmgr->oem_read_buf[OEM_BUF_CURRENT];
        battmgr->status.voltage_now = battmgr->oem_read_buf[OEM_BUF_VOLTAGE];
        battmgr->info.cycle_count = battmgr->oem_read_buf[OEM_BUF_CYCLE_COUNT];
        battmgr->info.last_full_capacity = battmgr->oem_read_buf[OEM_BUF_FCC];
        battmgr->info.charge_count = battmgr->oem_read_buf[OEM_BUF_CHARGE_COUNTER];
        battmgr->status.soh_percent = battmgr->oem_read_buf[OEM_BUF_SOH];
}


static int qcom_battmgr_bat_sm8350_update(struct qcom_battmgr *battmgr,
					  enum power_supply_property psp)
{
	unsigned int prop;
	int ret;

	if (psp >= ARRAY_SIZE(sm8350_bat_prop_map))
		return -EINVAL;

	prop = sm8350_bat_prop_map[psp];

	mutex_lock(&battmgr->lock);
	ret = qcom_battmgr_request_property(battmgr, BATTMGR_BAT_PROPERTY_GET, prop, 0);
	mutex_unlock(&battmgr->lock);

        /* Try OEM read buffer and override with real fuel gauge data */
        if (!qcom_battmgr_oem_read_buffer(battmgr))
                qcom_battmgr_apply_oem_data(battmgr);


	return ret;
}

static int qcom_battmgr_bat_sc8280xp_update(struct qcom_battmgr *battmgr,
					    enum power_supply_property psp)
{
	int ret;

	mutex_lock(&battmgr->lock);

	if (!battmgr->info.valid) {
		ret = qcom_battmgr_update_info(battmgr);
		if (ret < 0)
			goto out_unlock;
		battmgr->info.valid = true;
	}

	ret = qcom_battmgr_update_status(battmgr);
	if (ret < 0)
		goto out_unlock;

	if (psp == POWER_SUPPLY_PROP_TIME_TO_FULL_AVG) {
		ret = qcom_battmgr_update_charge_time(battmgr);
		if (ret < 0) {
			ret = -ENODATA;
			goto out_unlock;
		}
	}

	if (psp == POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG) {
		ret = qcom_battmgr_update_discharge_time(battmgr);
		if (ret < 0) {
			ret = -ENODATA;
			goto out_unlock;
		}
	}

out_unlock:
	mutex_unlock(&battmgr->lock);
	return ret;
}

static int qcom_battmgr_bat_get_property(struct power_supply *psy,
					 enum power_supply_property psp,
					 union power_supply_propval *val)
{
	struct qcom_battmgr *battmgr = power_supply_get_drvdata(psy);
	enum qcom_battmgr_unit unit = battmgr->unit;
	int ret;

	if (!battmgr->service_up)
		return -EAGAIN;

	if (battmgr->variant == QCOM_BATTMGR_SC8280XP ||
	    battmgr->variant == QCOM_BATTMGR_X1E80100)
		ret = qcom_battmgr_bat_sc8280xp_update(battmgr, psp);
	else
		ret = qcom_battmgr_bat_sm8350_update(battmgr, psp);
	if (ret < 0)
		return ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
                /*
                 * Query USB_ONLINE fresh — it's only updated when explicitly
                 * read, and upower reads STATUS immediately on plug events
                 * before the periodic recheck worker can refresh it.
                 */
                mutex_lock(&battmgr->lock);
                qcom_battmgr_request_property(battmgr,
                                BATTMGR_USB_PROPERTY_GET, USB_ONLINE, 0);
                mutex_unlock(&battmgr->lock);

		val->intval = battmgr->status.status;
                /*
                 * The Oplus ADSP doesn't reliably update BATT_STATUS on
                 * charger removal — it can stay stuck at "Charging".
                 * Override based on actual charger presence.
                 */
                if (!battmgr->usb.online && !battmgr->wireless.online &&
                    val->intval == POWER_SUPPLY_STATUS_CHARGING)
                        val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
                /*
                 * Conversely, the ADSP often reports "Discharging" even
                 * when a charger is connected and current is flowing in.
                 * Override based on actual charger presence.
                 */
                if ((battmgr->usb.online || battmgr->wireless.online) &&
                    val->intval == POWER_SUPPLY_STATUS_DISCHARGING)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = battmgr->info.charge_type;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = battmgr->status.health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = battmgr->info.present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = battmgr->info.technology;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = battmgr->info.cycle_count;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = battmgr->info.voltage_max_design;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = battmgr->info.voltage_max;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battmgr->status.voltage_now;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = battmgr->status.voltage_ocv;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = battmgr->status.current_now;
		/* Negroni firmware uses positive current for discharge. */
		if (battmgr->invert_current)
			val->intval = -val->intval;
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		val->intval = battmgr->status.power_now;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		if (unit != QCOM_BATTMGR_UNIT_mAh)
			return -ENODATA;
		val->intval = battmgr->info.design_capacity;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (unit != QCOM_BATTMGR_UNIT_mAh)
			return -ENODATA;
		val->intval = battmgr->info.last_full_capacity;
		break;
	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
		if (unit != QCOM_BATTMGR_UNIT_mAh)
			return -ENODATA;
		val->intval = battmgr->info.capacity_low;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		if (unit != QCOM_BATTMGR_UNIT_mAh)
			return -ENODATA;
		val->intval = battmgr->status.capacity;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = battmgr->info.charge_count;
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		if (unit != QCOM_BATTMGR_UNIT_mWh)
			return -ENODATA;
		val->intval = battmgr->info.design_capacity;
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		if (unit != QCOM_BATTMGR_UNIT_mWh)
			return -ENODATA;
		val->intval = battmgr->info.last_full_capacity;
		break;
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		if (unit != QCOM_BATTMGR_UNIT_mWh)
			return -ENODATA;
		val->intval = battmgr->info.capacity_low;
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		if (unit != QCOM_BATTMGR_UNIT_mWh)
			return -ENODATA;
		val->intval = battmgr->status.capacity;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (battmgr->status.percent == (unsigned int)-1)
			return -ENODATA;
		val->intval = battmgr->status.percent;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = battmgr->status.temperature;
		break;
	case POWER_SUPPLY_PROP_INTERNAL_RESISTANCE:
		val->intval = battmgr->status.resistance;
		break;
	case POWER_SUPPLY_PROP_STATE_OF_HEALTH:
		val->intval = battmgr->status.soh_percent;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		val->intval = battmgr->status.discharge_time;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		val->intval = battmgr->status.charge_time;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD:
		val->intval = battmgr->info.charge_ctrl_start;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD:
		val->intval = battmgr->info.charge_ctrl_end;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURE_YEAR:
		val->intval = battmgr->info.year;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURE_MONTH:
		val->intval = battmgr->info.month;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURE_DAY:
		val->intval = battmgr->info.day;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = battmgr->info.model_number;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = battmgr->info.oem_info;
		break;
	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		val->strval = battmgr->info.serial_number;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int qcom_battmgr_set_charge_control(struct qcom_battmgr *battmgr,
					   u32 target_soc, u32 delta_soc)
{
	struct qcom_battmgr_charge_ctrl_request request = {
		.hdr.owner = cpu_to_le32(PMIC_GLINK_OWNER_BATTMGR),
		.hdr.type = cpu_to_le32(PMIC_GLINK_REQ_RESP),
		.hdr.opcode = cpu_to_le32(BATTMGR_CHG_CTRL_LIMIT_EN),
		.enable = cpu_to_le32(1),
		.target_soc = cpu_to_le32(target_soc),
		.delta_soc = cpu_to_le32(delta_soc),
	};

	return qcom_battmgr_request(battmgr, &request, sizeof(request));
}

static int qcom_battmgr_set_charge_start_threshold(struct qcom_battmgr *battmgr, int start_soc)
{
	u32 target_soc, delta_soc;
	int ret;

	start_soc = clamp(start_soc, CHARGE_CTRL_START_THR_MIN, CHARGE_CTRL_START_THR_MAX);

	/*
	 * If the new start threshold is larger than the old end threshold,
	 * move the end threshold one step (DELTA_SOC) after the new start
	 * threshold.
	 */
	if (start_soc > battmgr->info.charge_ctrl_end) {
		target_soc = start_soc + CHARGE_CTRL_DELTA_SOC;
		target_soc = min_t(u32, target_soc, CHARGE_CTRL_END_THR_MAX);
		delta_soc = target_soc - start_soc;
		delta_soc = min_t(u32, delta_soc, CHARGE_CTRL_DELTA_SOC);
	} else {
		target_soc =  battmgr->info.charge_ctrl_end;
		delta_soc = battmgr->info.charge_ctrl_end - start_soc;
	}

	mutex_lock(&battmgr->lock);
	ret = qcom_battmgr_set_charge_control(battmgr, target_soc, delta_soc);
	mutex_unlock(&battmgr->lock);
	if (!ret) {
		battmgr->info.charge_ctrl_start = start_soc;
		battmgr->info.charge_ctrl_end = target_soc;
	}

	return 0;
}

static int qcom_battmgr_set_charge_end_threshold(struct qcom_battmgr *battmgr, int end_soc)
{
	u32 delta_soc = CHARGE_CTRL_DELTA_SOC;
	int ret;

	end_soc = clamp(end_soc, CHARGE_CTRL_END_THR_MIN, CHARGE_CTRL_END_THR_MAX);

	if (battmgr->info.charge_ctrl_start && end_soc > battmgr->info.charge_ctrl_start)
		delta_soc = end_soc - battmgr->info.charge_ctrl_start;

	mutex_lock(&battmgr->lock);
	ret = qcom_battmgr_set_charge_control(battmgr, end_soc, delta_soc);
	mutex_unlock(&battmgr->lock);
	if (!ret) {
		battmgr->info.charge_ctrl_start = end_soc - delta_soc;
		battmgr->info.charge_ctrl_end = end_soc;
	}

	return 0;
}

static int qcom_battmgr_charge_control_thresholds_init(struct qcom_battmgr *battmgr)
{
	int ret;
	u8 en, end_soc, start_soc, delta_soc;

	ret = nvmem_cell_read_u8(battmgr->dev->parent, "charge_limit_en", &en);
	if (!ret && en != 0) {
		ret = nvmem_cell_read_u8(battmgr->dev->parent, "charge_limit_end", &end_soc);
		if (ret < 0)
			return ret;

		ret = nvmem_cell_read_u8(battmgr->dev->parent, "charge_limit_delta", &delta_soc);
		if (ret < 0)
			return ret;

		if (delta_soc >= end_soc)
			return -EINVAL;

		start_soc = end_soc - delta_soc;
		end_soc = clamp(end_soc, CHARGE_CTRL_END_THR_MIN, CHARGE_CTRL_END_THR_MAX);
		start_soc = clamp(start_soc, CHARGE_CTRL_START_THR_MIN, CHARGE_CTRL_START_THR_MAX);

		battmgr->info.charge_ctrl_start = start_soc;
		battmgr->info.charge_ctrl_end = end_soc;
	}

	return 0;
}

static int qcom_battmgr_bat_is_writeable(struct power_supply *psy,
					 enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD:
		return 1;
	default:
		return 0;
	}

	return 0;
}

static int qcom_battmgr_bat_set_property(struct power_supply *psy,
					 enum power_supply_property psp,
					 const union power_supply_propval *pval)
{
	struct qcom_battmgr *battmgr = power_supply_get_drvdata(psy);

	if (!battmgr->service_up)
		return -EAGAIN;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD:
		return qcom_battmgr_set_charge_start_threshold(battmgr, pval->intval);
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD:
		return qcom_battmgr_set_charge_end_threshold(battmgr, pval->intval);
	default:
		return -EINVAL;
	}

	return 0;
}

static const enum power_supply_property sc8280xp_bat_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_EMPTY,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_MANUFACTURE_YEAR,
	POWER_SUPPLY_PROP_MANUFACTURE_MONTH,
	POWER_SUPPLY_PROP_MANUFACTURE_DAY,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
};

static const struct power_supply_desc sc8280xp_bat_psy_desc = {
	.name = "qcom-battmgr-bat",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = sc8280xp_bat_props,
	.num_properties = ARRAY_SIZE(sc8280xp_bat_props),
	.get_property = qcom_battmgr_bat_get_property,
};

static const enum power_supply_property x1e80100_bat_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_EMPTY,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_MANUFACTURE_YEAR,
	POWER_SUPPLY_PROP_MANUFACTURE_MONTH,
	POWER_SUPPLY_PROP_MANUFACTURE_DAY,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD,
};

static const struct power_supply_desc x1e80100_bat_psy_desc = {
	.name = "qcom-battmgr-bat",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = x1e80100_bat_props,
	.num_properties = ARRAY_SIZE(x1e80100_bat_props),
	.get_property = qcom_battmgr_bat_get_property,
	.set_property = qcom_battmgr_bat_set_property,
	.property_is_writeable = qcom_battmgr_bat_is_writeable,
};

static const enum power_supply_property sm8350_bat_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_INTERNAL_RESISTANCE,
	POWER_SUPPLY_PROP_STATE_OF_HEALTH,
	POWER_SUPPLY_PROP_POWER_NOW,
};

static const struct power_supply_desc sm8350_bat_psy_desc = {
	.name = "qcom-battmgr-bat",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = sm8350_bat_props,
	.num_properties = ARRAY_SIZE(sm8350_bat_props),
	.get_property = qcom_battmgr_bat_get_property,
};

static const enum power_supply_property sm8550_bat_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_INTERNAL_RESISTANCE,
	POWER_SUPPLY_PROP_STATE_OF_HEALTH,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD,
};

static const struct power_supply_desc sm8550_bat_psy_desc = {
	.name = "qcom-battmgr-bat",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = sm8550_bat_props,
	.num_properties = ARRAY_SIZE(sm8550_bat_props),
	.get_property = qcom_battmgr_bat_get_property,
	.set_property = qcom_battmgr_bat_set_property,
	.property_is_writeable = qcom_battmgr_bat_is_writeable,
};

static int qcom_battmgr_ac_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct qcom_battmgr *battmgr = power_supply_get_drvdata(psy);
	int ret;

	if (!battmgr->service_up)
		return -EAGAIN;

	ret = qcom_battmgr_bat_sc8280xp_update(battmgr, psp);
	if (ret)
		return ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = battmgr->ac.online;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const enum power_supply_property sc8280xp_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct power_supply_desc sc8280xp_ac_psy_desc = {
	.name = "qcom-battmgr-ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = sc8280xp_ac_props,
	.num_properties = ARRAY_SIZE(sc8280xp_ac_props),
	.get_property = qcom_battmgr_ac_get_property,
};

static const u8 sm8350_usb_prop_map[] = {
	[POWER_SUPPLY_PROP_ONLINE] = USB_ONLINE,
	[POWER_SUPPLY_PROP_VOLTAGE_NOW] = USB_VOLT_NOW,
	[POWER_SUPPLY_PROP_VOLTAGE_MAX] = USB_VOLT_MAX,
	[POWER_SUPPLY_PROP_CURRENT_NOW] = USB_CURR_NOW,
	[POWER_SUPPLY_PROP_CURRENT_MAX] = USB_CURR_MAX,
	[POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT] = USB_INPUT_CURR_LIMIT,
	[POWER_SUPPLY_PROP_USB_TYPE] = USB_TYPE,
};

static int qcom_battmgr_usb_sm8350_update(struct qcom_battmgr *battmgr,
					  enum power_supply_property psp)
{
	unsigned int prop;
	int ret;

	if (psp >= ARRAY_SIZE(sm8350_usb_prop_map))
		return -EINVAL;

	prop = sm8350_usb_prop_map[psp];

        /*
         * For USB type, query USB_ADAP_TYPE (property 7) instead of USB_TYPE
         * (property 6). The Oplus ADSP firmware reports the real negotiated
         * adapter type (PD, DCP, PD_PPS, etc.) via USB_ADAP_TYPE, while
         * USB_TYPE always returns SDP (the BC1.2 fallback). Without this,
         * the device charges at SDP rates (~500mA) instead of full speed.
         */
        if (psp == POWER_SUPPLY_PROP_USB_TYPE)
                prop = USB_ADAP_TYPE;


	mutex_lock(&battmgr->lock);
	ret = qcom_battmgr_request_property(battmgr, BATTMGR_USB_PROPERTY_GET, prop, 0);
	mutex_unlock(&battmgr->lock);

	return ret;
}

static int qcom_battmgr_usb_get_property(struct power_supply *psy,
					 enum power_supply_property psp,
					 union power_supply_propval *val)
{
	struct qcom_battmgr *battmgr = power_supply_get_drvdata(psy);
	int ret;

	if (!battmgr->service_up)
		return -EAGAIN;

	if (battmgr->variant == QCOM_BATTMGR_SC8280XP ||
	    battmgr->variant == QCOM_BATTMGR_X1E80100)
		ret = qcom_battmgr_bat_sc8280xp_update(battmgr, psp);
	else
		ret = qcom_battmgr_usb_sm8350_update(battmgr, psp);
	if (ret)
		return ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = battmgr->usb.online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battmgr->usb.voltage_now;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = battmgr->usb.voltage_max;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = battmgr->usb.current_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = battmgr->usb.current_max;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = battmgr->usb.current_limit;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = battmgr->usb.usb_type;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const enum power_supply_property sc8280xp_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct power_supply_desc sc8280xp_usb_psy_desc = {
	.name = "qcom-battmgr-usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = sc8280xp_usb_props,
	.num_properties = ARRAY_SIZE(sc8280xp_usb_props),
	.get_property = qcom_battmgr_usb_get_property,
	.usb_types = BIT(POWER_SUPPLY_USB_TYPE_UNKNOWN) |
		     BIT(POWER_SUPPLY_USB_TYPE_SDP)     |
		     BIT(POWER_SUPPLY_USB_TYPE_DCP)     |
		     BIT(POWER_SUPPLY_USB_TYPE_CDP)     |
		     BIT(POWER_SUPPLY_USB_TYPE_ACA)     |
		     BIT(POWER_SUPPLY_USB_TYPE_C)       |
		     BIT(POWER_SUPPLY_USB_TYPE_PD)      |
		     BIT(POWER_SUPPLY_USB_TYPE_PD_DRP)  |
		     BIT(POWER_SUPPLY_USB_TYPE_PD_PPS)  |
		     BIT(POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID),
};

static const enum power_supply_property sm8350_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_USB_TYPE,
};

static const struct power_supply_desc sm8350_usb_psy_desc = {
	.name = "qcom-battmgr-usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = sm8350_usb_props,
	.num_properties = ARRAY_SIZE(sm8350_usb_props),
	.get_property = qcom_battmgr_usb_get_property,
	.usb_types = BIT(POWER_SUPPLY_USB_TYPE_UNKNOWN) |
		     BIT(POWER_SUPPLY_USB_TYPE_SDP)     |
		     BIT(POWER_SUPPLY_USB_TYPE_DCP)     |
		     BIT(POWER_SUPPLY_USB_TYPE_CDP)     |
		     BIT(POWER_SUPPLY_USB_TYPE_ACA)     |
		     BIT(POWER_SUPPLY_USB_TYPE_C)       |
		     BIT(POWER_SUPPLY_USB_TYPE_PD)      |
		     BIT(POWER_SUPPLY_USB_TYPE_PD_DRP)  |
		     BIT(POWER_SUPPLY_USB_TYPE_PD_PPS)  |
		     BIT(POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID),
};

static const u8 sm8350_wls_prop_map[] = {
	[POWER_SUPPLY_PROP_ONLINE] = WLS_ONLINE,
	[POWER_SUPPLY_PROP_VOLTAGE_NOW] = WLS_VOLT_NOW,
	[POWER_SUPPLY_PROP_VOLTAGE_MAX] = WLS_VOLT_MAX,
	[POWER_SUPPLY_PROP_CURRENT_NOW] = WLS_CURR_NOW,
	[POWER_SUPPLY_PROP_CURRENT_MAX] = WLS_CURR_MAX,
};

static int qcom_battmgr_wls_sm8350_update(struct qcom_battmgr *battmgr,
					  enum power_supply_property psp)
{
	unsigned int prop;
	int ret;

	if (psp >= ARRAY_SIZE(sm8350_wls_prop_map))
		return -EINVAL;

	prop = sm8350_wls_prop_map[psp];

	mutex_lock(&battmgr->lock);
	ret = qcom_battmgr_request_property(battmgr, BATTMGR_WLS_PROPERTY_GET, prop, 0);
	mutex_unlock(&battmgr->lock);

	return ret;
}

static int qcom_battmgr_wls_get_property(struct power_supply *psy,
					 enum power_supply_property psp,
					 union power_supply_propval *val)
{
	struct qcom_battmgr *battmgr = power_supply_get_drvdata(psy);
	int ret;

	if (!battmgr->service_up)
		return -EAGAIN;

	if (battmgr->variant == QCOM_BATTMGR_SC8280XP ||
	    battmgr->variant == QCOM_BATTMGR_X1E80100)
		ret = qcom_battmgr_bat_sc8280xp_update(battmgr, psp);
	else
		ret = qcom_battmgr_wls_sm8350_update(battmgr, psp);
	if (ret < 0)
		return ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = battmgr->wireless.online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battmgr->wireless.voltage_now;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = battmgr->wireless.voltage_max;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = battmgr->wireless.current_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = battmgr->wireless.current_max;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const enum power_supply_property sc8280xp_wls_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct power_supply_desc sc8280xp_wls_psy_desc = {
	.name = "qcom-battmgr-wls",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = sc8280xp_wls_props,
	.num_properties = ARRAY_SIZE(sc8280xp_wls_props),
	.get_property = qcom_battmgr_wls_get_property,
};

static const enum power_supply_property sm8350_wls_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

static const struct power_supply_desc sm8350_wls_psy_desc = {
	.name = "qcom-battmgr-wls",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = sm8350_wls_props,
	.num_properties = ARRAY_SIZE(sm8350_wls_props),
	.get_property = qcom_battmgr_wls_get_property,
};

static int qcom_battmgr_oem_read_buffer(struct qcom_battmgr *battmgr)
{
        struct qcom_battmgr_oem_read_req req = {
                .hdr.owner = cpu_to_le32(PMIC_GLINK_OWNER_BATTMGR),
                .hdr.type = cpu_to_le32(PMIC_GLINK_REQ_RESP),
                .hdr.opcode = cpu_to_le32(OEM_OPCODE_READ_BUFFER),
                .data_size = cpu_to_le32(sizeof(battmgr->oem_read_buf)),
        };
        unsigned long left;
        int ret;

        /* Don't retry if we already know firmware doesn't support this */
        if (battmgr->oem_read_buf_tried && !battmgr->oem_read_buf_supported)
                return -EOPNOTSUPP;

        mutex_lock(&battmgr->oem_read_lock);
        reinit_completion(&battmgr->oem_read_ack);

        ret = pmic_glink_send(battmgr->client, &req, sizeof(req));
        if (ret < 0) {
                mutex_unlock(&battmgr->oem_read_lock);
                battmgr->oem_read_buf_tried = true;
                return ret;
        }

        left = wait_for_completion_timeout(&battmgr->oem_read_ack, msecs_to_jiffies(500));
        mutex_unlock(&battmgr->oem_read_lock);

        battmgr->oem_read_buf_tried = true;
        if (!left) {
                dev_info(battmgr->dev, "oem read buffer timed out, firmware may not support it\n");
                return -ETIMEDOUT;
        }

        battmgr->oem_read_buf_supported = true;
        dev_dbg(battmgr->dev, "oem read buffer supported by firmware\n");
        return 0;
}

static void qcom_battmgr_oem_read_response(struct qcom_battmgr *battmgr,
                                            const void *data, size_t len)
{
        const struct qcom_battmgr_oem_read_resp *resp = data;
        u32 buf_len;

        if (len > sizeof(*resp)) {
                dev_warn(battmgr->dev, "oem read: incorrect length %zu\n", len);
                return;
        }

        buf_len = le32_to_cpu(resp->data_size);
        if (buf_len == 0 || buf_len > sizeof(battmgr->oem_read_buf)) {
                dev_warn(battmgr->dev, "oem read: invalid buffer length %u\n", buf_len);
                return;
        }

        memcpy(battmgr->oem_read_buf, resp->data_buffer, buf_len);
        battmgr->oem_read_buf_valid = true;

        dev_dbg(battmgr->dev, "oem read: data_size=%u bytes (%u u32s)\n",
                 buf_len, buf_len / 4);
        dev_dbg(battmgr->dev,
                "oem buf[0-14]: %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
                battmgr->oem_read_buf[0], battmgr->oem_read_buf[1],
                battmgr->oem_read_buf[2], battmgr->oem_read_buf[3],
                battmgr->oem_read_buf[4], battmgr->oem_read_buf[5],
                battmgr->oem_read_buf[6], battmgr->oem_read_buf[7],
                battmgr->oem_read_buf[8], battmgr->oem_read_buf[9],
                battmgr->oem_read_buf[10], battmgr->oem_read_buf[11],
                battmgr->oem_read_buf[12], battmgr->oem_read_buf[13],
                battmgr->oem_read_buf[14]);
        dev_dbg(battmgr->dev,
                "oem buf[15-29]: %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
                battmgr->oem_read_buf[15], battmgr->oem_read_buf[16],
                battmgr->oem_read_buf[17], battmgr->oem_read_buf[18],
                battmgr->oem_read_buf[19], battmgr->oem_read_buf[20],
                battmgr->oem_read_buf[21], battmgr->oem_read_buf[22],
                battmgr->oem_read_buf[23], battmgr->oem_read_buf[24],
                battmgr->oem_read_buf[25], battmgr->oem_read_buf[26],
                battmgr->oem_read_buf[27], battmgr->oem_read_buf[28],
                battmgr->oem_read_buf[29]);

        complete(&battmgr->oem_read_ack);
}

static int oplus_otg_boost_en_gpio_init(struct qcom_battmgr *battmgr)
{

	battmgr->otg_boost_en_pinctrl = devm_pinctrl_get(battmgr->dev);
	if (IS_ERR_OR_NULL(battmgr->otg_boost_en_pinctrl))
		return -EINVAL;

	battmgr->otg_boost_en_active =
		pinctrl_lookup_state(battmgr->otg_boost_en_pinctrl, "otg_booster_en_active");
	if (IS_ERR_OR_NULL(battmgr->otg_boost_en_active))
		return -EINVAL;
	battmgr->otg_boost_en_sleep =
		pinctrl_lookup_state(battmgr->otg_boost_en_pinctrl, "otg_booster_en_sleep");
	if (IS_ERR_OR_NULL(battmgr->otg_boost_en_sleep))
		return -EINVAL;

	pinctrl_select_state(battmgr->otg_boost_en_pinctrl,
		battmgr->otg_boost_en_sleep);

	battmgr->otg_boost_en_gpio = devm_gpiod_get_optional(battmgr->dev, "qcom,otg-booster-en", GPIOD_OUT_LOW);

	return 0;
}

static int oplus_otg_ovp_en_gpio_init(struct qcom_battmgr *battmgr)
{
	battmgr->otg_ovp_en_pinctrl = devm_pinctrl_get(battmgr->dev);
	if (IS_ERR_OR_NULL(battmgr->otg_ovp_en_pinctrl))
		return -EINVAL;

	battmgr->otg_ovp_en_active =
		pinctrl_lookup_state(battmgr->otg_ovp_en_pinctrl, "otg_ovp_en_active");
	if (IS_ERR_OR_NULL(battmgr->otg_ovp_en_active))
		return -EINVAL;

	battmgr->otg_ovp_en_sleep =
		pinctrl_lookup_state(battmgr->otg_ovp_en_pinctrl, "otg_ovp_en_sleep");
	if (IS_ERR_OR_NULL(battmgr->otg_ovp_en_sleep))
		return -EINVAL;

	pinctrl_select_state(battmgr->otg_ovp_en_pinctrl,
		battmgr->otg_ovp_en_sleep);

	battmgr->otg_ovp_en_gpio = devm_gpiod_get_optional(battmgr->dev, "qcom,otg-ovp-en", GPIOD_OUT_LOW);

	return 0;
}

static void oplus_set_otg_boost_en_val(struct qcom_battmgr *battmgr, int value)
{

	if (battmgr->otg_boost_en_gpio <= 0)
		return;

	if (IS_ERR_OR_NULL(battmgr->otg_boost_en_pinctrl)
		|| IS_ERR_OR_NULL(battmgr->otg_boost_en_active)
		|| IS_ERR_OR_NULL(battmgr->otg_boost_en_sleep))
		return;

	if (value) {
		gpiod_direction_output_raw(battmgr->otg_boost_en_gpio , 1);
		pinctrl_select_state(battmgr->otg_boost_en_pinctrl,
				battmgr->otg_boost_en_active);
	} else {
		gpiod_direction_output_raw(battmgr->otg_boost_en_gpio, 0);
		pinctrl_select_state(battmgr->otg_boost_en_pinctrl,
				battmgr->otg_boost_en_sleep);
	}
}

static void oplus_set_otg_ovp_en_val(struct qcom_battmgr *battmgr, int value)
{

	if (battmgr->otg_ovp_en_gpio <= 0)
		return;

	if (IS_ERR_OR_NULL(battmgr->otg_ovp_en_pinctrl)
		|| IS_ERR_OR_NULL(battmgr->otg_ovp_en_active)
		|| IS_ERR_OR_NULL(battmgr->otg_ovp_en_sleep))
		return;

	if (value) {
		gpiod_direction_output_raw(battmgr->otg_ovp_en_gpio , 1);
		pinctrl_select_state(battmgr->otg_ovp_en_pinctrl,
				battmgr->otg_ovp_en_active);
	} else {
		gpiod_direction_output_raw(battmgr->otg_ovp_en_gpio, 0);
		pinctrl_select_state(battmgr->otg_ovp_en_pinctrl,
				battmgr->otg_ovp_en_sleep);
	}
}


static void qcom_battmgr_notification(struct qcom_battmgr *battmgr,
				      const struct qcom_battmgr_message *msg,
				      int len)
{
	size_t payload_len = len - sizeof(struct pmic_glink_hdr);
	unsigned int notification;

	if (payload_len != sizeof(msg->notification)) {
		dev_warn(battmgr->dev, "ignoring notification with invalid length\n");
		return;
	}

	notification = le32_to_cpu(msg->notification);
	notification &= 0xff;
	switch (notification) {
	case NOTIF_BAT_INFO:
		battmgr->info.valid = false;
		fallthrough;
	case NOTIF_BAT_STATUS:
	case NOTIF_BAT_PROPERTY:
	case NOTIF_BAT_CHARGING_STATE:
		power_supply_changed(battmgr->bat_psy);
		break;
	case NOTIF_USB_PROPERTY:
		power_supply_changed(battmgr->usb_psy);
		break;
	case NOTIF_WLS_PROPERTY:
		power_supply_changed(battmgr->wls_psy);
		break;
        case NOTIF_TYPEC_STATE_CHANGE:
        case NOTIF_PLUGIN_IRQ:
                /*
                 * The ADSP may not have updated USB_ONLINE yet, so the
                 * above notifications may carry stale data.  Schedule
                 * the recheck worker at 500 ms to re-query USB_ONLINE
                 * and emit a corrected notification.
                 *
                 * Don't reset vooc_limits_set here — the ADSP fires
                 * these notifications spuriously, and resetting the
                 * flag causes repeated ICL/FCC/PDO writes that
                 * interfere with PD negotiation.  The recheck worker
                 * resets the flag only when USB_ONLINE actually goes 0.
                 */
                // mod_delayed_work(system_wq, &battmgr->voocphy_recheck_work,
                //                  msecs_to_jiffies(500));
                break;
        case NOTIF_CHG_STATUS_GET:
                /*
                 * ADSP asks: "is charging allowed?"
                 * Must reply with BATT_SEND_CHG_STATUS=1 or ADSP aborts
                 * the VOOC handshake and drops to failsafe 5V/0.5A.
                 */
                schedule_work(&battmgr->chg_status_reply_work);
                break;
        case NOTIF_CHG_STATUS_SET:
        case NOTIF_CP_MOS_DISABLE:
                // power_supply_changed(battmgr->bat_psy);
                break;
        case NOTIF_ADSP_SUSPEND_CHG:
                /* ADSP tells AP to suspend charging */
                dev_info(battmgr->dev, "vooc: ADSP requests charging suspend\n");
                battmgr->adsp_suspended_chg = true;
                schedule_work(&battmgr->chg_status_reply_work);
                break;
        case NOTIF_VOOC_STATUS_GET:
                /*
                 * ADSP signals that VOOC status changed.
                 * Schedule worker to read USB_VOOCPHY_STATUS.
                 */
                schedule_work(&battmgr->voocphy_status_work);
                break;
        case NOTIF_VOOC_VBUS_ADC_ENABLE:
                dev_info(battmgr->dev, "vooc: VBUS ADC enable (handshake starting)\n");
                // power_supply_changed(battmgr->bat_psy);
                break;
        case NOTIF_PD_SVOOC:
                dev_info(battmgr->dev, "vooc: PD-SVOOC adapter detected\n");
                // power_supply_changed(battmgr->usb_psy);
                break;
        case NOTIF_VOOC_CHG_PUMP_0:
        case NOTIF_VOOC_CHG_PUMP_1:
                dev_dbg(battmgr->dev, "vooc: charge pump event %#x\n", notification);
                // power_supply_changed(battmgr->bat_psy);
                break;
	case BC_OTG_ENABLE:
		oplus_set_otg_ovp_en_val(battmgr, 1);
		oplus_set_otg_boost_en_val(battmgr, 1);
		battmgr->otg_enabled = true;
		power_supply_changed(battmgr->usb_psy);
		break;
	case BC_OTG_DISABLE:
		oplus_set_otg_ovp_en_val(battmgr, 0);
		oplus_set_otg_boost_en_val(battmgr, 0);
		battmgr->otg_enabled = false;
		power_supply_changed(battmgr->usb_psy);
		break;
	default:
		dev_err(battmgr->dev, "unknown notification: %#x\n", notification);
		break;
	}
}

static void qcom_battmgr_sc8280xp_strcpy(char *dest, const char *src)
{
	size_t len = src[0];

	/* Some firmware versions return Pascal-style strings */
	if (len < BATTMGR_STRING_LEN && len == strnlen(src + 1, BATTMGR_STRING_LEN - 1)) {
		memcpy(dest, src + 1, len);
		dest[len] = '\0';
	} else {
		strscpy(dest, src, BATTMGR_STRING_LEN);
	}
}

static unsigned int qcom_battmgr_sc8280xp_parse_technology(const char *chemistry)
{
	if ((!strncmp(chemistry, "LIO", 3)) ||
	    (!strncmp(chemistry, "OOI", 3)))
		return POWER_SUPPLY_TECHNOLOGY_LION;
	if (!strncmp(chemistry, "LIP", 3) ||
	    !strncmp(chemistry, "LiP", 3))
		return POWER_SUPPLY_TECHNOLOGY_LIPO;

	pr_err("Unknown battery technology '%s'\n", chemistry);
	return POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
}

static unsigned int qcom_battmgr_sc8280xp_convert_temp(unsigned int temperature)
{
	return DIV_ROUND_CLOSEST(temperature, 10);
}

static void qcom_battmgr_sc8280xp_callback(struct qcom_battmgr *battmgr,
					   const struct qcom_battmgr_message *resp,
					   size_t len)
{
	unsigned int opcode = le32_to_cpu(resp->hdr.opcode);
	unsigned int source;
	unsigned int state;
	size_t payload_len = len - sizeof(struct pmic_glink_hdr);

	if (payload_len < sizeof(__le32)) {
		dev_warn(battmgr->dev, "invalid payload length for %#x: %zd\n",
			 opcode, len);
		return;
	}

	switch (opcode) {
	case BATTMGR_REQUEST_NOTIFICATION:
		battmgr->error = 0;
		break;
	case BATTMGR_BAT_INFO:
		/* some firmware versions report an extra __le32 at the end of the payload */
		if (payload_len != sizeof(resp->info) &&
		    payload_len != (sizeof(resp->info) + sizeof(__le32))) {
			dev_warn(battmgr->dev,
				 "invalid payload length for battery information request: %zd\n",
				 payload_len);
			battmgr->error = -ENODATA;
			return;
		}

		battmgr->unit = le32_to_cpu(resp->info.power_unit);

		battmgr->info.present = true;
		battmgr->info.design_capacity = le32_to_cpu(resp->info.design_capacity) * 1000;
		battmgr->info.last_full_capacity = le32_to_cpu(resp->info.last_full_capacity) * 1000;
		battmgr->info.voltage_max_design = le32_to_cpu(resp->info.design_voltage) * 1000;
		battmgr->info.capacity_low = le32_to_cpu(resp->info.capacity_low) * 1000;
		battmgr->info.cycle_count = le32_to_cpu(resp->info.cycle_count);
		qcom_battmgr_sc8280xp_strcpy(battmgr->info.model_number, resp->info.model_number);
		qcom_battmgr_sc8280xp_strcpy(battmgr->info.serial_number, resp->info.serial_number);
		battmgr->info.technology = qcom_battmgr_sc8280xp_parse_technology(resp->info.battery_chemistry);
		qcom_battmgr_sc8280xp_strcpy(battmgr->info.oem_info, resp->info.oem_info);
		battmgr->info.day = resp->info.day;
		battmgr->info.month = resp->info.month;
		battmgr->info.year = le16_to_cpu(resp->info.year);
		break;
	case BATTMGR_BAT_STATUS:
		if (payload_len != sizeof(resp->status)) {
			dev_warn(battmgr->dev,
				 "invalid payload length for battery status request: %zd\n",
				 payload_len);
			battmgr->error = -ENODATA;
			return;
		}

		state = le32_to_cpu(resp->status.battery_state);
		if (state & BIT(0))
			battmgr->status.status = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (state & BIT(1))
			battmgr->status.status = POWER_SUPPLY_STATUS_CHARGING;
		else
			battmgr->status.status = POWER_SUPPLY_STATUS_NOT_CHARGING;

		battmgr->status.capacity = le32_to_cpu(resp->status.capacity) * 1000;
		battmgr->status.power_now = le32_to_cpu(resp->status.rate) * 1000;
		battmgr->status.voltage_now = le32_to_cpu(resp->status.battery_voltage) * 1000;
		battmgr->status.temperature = qcom_battmgr_sc8280xp_convert_temp(le32_to_cpu(resp->status.temperature));

		source = le32_to_cpu(resp->status.charging_source);
		battmgr->ac.online = source == BATTMGR_CHARGING_SOURCE_AC;
		battmgr->usb.online = source == BATTMGR_CHARGING_SOURCE_USB;
		battmgr->wireless.online = source == BATTMGR_CHARGING_SOURCE_WIRELESS;
		if (battmgr->info.last_full_capacity != 0) {
			/*
			 * 100 * battmgr->status.capacity can overflow a 32bit
			 * unsigned integer. FW readings are in m{W/A}h, which
			 * are multiplied by 1000 converting them to u{W/A}h,
			 * the format the power_supply API expects.
			 * To avoid overflow use the original value for dividend
			 * and convert the divider back to m{W/A}h, which can be
			 * done without any loss of precision.
			 */
			battmgr->status.percent =
				(100 * le32_to_cpu(resp->status.capacity)) /
				(battmgr->info.last_full_capacity / 1000);
		} else {
			/*
			 * Let the sysfs handler know no data is available at
			 * this time.
			 */
			battmgr->status.percent = (unsigned int)-1;
		}
		break;
	case BATTMGR_BAT_DISCHARGE_TIME:
		battmgr->status.discharge_time = le32_to_cpu(resp->time);
		break;
	case BATTMGR_BAT_CHARGE_TIME:
		battmgr->status.charge_time = le32_to_cpu(resp->time);
		break;
	case BATTMGR_CHG_CTRL_LIMIT_EN:
		battmgr->error = 0;
		break;
	default:
		dev_warn(battmgr->dev, "unknown message %#x\n", opcode);
		break;
	}

	complete(&battmgr->ack);
}

static void qcom_battmgr_sm8350_callback(struct qcom_battmgr *battmgr,
					 const struct qcom_battmgr_message *resp,
					 size_t len)
{
	unsigned int property;
	unsigned int opcode = le32_to_cpu(resp->hdr.opcode);
	size_t payload_len = len - sizeof(struct pmic_glink_hdr);
	unsigned int val;

	if (payload_len < sizeof(__le32)) {
		dev_warn(battmgr->dev, "invalid payload length for %#x: %zd\n",
			 opcode, len);
		return;
	}

	switch (opcode) {
	case BATTMGR_BAT_PROPERTY_GET:
		property = le32_to_cpu(resp->intval.property);
		if (property == BATT_MODEL_NAME) {
			if (payload_len != sizeof(resp->strval)) {
				dev_warn(battmgr->dev,
					 "invalid payload length for BATT_MODEL_NAME request: %zd\n",
					 payload_len);
				battmgr->error = -ENODATA;
				return;
			}
		} else {
			if (payload_len != sizeof(resp->intval)) {
				dev_warn(battmgr->dev,
					 "invalid payload length for %#x request: %zd\n",
					 property, payload_len);
				battmgr->error = -ENODATA;
				return;
			}

			battmgr->error = le32_to_cpu(resp->intval.result);
			if (battmgr->error)
				goto out_complete;
		}

		switch (property) {
		case BATT_STATUS:
			battmgr->status.status = le32_to_cpu(resp->intval.value);
			break;
		case BATT_HEALTH:
			battmgr->status.health = le32_to_cpu(resp->intval.value);
			break;
		case BATT_PRESENT:
			battmgr->info.present = le32_to_cpu(resp->intval.value);
			break;
		case BATT_CHG_TYPE:
			battmgr->info.charge_type = le32_to_cpu(resp->intval.value);
			break;
		case BATT_CAPACITY:
			battmgr->status.percent = le32_to_cpu(resp->intval.value) / 100;
			break;
		case BATT_SOH:
			battmgr->status.soh_percent = le32_to_cpu(resp->intval.value);
			break;
		case BATT_VOLT_OCV:
			battmgr->status.voltage_ocv = le32_to_cpu(resp->intval.value);
			break;
		case BATT_VOLT_NOW:
			battmgr->status.voltage_now = le32_to_cpu(resp->intval.value);
			break;
		case BATT_VOLT_MAX:
			battmgr->info.voltage_max = le32_to_cpu(resp->intval.value);
			break;
		case BATT_CURR_NOW:
			battmgr->status.current_now = le32_to_cpu(resp->intval.value);
			break;
		case BATT_TEMP:
			val = le32_to_cpu(resp->intval.value);
			battmgr->status.temperature = DIV_ROUND_CLOSEST(val, 10);
			break;
		case BATT_TECHNOLOGY:
			battmgr->info.technology = le32_to_cpu(resp->intval.value);
			break;
		case BATT_CHG_COUNTER:
			battmgr->info.charge_count = le32_to_cpu(resp->intval.value);
			break;
		case BATT_CYCLE_COUNT:
			battmgr->info.cycle_count = le32_to_cpu(resp->intval.value);
			break;
		case BATT_CHG_FULL_DESIGN:
			battmgr->info.design_capacity = le32_to_cpu(resp->intval.value);
			break;
		case BATT_CHG_FULL:
			battmgr->info.last_full_capacity = le32_to_cpu(resp->intval.value);
			break;
		case BATT_MODEL_NAME:
			strscpy(battmgr->info.model_number, resp->strval.model, BATTMGR_STRING_LEN);
			break;
		case BATT_TTF_AVG:
			battmgr->status.charge_time = le32_to_cpu(resp->intval.value);
			break;
		case BATT_TTE_AVG:
			battmgr->status.discharge_time = le32_to_cpu(resp->intval.value);
			break;
		case BATT_RESISTANCE:
			battmgr->status.resistance = le32_to_cpu(resp->intval.value);
			break;
		case BATT_POWER_NOW:
			battmgr->status.power_now = le32_to_cpu(resp->intval.value);
			break;
		case BATT_CHG_CTRL_START_THR:
			battmgr->info.charge_ctrl_start = le32_to_cpu(resp->intval.value);
			break;
		case BATT_CHG_CTRL_END_THR:
			battmgr->info.charge_ctrl_end = le32_to_cpu(resp->intval.value);
			break;
		default:
			dev_warn(battmgr->dev, "unknown property %#x\n", property);
			break;
		}
		break;
	case BATTMGR_USB_PROPERTY_GET:
		property = le32_to_cpu(resp->intval.property);
		if (payload_len != sizeof(resp->intval)) {
			dev_warn(battmgr->dev,
				 "invalid payload length for %#x request: %zd\n",
				 property, payload_len);
			battmgr->error = -ENODATA;
			return;
		}

		battmgr->error = le32_to_cpu(resp->intval.result);
		if (battmgr->error)
			goto out_complete;

		switch (property) {
		case USB_ONLINE:
			battmgr->usb.online = le32_to_cpu(resp->intval.value);
			break;
		case USB_VOLT_NOW:
			battmgr->usb.voltage_now = le32_to_cpu(resp->intval.value);
			break;
		case USB_VOLT_MAX:
			battmgr->usb.voltage_max = le32_to_cpu(resp->intval.value);
			break;
		case USB_CURR_NOW:
			battmgr->usb.current_now = le32_to_cpu(resp->intval.value);
			break;
		case USB_CURR_MAX:
			battmgr->usb.current_max = le32_to_cpu(resp->intval.value);
			break;
		case USB_INPUT_CURR_LIMIT:
			battmgr->usb.current_limit = le32_to_cpu(resp->intval.value);
			break;
		case USB_TYPE:
			battmgr->usb.usb_type = le32_to_cpu(resp->intval.value);
			break;
                case USB_ADAP_TYPE:
                        /*
                         * USB_ADAP_TYPE returns the real negotiated adapter type
                         * from the Oplus ADSP firmware. Use this as the reported
                         * usb_type so the power supply subsystem sees the actual
                         * adapter type (PD/DCP/etc.) instead of SDP.
                         */
                        battmgr->usb.adap_type = le32_to_cpu(resp->intval.value);
                        battmgr->usb.usb_type = battmgr->usb.adap_type;
                        break;
                case USB_OPLUS_VOOCPHY_STATUS:
                        battmgr->usb.voocphy_status_raw = le32_to_cpu(resp->intval.value);
                        break;
		default:
			dev_warn(battmgr->dev, "unknown property %#x\n", property);
			break;
		}
		break;
	case BATTMGR_WLS_PROPERTY_GET:
		property = le32_to_cpu(resp->intval.property);
		if (payload_len != sizeof(resp->intval)) {
			dev_warn(battmgr->dev,
				 "invalid payload length for %#x request: %zd\n",
				 property, payload_len);
			battmgr->error = -ENODATA;
			return;
		}

		battmgr->error = le32_to_cpu(resp->intval.result);
		if (battmgr->error)
			goto out_complete;

		switch (property) {
		case WLS_ONLINE:
			battmgr->wireless.online = le32_to_cpu(resp->intval.value);
			break;
		case WLS_VOLT_NOW:
			battmgr->wireless.voltage_now = le32_to_cpu(resp->intval.value);
			break;
		case WLS_VOLT_MAX:
			battmgr->wireless.voltage_max = le32_to_cpu(resp->intval.value);
			break;
		case WLS_CURR_NOW:
			battmgr->wireless.current_now = le32_to_cpu(resp->intval.value);
			break;
		case WLS_CURR_MAX:
			battmgr->wireless.current_max = le32_to_cpu(resp->intval.value);
			break;
		default:
			dev_warn(battmgr->dev, "unknown property %#x\n", property);
			break;
		}
		break;
	case BATTMGR_REQUEST_NOTIFICATION:
	case BATTMGR_CHG_CTRL_LIMIT_EN:
		battmgr->error = 0;
		break;
        case BATTMGR_BAT_PROPERTY_SET:
        case BATTMGR_USB_PROPERTY_SET:
                /* Response to property write (e.g. Oplus gauge init, voocphy enable) */
                dev_dbg(battmgr->dev, "property set response: opcode=0x%x prop=%u result=%u\n",
                        opcode, le32_to_cpu(resp->intval.property),
                        le32_to_cpu(resp->intval.result));
                battmgr->error = le32_to_cpu(resp->intval.result);
                break;
	default:
		dev_warn(battmgr->dev, "unknown message %#x\n", opcode);
		break;
	}

out_complete:
	complete(&battmgr->ack);
}

static void qcom_battmgr_callback(const void *data, size_t len, void *priv)
{
	const struct pmic_glink_hdr *hdr = data;
	struct qcom_battmgr *battmgr = priv;
	unsigned int opcode = le32_to_cpu(hdr->opcode);

	if (opcode == BATTMGR_NOTIFICATION)
		qcom_battmgr_notification(battmgr, data, len);
	else if (opcode == OEM_OPCODE_READ_BUFFER)
                qcom_battmgr_oem_read_response(battmgr, data, len);
	else if (battmgr->variant == QCOM_BATTMGR_SC8280XP ||
		 battmgr->variant == QCOM_BATTMGR_X1E80100)
		qcom_battmgr_sc8280xp_callback(battmgr, data, len);
	else
		qcom_battmgr_sm8350_callback(battmgr, data, len);
}

/*
 * VOOC status read worker.
 *
 * Triggered by notification 0x48 (BC_VOOC_STATUS_GET) from the ADSP.
 * Reads USB_OPLUS_VOOCPHY_STATUS and decodes the packed VOOC state:
 *   bits [7:0]  = VOOC fast charge status (enum adsp_voocphy_fast_status)
 *   bits [14:8] = fast charger type ID
 *   bits [23:16] = track/error status
 */
static void qcom_battmgr_voocphy_status_worker(struct work_struct *work)
{
        struct qcom_battmgr *battmgr = container_of(work,
                        struct qcom_battmgr, voocphy_status_work);
        static const char * const vooc_state_names[] = {
                [ADSP_VPHY_FAST_NOTIFY_UNKNOW]           = "UNKNOWN",
                [ADSP_VPHY_FAST_NOTIFY_PRESENT]          = "PRESENT",
                [ADSP_VPHY_FAST_NOTIFY_ONGOING]          = "ONGOING",
                [ADSP_VPHY_FAST_NOTIFY_ABSENT]           = "ABSENT",
                [ADSP_VPHY_FAST_NOTIFY_FULL]             = "FULL",
                [ADSP_VPHY_FAST_NOTIFY_BAD_CONNECTED]    = "BAD_CONNECTED",
                [ADSP_VPHY_FAST_NOTIFY_BATT_TEMP_OVER]   = "BATT_TEMP_OVER",
                [ADSP_VPHY_FAST_NOTIFY_BTB_TEMP_OVER]    = "BTB_TEMP_OVER",
                [ADSP_VPHY_FAST_NOTIFY_DUMMY_START]      = "DUMMY_START",
                [ADSP_VPHY_FAST_NOTIFY_ADAPTER_COPYCAT]  = "ADAPTER_COPYCAT",
                [ADSP_VPHY_FAST_NOTIFY_ERR_COMMU]        = "ERR_COMMU",
                [ADSP_VPHY_FAST_NOTIFY_SWITCH_TEMP_RANGE]= "SWITCH_TEMP_RANGE",
                [ADSP_VPHY_FAST_NOTIFY_COMMU_TIME_OUT]   = "COMMU_TIME_OUT",
                [ADSP_VPHY_FAST_NOTIFY_COMMU_CLK_ERR]    = "COMMU_CLK_ERR",
                [ADSP_VPHY_FAST_NOTIFY_COMMU_SEND_ERR]   = "COMMU_SEND_ERR",
                [ADSP_VPHY_FAST_NOTIFY_HW_VBATT_HIGH]    = "HW_VBATT_HIGH",
                [ADSP_VPHY_FAST_NOTIFY_HW_TBATT_HIGH]    = "HW_TBATT_HIGH",
		[ADSP_VPHY_FAST_NOTIFY_CRASH]    	 = "CRASH",
        };
        int ret;
        unsigned int raw, vooc_state, fast_chg_type, track_status;
        const char *state_name;

        if (!battmgr->service_up)
                return;

        mutex_lock(&battmgr->lock);
        ret = qcom_battmgr_request_property(battmgr,
                        BATTMGR_USB_PROPERTY_GET,
                        USB_OPLUS_VOOCPHY_STATUS, 0);
        mutex_unlock(&battmgr->lock);

        if (ret) {
                dev_err(battmgr->dev,
                        "vooc: failed to read VOOCPHY_STATUS: %d\n", ret);
                return;
        }

        raw = battmgr->usb.voocphy_status_raw;
        vooc_state = raw & 0xFF;
        fast_chg_type = (raw >> 8) & 0x7F;
        track_status = (raw >> 16) & 0xFF;

        state_name = (vooc_state < ARRAY_SIZE(vooc_state_names) &&
                      vooc_state_names[vooc_state])
                     ? vooc_state_names[vooc_state] : "?";

        dev_info(battmgr->dev,
                 "vooc status: raw=0x%08x state=%s(%u) fast_chg_type=0x%02x track=0x%02x\n",
                 raw, state_name, vooc_state, fast_chg_type, track_status);

        /*
         * On error states (ERR_COMMU, COMMU_TIME_OUT, COMMU_CLK_ERR),
         * re-enable VoocPHY to retry the handshake — matching stock
         * behavior in oplus_adsp_voocphy_status_func().
         */
        if (vooc_state == ADSP_VPHY_FAST_NOTIFY_ERR_COMMU ||
            vooc_state == ADSP_VPHY_FAST_NOTIFY_COMMU_TIME_OUT ||
            vooc_state == ADSP_VPHY_FAST_NOTIFY_COMMU_CLK_ERR) {
                dev_warn(battmgr->dev,
                         "vooc: communication error (%s), will re-enable VoocPHY\n",
                         state_name);
                battmgr->vooc_limits_set = false;
                battmgr->last_configured_adap_type = -1;
        }

        // power_supply_changed(battmgr->bat_psy);
}

/*
 * Oplus ADSP fuel gauge + VoocPHY init.
 *
 * The stock Oplus firmware requires two init steps:
 *
 * 1. Gauge init: property_id=38 (BATT_ADSP_GAUGE_INIT) with value=1 via
 *    opcode 0x31 (BATTMGR_BAT_PROPERTY_SET). This initializes the bq27541
 *    fuel gauge on the ADSP.
 *
 * 2. VoocPHY enable: property_id=13 (USB_VOOCPHY_ENABLE) with value=1 via
 *    opcode 0x33 (BATTMGR_USB_PROPERTY_SET). The VoocPHY subsystem on the
 *    ADSP manages both VOOC fast charging AND the fuel gauge. Without
 *    enabling it, the gauge may not fully populate SOC.
 *
 * This matches the stock driver's crash recovery sequence:
 *   oplus_ap_init_adsp_gague() → oplus_adsp_voocphy_enable(true)
 */
static void qcom_battmgr_oplus_gauge_init(struct qcom_battmgr *battmgr)
{
        int ret;

        if (battmgr->gauge_init_done) {
                dev_dbg(battmgr->dev, "oplus gauge init: already done, skipping\n");
                return;
        }

        /* Step 1: Initialize the ADSP fuel gauge */
        dev_info(battmgr->dev,
                 "oplus gauge init: sending BATT property_id=%d value=1 via opcode 0x%x\n",
                 BATT_OPLUS_ADSP_GAUGE_INIT, BATTMGR_BAT_PROPERTY_SET);

        mutex_lock(&battmgr->lock);
        ret = qcom_battmgr_request_property(battmgr, BATTMGR_BAT_PROPERTY_SET,
                                            BATT_OPLUS_ADSP_GAUGE_INIT, 1);
        mutex_unlock(&battmgr->lock);

        if (ret) {
                dev_err(battmgr->dev,
                        "oplus gauge init: FAILED ret=%d\n", ret);
                return;
        }
        dev_info(battmgr->dev, "oplus gauge init: SUCCESS\n");

        /* Step 2: Enable the ADSP VoocPHY subsystem */
        dev_info(battmgr->dev,
                 "oplus voocphy enable: sending USB property_id=%d value=1 via opcode 0x%x\n",
                 USB_OPLUS_VOOCPHY_ENABLE, BATTMGR_USB_PROPERTY_SET);

        mutex_lock(&battmgr->lock);
        ret = qcom_battmgr_request_property(battmgr, BATTMGR_USB_PROPERTY_SET,
                                            USB_OPLUS_VOOCPHY_ENABLE, 1);
        mutex_unlock(&battmgr->lock);

        if (ret) {
                dev_err(battmgr->dev,
                        "oplus voocphy enable: FAILED ret=%d\n", ret);
        } else {
                dev_info(battmgr->dev, "oplus voocphy enable: SUCCESS\n");
        }

        /* Step 3: Write dummy RTC SOC to initialize gauge state */
        dev_info(battmgr->dev, "oplus gauge: sending BATT_RTC_SOC (id=31) via opcode 0x31\n");
        mutex_lock(&battmgr->lock);
        ret = qcom_battmgr_request_property(battmgr, BATTMGR_BAT_PROPERTY_SET,
                                            BATT_OPLUS_RTC_SOC, 50); /* Dummy 50% */
        mutex_unlock(&battmgr->lock);
        if (ret)
                dev_warn(battmgr->dev, "failed to send BATT_RTC_SOC: %d (non-fatal)\n", ret);

        /* Step 4: Write UPDATE_SOC_SMOOTH_PARAM to trigger internal updates */
        dev_info(battmgr->dev, "oplus gauge: sending BATT_UPDATE_SOC_SMOOTH_PARAM (id=39) via opcode 0x31\n");
        mutex_lock(&battmgr->lock);
        ret = qcom_battmgr_request_property(battmgr, BATTMGR_BAT_PROPERTY_SET,
                                            BATT_OPLUS_UPDATE_SOC_SMOOTH_PARAM, 1);
        mutex_unlock(&battmgr->lock);
        if (ret)
                dev_warn(battmgr->dev, "failed to send BATT_UPDATE_SOC_SMOOTH_PARAM: %d (non-fatal)\n", ret);

        /*
         * Step 5: Detect adapter type and set charging parameters.
         * Query USB_ADAP_TYPE to determine the actual charger type,
         * then set PDO voltage, ICL, and FCC accordingly instead of
         * hardcoding values that only work for one charger type.
         */
        {
                int adap_type;
                int pdo_mv = 0;    /* 0 = don't change PDO */
                int icl_ua = 500000;  /* default SDP 500mA */
                int fcc_ua = 500000;  /* default SDP 500mA */

                /* Query the adapter type from the ADSP */
                mutex_lock(&battmgr->lock);
                ret = qcom_battmgr_request_property(battmgr,
                                BATTMGR_USB_PROPERTY_GET, USB_ADAP_TYPE, 0);
                mutex_unlock(&battmgr->lock);

                adap_type = battmgr->usb.adap_type;
                if (ret) {
                        dev_warn(battmgr->dev,
                                 "failed to query adapter type: %d, assuming PD\n", ret);
                        adap_type = POWER_SUPPLY_USB_TYPE_PD;
                }

                switch (adap_type) {
                case POWER_SUPPLY_USB_TYPE_PD:
                case POWER_SUPPLY_USB_TYPE_PD_DRP:
                case POWER_SUPPLY_USB_TYPE_PD_PPS:
                case POWER_SUPPLY_USB_TYPE_PD_SDP:
                        pdo_mv = 9000;
                        icl_ua = 3000000;
                        fcc_ua = 3000000;
                        dev_info(battmgr->dev,
                                 "oplus: PD/PPS adapter detected (type=%d), requesting 9V/3A\n",
                                 adap_type);
                        break;
                case POWER_SUPPLY_USB_TYPE_DCP:
                        /*
                         * DCP: no PD negotiation. Set high ICL/FCC because
                         * VOOC/SuperVOOC chargers present as DCP to BC1.2.
                         * The ADSP VoocPHY handles the proprietary handshake
                         * and regulates current internally — low Linux-side
                         * limits starve the VoocPHY and block fast charging.
                         */
                        pdo_mv = 0;
                        icl_ua = 7300000;
                        fcc_ua = 7300000;
                        dev_info(battmgr->dev,
                                 "oplus: DCP adapter detected, setting 7.3A ICL (VOOC/SuperVOOC)\n");
                        break;
                case POWER_SUPPLY_USB_TYPE_CDP:
                        pdo_mv = 0;
                        icl_ua = 1500000;
                        fcc_ua = 1500000;
                        dev_info(battmgr->dev,
                                 "oplus: CDP adapter detected, setting 1.5A ICL\n");
                        break;
                case POWER_SUPPLY_USB_TYPE_C:
                        pdo_mv = 0;
                        icl_ua = 1500000;
                        fcc_ua = 2000000;
                        dev_info(battmgr->dev,
                                 "oplus: Type-C adapter detected, setting 1.5A ICL\n");
                        break;
                case POWER_SUPPLY_USB_TYPE_SDP:
                default:
                        pdo_mv = 0;
                        icl_ua = 500000;
                        fcc_ua = 500000;
                        dev_info(battmgr->dev,
                                 "oplus: SDP/unknown adapter (type=%d), using defaults\n",
                                 adap_type);
                        break;
                }

                /* Step 5a: Request higher voltage PDO if PD-capable */
                if (pdo_mv > 0) {
                        mutex_lock(&battmgr->lock);
                        ret = qcom_battmgr_request_property(battmgr,
                                        BATTMGR_BAT_PROPERTY_SET,
                                        BATT_OPLUS_SET_PDO, pdo_mv);
                        mutex_unlock(&battmgr->lock);
                        if (ret)
                                dev_warn(battmgr->dev,
                                         "failed to set PDO %dmV: %d\n",
                                         pdo_mv, ret);
                        else
                                msleep(300); /* PD re-negotiation delay */
                }

                /* Step 6: Set input current limit (after PDO switch) */
                dev_info(battmgr->dev,
                         "oplus: setting ICL to %d.%03dA\n",
                         icl_ua / 1000000, (icl_ua % 1000000) / 1000);
                mutex_lock(&battmgr->lock);
                ret = qcom_battmgr_request_property(battmgr,
                                BATTMGR_USB_PROPERTY_SET,
                                USB_INPUT_CURR_LIMIT, icl_ua);
                mutex_unlock(&battmgr->lock);
                if (ret)
                        dev_warn(battmgr->dev,
                                 "failed to set ICL: %d\n", ret);

                /* Step 7: Enable charging */
                mutex_lock(&battmgr->lock);
                ret = qcom_battmgr_request_property(battmgr,
                                BATTMGR_BAT_PROPERTY_SET,
                                BATT_OPLUS_CHG_EN, 1);
                mutex_unlock(&battmgr->lock);
                if (ret)
                        dev_warn(battmgr->dev,
                                 "failed to enable charging: %d\n", ret);

                /*
                 * Step 8: Set fast charge current.
                 * Skip for DCP — VOOC/SuperVOOC chargers present as DCP,
                 * and the ADSP VoocPHY controls charging current autonomously.
                 * Writing BATT_CHG_CTRL_LIM during VOOC causes ADSP error 514.
                 */
                if (adap_type != POWER_SUPPLY_USB_TYPE_DCP) {
                        dev_info(battmgr->dev,
                                 "oplus: setting FCC to %d.%03dA\n",
                                 fcc_ua / 1000000, (fcc_ua % 1000000) / 1000);
                        mutex_lock(&battmgr->lock);
                        ret = qcom_battmgr_request_property(battmgr,
                                        BATTMGR_BAT_PROPERTY_SET,
                                        BATT_CHG_CTRL_LIM, fcc_ua);
                        mutex_unlock(&battmgr->lock);
                        if (ret)
                                dev_warn(battmgr->dev,
                                         "failed to set FCC: %d\n", ret);
                } else {
                        dev_info(battmgr->dev,
                                 "oplus: DCP/VOOC mode — skipping FCC write (ADSP VoocPHY controls current)\n");
                }
                battmgr->last_configured_adap_type = adap_type;
        }

        battmgr->gauge_init_done = true;
        battmgr->vooc_limits_set = true;
        dev_info(battmgr->dev, "oplus gauge init sequence complete\n");

        /* Start periodic VoocPHY re-enable check (every 5s, like stock) */
        schedule_delayed_work(&battmgr->voocphy_recheck_work,
                              msecs_to_jiffies(5000));
}

/*
 * Worker to reply to ADSP whenever it asks for AP charging status (notification 0x59).
 * If we fail to reply with BATT_OPLUS_SEND_CHG_STATUS (37) = 1, the ADSP will abort
 * the VOOC handshake and drop to 5V/0.5A charging.
 */
static void qcom_battmgr_chg_status_reply_work(struct work_struct *work)
{
        struct qcom_battmgr *battmgr = container_of(work,
                        struct qcom_battmgr, chg_status_reply_work);
        int status = !battmgr->adsp_suspended_chg;
        int ret;

        if (!battmgr->service_up)
                return;

        mutex_lock(&battmgr->lock);
        ret = qcom_battmgr_request_property(battmgr,
                        BATTMGR_BAT_PROPERTY_SET,
                        BATT_OPLUS_SEND_CHG_STATUS, status);
        mutex_unlock(&battmgr->lock);

        if (ret)
                dev_warn(battmgr->dev, "failed to send chg status %d: %d\n", status, ret);
}

/*
 * Periodic VoocPHY re-enable worker.
 *
 * The stock Oplus driver (oplus_adsp_voocphy_enable_check_func) polls
 * every 5 seconds and re-enables VoocPHY if it has been disabled.
 * VOOC chargers present as DCP — once VoocPHY is enabled, the ADSP
 * hardware performs the proprietary handshake autonomously. Without
 * periodic re-enabling, VoocPHY may fail on first attempt and never
 * retry, leaving the charger stuck at low-power DCP.
 *
 * This worker also handles charger hot-plug: if the gauge init ran
 * before the charger was plugged in (SDP/0.5A), we re-query the
 * adapter type and upgrade ICL/FCC when DCP is detected.
 */
static void qcom_battmgr_voocphy_recheck(struct work_struct *work)
{
        struct qcom_battmgr *battmgr = container_of(work,
                        struct qcom_battmgr, voocphy_recheck_work.work);
        int ret, adap_type;

        if (!battmgr->service_up)
                return;

        /*
         * Refresh usb.online — it only gets updated when explicitly
         * queried, and stale values cause wrong status reporting and
         * repeated charger setup.
         */
        mutex_lock(&battmgr->lock);
        qcom_battmgr_request_property(battmgr,
                        BATTMGR_USB_PROPERTY_GET, USB_ONLINE, 0);
        mutex_unlock(&battmgr->lock);

        if (!battmgr->usb.online || battmgr->otg_enabled) {
                /*
                 * Debounce: require 2 consecutive offline readings (~10s)
                 * before treating as a genuine unplug.  During PD
                 * negotiation USB_ONLINE flaps briefly to 0 — reacting
                 * immediately causes status oscillation in upower and
                 * re-sends ICL/FCC commands that disrupt negotiation.
                 */
                battmgr->usb_offline_count++;
                if (battmgr->usb_offline_count >= 2) {
                        battmgr->vooc_limits_set = false;
                        battmgr->adsp_suspended_chg = false;
                        battmgr->last_configured_adap_type = -1;
                }
                goto reschedule;
        }
        battmgr->usb_offline_count = 0;

        /*
         * Re-query the adapter type from the ADSP.  The type may have
         * changed since gauge init (e.g. charger plugged in after boot).
         */
        mutex_lock(&battmgr->lock);
        ret = qcom_battmgr_request_property(battmgr,
                        BATTMGR_USB_PROPERTY_GET, USB_ADAP_TYPE, 0);
        mutex_unlock(&battmgr->lock);

        adap_type = battmgr->usb.adap_type;
        if (ret) {
                dev_dbg(battmgr->dev,
                        "voocphy recheck: failed to query adap_type: %d\n",
                        ret);
                goto reschedule;
        }

        /*
         * First time seeing a charger after plug-in: configure charging
         * parameters based on the detected adapter type.  Don't repeat —
         * the ADSP commands interfere with time-sensitive VOOC handshakes
         * if sent repeatedly.
         */
        /* Force reconfiguration if adapter type changed (e.g. SDP→PD) */
        if (battmgr->vooc_limits_set &&
            adap_type != battmgr->last_configured_adap_type)
                battmgr->vooc_limits_set = false;

        if (!battmgr->vooc_limits_set) {
                int pdo_mv = 0;
                int icl_ua = 500000;
                int fcc_ua = 500000;

                switch (adap_type) {
                case POWER_SUPPLY_USB_TYPE_PD:
                case POWER_SUPPLY_USB_TYPE_PD_DRP:
                case POWER_SUPPLY_USB_TYPE_PD_PPS:
                case POWER_SUPPLY_USB_TYPE_PD_SDP:
                        pdo_mv = 9000;
                        icl_ua = 3000000;
                        fcc_ua = 3000000;
                        dev_info(battmgr->dev,
                                "voocphy recheck: PD adapter (type=%d), requesting 9V/3A\n",
                                adap_type);
                        break;
                case POWER_SUPPLY_USB_TYPE_DCP:
                        icl_ua = 7300000;
                        fcc_ua = 7300000;
                        dev_info(battmgr->dev,
                                "voocphy recheck: DCP detected, setting 7.3A ICL (VOOC/SuperVOOC)\n");
                        break;
                case POWER_SUPPLY_USB_TYPE_CDP:
                        icl_ua = 1500000;
                        fcc_ua = 1500000;
                        dev_info(battmgr->dev,
                                "voocphy recheck: CDP detected, setting 1.5A ICL\n");
                        break;
                case POWER_SUPPLY_USB_TYPE_C:
                        icl_ua = 1500000;
                        fcc_ua = 2000000;
                        dev_info(battmgr->dev,
                                "voocphy recheck: Type-C detected, setting 1.5A ICL\n");
                        break;
                case POWER_SUPPLY_USB_TYPE_SDP:
                default:
                        dev_info(battmgr->dev,
                                "voocphy recheck: SDP/unknown (type=%d), using defaults\n",
                                adap_type);
                        break;
                }

                /* Request higher voltage PDO if PD-capable */
                if (pdo_mv > 0) {
                        mutex_lock(&battmgr->lock);
                        ret = qcom_battmgr_request_property(battmgr,
                                        BATTMGR_BAT_PROPERTY_SET,
                                        BATT_OPLUS_SET_PDO, pdo_mv);
                        mutex_unlock(&battmgr->lock);
                        if (ret)
                                dev_warn(battmgr->dev,
                                        "recheck: failed to set PDO %dmV: %d\n",
                                        pdo_mv, ret);
                        else
                                msleep(300); /* PD re-negotiation delay */
                }

                /* Set ICL */
                mutex_lock(&battmgr->lock);
                qcom_battmgr_request_property(battmgr,
                                BATTMGR_USB_PROPERTY_SET,
                                USB_INPUT_CURR_LIMIT, icl_ua);
                /* Enable charging */
                qcom_battmgr_request_property(battmgr,
                                BATTMGR_BAT_PROPERTY_SET,
                                BATT_OPLUS_CHG_EN, 1);
                /* Enable VoocPHY (needed for DCP/VOOC) */
                qcom_battmgr_request_property(battmgr,
                                BATTMGR_USB_PROPERTY_SET,
                                USB_OPLUS_VOOCPHY_ENABLE, 1);
                mutex_unlock(&battmgr->lock);

                /* Set FCC (skip for DCP — VoocPHY controls current) */
                if (adap_type != POWER_SUPPLY_USB_TYPE_DCP) {
                        mutex_lock(&battmgr->lock);
                        qcom_battmgr_request_property(battmgr,
                                        BATTMGR_BAT_PROPERTY_SET,
                                        BATT_CHG_CTRL_LIM, fcc_ua);
                        mutex_unlock(&battmgr->lock);
                }

                battmgr->vooc_limits_set = true;
                battmgr->last_configured_adap_type = adap_type;

                /* Notify userspace so upower re-reads battery status */
                power_supply_changed(battmgr->bat_psy);
                power_supply_changed(battmgr->usb_psy);
                goto reschedule;
        }

        /* Subsequent cycles for DCP: re-enable VoocPHY (stock behavior) */
        if (adap_type == POWER_SUPPLY_USB_TYPE_DCP) {
                mutex_lock(&battmgr->lock);
                ret = qcom_battmgr_request_property(battmgr,
                                BATTMGR_USB_PROPERTY_SET,
                                USB_OPLUS_VOOCPHY_ENABLE, 1);
                mutex_unlock(&battmgr->lock);
                if (ret)
                        dev_dbg(battmgr->dev,
                                "voocphy re-enable failed: %d\n", ret);
        }

reschedule:
        schedule_delayed_work(&battmgr->voocphy_recheck_work,
                              msecs_to_jiffies(5000));
}


static void qcom_battmgr_enable_worker(struct work_struct *work)
{
	struct qcom_battmgr *battmgr = container_of(work, struct qcom_battmgr, enable_work);
	struct qcom_battmgr_enable_request req = {
		.hdr.owner = cpu_to_le32(PMIC_GLINK_OWNER_BATTMGR),
		.hdr.type = cpu_to_le32(PMIC_GLINK_NOTIFY),
		.hdr.opcode = cpu_to_le32(BATTMGR_REQUEST_NOTIFICATION),
	};
	int ret;

	ret = qcom_battmgr_request(battmgr, &req, sizeof(req));
        if (ret) {
                dev_err(battmgr->dev, "failed to request power notifications\n");
                return;
        }

        /*
         * Initialize the Oplus ADSP fuel gauge after GLINK is up and
         * notifications are registered. This matches the stock driver's
         * probe sequence which calls oplus_ap_init_adsp_gague() right after
         * pmic_glink_register_client().
         */
        qcom_battmgr_oplus_gauge_init(battmgr);
	schedule_delayed_work(&battmgr->otg_init_work, round_jiffies_relative(msecs_to_jiffies(3500)));
}

static void qcom_battmgr_pdr_notify(void *priv, int state)
{
	struct qcom_battmgr *battmgr = priv;

	if (state == SERVREG_SERVICE_STATE_UP) {
		battmgr->service_up = true;
		schedule_work(&battmgr->enable_work);
	} else {
		battmgr->service_up = false;
	}
}

static const struct of_device_id qcom_battmgr_of_variants[] = {
	{ .compatible = "qcom,glymur-pmic-glink", .data = (void *)QCOM_BATTMGR_X1E80100 },
	{ .compatible = "qcom,kaanapali-pmic-glink", .data = (void *)QCOM_BATTMGR_SM8550 },
	{ .compatible = "qcom,sc8180x-pmic-glink", .data = (void *)QCOM_BATTMGR_SC8280XP },
	{ .compatible = "qcom,sc8280xp-pmic-glink", .data = (void *)QCOM_BATTMGR_SC8280XP },
	{ .compatible = "qcom,sm8550-pmic-glink", .data = (void *)QCOM_BATTMGR_SM8550 },
	{ .compatible = "qcom,x1e80100-pmic-glink", .data = (void *)QCOM_BATTMGR_X1E80100 },
	/* Unmatched devices falls back to QCOM_BATTMGR_SM8350 */
	{}
};

static char *qcom_battmgr_battery[] = { "battery" };

static void oplus_otg_init_status_func(struct work_struct *work)
{
	int count = 20;
	struct qcom_battmgr *battmgr = container_of(to_delayed_work(work), struct qcom_battmgr, otg_init_work);

	while (count--)
		msleep(500);


	qcom_battmgr_request_property(battmgr, BATTMGR_USB_PROPERTY_SET, USB_OTG_AP_ENABLE, true);

	// oplus_get_otg_online_status_with_cid_scheme(battmgr);
	// if (battmgr->cid_status != 0) {
	// 	chg_err("Oplus_otg_ap_enable,flag bcdev->cid_status != 0\n");
	// 	oplus_ccdetect_enable(battmgr);
	// }
}

static int qcom_battmgr_probe(struct auxiliary_device *adev,
			      const struct auxiliary_device_id *id)
{
	const struct power_supply_desc *psy_desc;
	struct power_supply_config psy_cfg_supply = {};
	struct power_supply_config psy_cfg = {};
	const struct of_device_id *match;
	struct qcom_battmgr *battmgr;
	struct device *dev = &adev->dev;
	int ret;

	battmgr = devm_kzalloc(dev, sizeof(*battmgr), GFP_KERNEL);
	if (!battmgr)
		return -ENOMEM;

	battmgr->dev = dev;
	/* Apply to both standard property and OEM buffer readings. */
	battmgr->invert_current = of_machine_is_compatible("oplus,negroni");

	psy_cfg.drv_data = battmgr;
	psy_cfg.fwnode = dev_fwnode(&adev->dev);

	psy_cfg_supply.drv_data = battmgr;
	psy_cfg_supply.fwnode = dev_fwnode(&adev->dev);
	psy_cfg_supply.supplied_to = qcom_battmgr_battery;
	psy_cfg_supply.num_supplicants = 1;

	INIT_WORK(&battmgr->enable_work, qcom_battmgr_enable_worker);
        INIT_DELAYED_WORK(&battmgr->voocphy_recheck_work,
                          qcom_battmgr_voocphy_recheck);
        INIT_WORK(&battmgr->voocphy_status_work,
                  qcom_battmgr_voocphy_status_worker);
        INIT_WORK(&battmgr->chg_status_reply_work,
                  qcom_battmgr_chg_status_reply_work);
	INIT_DELAYED_WORK(&battmgr->otg_init_work, oplus_otg_init_status_func);
        battmgr->last_configured_adap_type = -1;
	mutex_init(&battmgr->lock);
	init_completion(&battmgr->ack);
	mutex_init(&battmgr->oem_read_lock);
        init_completion(&battmgr->oem_read_ack);

	oplus_otg_boost_en_gpio_init(battmgr);
	oplus_otg_ovp_en_gpio_init(battmgr);

	match = of_match_device(qcom_battmgr_of_variants, dev->parent);
	if (match)
		battmgr->variant = (unsigned long)match->data;
	else
		battmgr->variant = QCOM_BATTMGR_SM8350;

	ret = qcom_battmgr_charge_control_thresholds_init(battmgr);
	if (ret < 0)
		return dev_err_probe(dev, ret,
				     "failed to init battery charge control thresholds\n");

	if (battmgr->variant == QCOM_BATTMGR_SC8280XP ||
	    battmgr->variant == QCOM_BATTMGR_X1E80100) {
		if (battmgr->variant == QCOM_BATTMGR_X1E80100)
			psy_desc = &x1e80100_bat_psy_desc;
		else
			psy_desc = &sc8280xp_bat_psy_desc;

		battmgr->bat_psy = devm_power_supply_register(dev, psy_desc, &psy_cfg);
		if (IS_ERR(battmgr->bat_psy))
			return dev_err_probe(dev, PTR_ERR(battmgr->bat_psy),
					     "failed to register battery power supply\n");

		battmgr->ac_psy = devm_power_supply_register(dev, &sc8280xp_ac_psy_desc, &psy_cfg_supply);
		if (IS_ERR(battmgr->ac_psy))
			return dev_err_probe(dev, PTR_ERR(battmgr->ac_psy),
					     "failed to register AC power supply\n");

		battmgr->usb_psy = devm_power_supply_register(dev, &sc8280xp_usb_psy_desc, &psy_cfg_supply);
		if (IS_ERR(battmgr->usb_psy))
			return dev_err_probe(dev, PTR_ERR(battmgr->usb_psy),
					     "failed to register USB power supply\n");

		battmgr->wls_psy = devm_power_supply_register(dev, &sc8280xp_wls_psy_desc, &psy_cfg_supply);
		if (IS_ERR(battmgr->wls_psy))
			return dev_err_probe(dev, PTR_ERR(battmgr->wls_psy),
					     "failed to register wireless charing power supply\n");
	} else {
		if (battmgr->variant == QCOM_BATTMGR_SM8550)
			psy_desc = &sm8550_bat_psy_desc;
		else
			psy_desc = &sm8350_bat_psy_desc;

		battmgr->bat_psy = devm_power_supply_register(dev, psy_desc, &psy_cfg);
		if (IS_ERR(battmgr->bat_psy))
			return dev_err_probe(dev, PTR_ERR(battmgr->bat_psy),
					     "failed to register battery power supply\n");

		battmgr->usb_psy = devm_power_supply_register(dev, &sm8350_usb_psy_desc, &psy_cfg_supply);
		if (IS_ERR(battmgr->usb_psy))
			return dev_err_probe(dev, PTR_ERR(battmgr->usb_psy),
					     "failed to register USB power supply\n");

		battmgr->wls_psy = devm_power_supply_register(dev, &sm8350_wls_psy_desc, &psy_cfg_supply);
		if (IS_ERR(battmgr->wls_psy))
			return dev_err_probe(dev, PTR_ERR(battmgr->wls_psy),
					     "failed to register wireless charing power supply\n");
	}

	ret = devm_work_autocancel(dev, &battmgr->enable_work,
				   qcom_battmgr_enable_worker);
	if (ret)
		return ret;

	battmgr->client = devm_pmic_glink_client_alloc(dev, PMIC_GLINK_OWNER_BATTMGR,
						       qcom_battmgr_callback,
						       qcom_battmgr_pdr_notify,
						       battmgr);
	if (IS_ERR(battmgr->client))
		return PTR_ERR(battmgr->client);

	pmic_glink_client_register(battmgr->client);

	return 0;
}

static const struct auxiliary_device_id qcom_battmgr_id_table[] = {
	{ .name = "pmic_glink.power-supply", },
	{},
};
MODULE_DEVICE_TABLE(auxiliary, qcom_battmgr_id_table);

static struct auxiliary_driver qcom_battmgr_driver = {
	.name = "pmic_glink_power_supply",
	.probe = qcom_battmgr_probe,
	.id_table = qcom_battmgr_id_table,
};

module_auxiliary_driver(qcom_battmgr_driver);

MODULE_DESCRIPTION("Qualcomm PMIC GLINK battery manager driver");
MODULE_LICENSE("GPL");
