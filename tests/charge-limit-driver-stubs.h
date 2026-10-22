/* Test harness for the actual driver command/evaluation functions. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#include "../linux/drivers/power/supply/qcom_battmgr_charge_limit.h"
typedef uint32_t u32;
typedef uint32_t __le32;
typedef uint16_t __le16;
typedef uint8_t u8;
#define cpu_to_le32(x) (x)
#define le32_to_cpu(x) (x)
#define DIV_ROUND_CLOSEST(x,y) (((x) + (y) / 2) / (y))
#define dev_warn(...) ((void)0)
#define dev_dbg(...) ((void)0)
#define complete(x) (++*(x))
#define strscpy(dst,src,n) snprintf(dst,n,"%s",src)
#define BATTMGR_STRING_LEN 128
#define BATTMGR_CHEMISTRY_LEN 4
#define READ_ONCE(x) (x)
#define WRITE_ONCE(x,v) ((x)=(v))
#define lockdep_assert_held(x) ((void)(x))
#define dev_info(...) ((void)0)
#define power_supply_changed(x) ((void)(x))
#define PMIC_GLINK_OWNER_BATTMGR 32778
#define PMIC_GLINK_REQ_RESP 1
#define POWER_SUPPLY_USB_TYPE_DCP 2
struct pmic_glink_hdr { u32 owner, type, opcode; };
/* SHARED_STRUCTURES */
struct test_chip { bool adsp_notify_ap_suspend; };
struct battery_chg_dev {
    struct { unsigned usb_in_status, usb_suspend_pmic, usb_subtype; } usb;
    struct test_chip *chip;
    unsigned curr_thermal_level, num_thermal_levels;
};
struct qcom_battmgr_property_request { struct pmic_glink_hdr hdr; u32 battery, property, value; };
struct qcom_battmgr {
    int lock, dev, bat_psy, ack, error;
    struct battery_chg_dev *bcdev;
    unsigned pending_opcode;
    bool charge_limit_supported, service_up, gauge_init_done;
    struct qcom_battmgr_charge_limit charge_limit;
    bool charge_limit_applied, charge_limit_applied_valid, charge_limit_vooc_disabled;
    bool adsp_suspended_chg;
    unsigned charge_limit_generation, service_generation, pending_property, charge_limit_soc_raw;
    struct qcom_battmgr_usb usb;
    struct qcom_battmgr_info info;
    struct qcom_battmgr_status status;
    struct qcom_battmgr_wireless wireless;
};
static struct qcom_battmgr_property_request trace[100];
static int calls, fail_set, fail_sample, reset_on_set;
static unsigned soc = 10000, adapter = 6;
static int qcom_battmgr_request(struct qcom_battmgr *b, void *data, size_t len)
{
    struct qcom_battmgr_property_request *r = data;
    b->pending_opcode = r->hdr.opcode;
    assert(len == sizeof(*r)); assert(calls < 100); trace[calls++] = *r;
    if (r->hdr.opcode == BC_BATTERY_STATUS_GET && r->property == BATT_CAPACITY) {
        if (fail_sample) return -ETIMEDOUT;
        b->charge_limit_soc_raw = soc;
    }
    if (r->hdr.opcode == BC_USB_STATUS_GET && r->property == USB_ADAP_TYPE)
        b->usb.usb_type = adapter;
    if (r->hdr.opcode == BC_BATTERY_STATUS_SET) {
        if (reset_on_set) b->service_generation++;
        if (fail_set) return -EREMOTEIO;
    }
    return 0;
}
