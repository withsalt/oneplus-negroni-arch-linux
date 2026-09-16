/* Test harness for the actual driver command/evaluation functions. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#include "../linux/drivers/power/supply/qcom_battmgr_charge_limit.h"
typedef uint32_t u32;
#define cpu_to_le32(x) (x)
#define READ_ONCE(x) (x)
#define WRITE_ONCE(x,v) ((x)=(v))
#define lockdep_assert_held(x) ((void)(x))
#define dev_info(...) ((void)0)
#define power_supply_changed(x) ((void)(x))
#define PMIC_GLINK_OWNER_BATTMGR 32778
#define PMIC_GLINK_REQ_RESP 1
#define BATTMGR_BAT_PROPERTY_GET 0x30
#define BATTMGR_BAT_PROPERTY_SET 0x31
#define BATTMGR_USB_PROPERTY_GET 0x32
#define BATTMGR_USB_PROPERTY_SET 0x33
#define BATT_OPLUS_CHG_EN 24
#define BATT_OPLUS_SEND_CHG_STATUS 37
#define BATT_CAPACITY 4
#define USB_ADAP_TYPE 7
#define USB_INPUT_CURR_LIMIT 5
#define USB_OPLUS_VOOCPHY_ENABLE 13
#define POWER_SUPPLY_USB_TYPE_DCP 2
struct pmic_glink_hdr { u32 owner, type, opcode; };
struct qcom_battmgr_property_request { struct pmic_glink_hdr hdr; u32 battery, property, value; };
struct qcom_battmgr {
    int lock, dev, bat_psy;
    bool charge_limit_supported, service_up, gauge_init_done;
    struct qcom_battmgr_charge_limit charge_limit;
    bool charge_limit_applied, charge_limit_applied_valid, charge_limit_vooc_disabled;
    bool adsp_suspended_chg;
    unsigned charge_limit_generation, service_generation, pending_property, charge_limit_soc_raw;
    struct { unsigned adap_type; } usb;
};
static struct qcom_battmgr_property_request trace[100];
static int calls, fail_set, fail_sample, reset_on_set;
static unsigned soc = 10000, adapter = 6;
static int qcom_battmgr_request(struct qcom_battmgr *b, void *data, size_t len)
{
    struct qcom_battmgr_property_request *r = data;
    assert(len == sizeof(*r)); assert(calls < 100); trace[calls++] = *r;
    if (r->hdr.opcode == BATTMGR_BAT_PROPERTY_GET && r->property == BATT_CAPACITY) {
        if (fail_sample) return -ETIMEDOUT;
        b->charge_limit_soc_raw = soc;
    }
    if (r->hdr.opcode == BATTMGR_USB_PROPERTY_GET && r->property == USB_ADAP_TYPE)
        b->usb.adap_type = adapter;
    if (r->hdr.opcode == BATTMGR_BAT_PROPERTY_SET) {
        if (reset_on_set) b->service_generation++;
        if (fail_set) return -EREMOTEIO;
    }
    return 0;
}
