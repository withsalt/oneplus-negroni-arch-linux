/* Test harness for the actual driver command/evaluation functions. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
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
#define lockdep_assert_held(x) assert(*(x) == 1)
#define mutex_lock(x) do { assert(*(x) == 0); *(x) = 1; } while (0)
#define mutex_unlock(x) do { assert(*(x) == 1); *(x) = 0; } while (0)
#define spin_lock_irqsave(x,flags) do { (flags) = 0; mutex_lock(x); } while (0)
#define spin_unlock_irqrestore(x,flags) do { (void)(flags); mutex_unlock(x); } while (0)
#define dev_info(...) ((void)0)
#define dev_err(...) ((void)0)
#define dev_warn_ratelimited(...) ((void)0)
#define chg_err(...) ((void)0)
#define printk(...) ((void)0)
#define pr_info(...) ((void)0)
#define power_supply_changed(x) ((void)(x))
#define pm_wakeup_dev_event(...) ((void)0)
#define usleep_range(...) ((void)0)
#define PMIC_GLINK_OWNER_BATTMGR 32778
#define PMIC_GLINK_REQ_RESP 1
#define PMIC_GLINK_NOTIFY 2
#define SERVREG_SERVICE_STATE_UP 1
#define SERVREG_SERVICE_STATE_DOWN 0
#define POWER_SUPPLY_USB_TYPE_DCP 2
#define POWER_SUPPLY_TYPE_UNKNOWN 0
#define POWER_SUPPLY_STATUS_NOT_CHARGING 3
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define BIT(n) (1UL << (n))
#define system_wq 0
#define msecs_to_jiffies(ms) (ms)
#define round_jiffies_relative(j) (j)
#define container_of(ptr,type,member) ((type *)((char *)(ptr) - offsetof(type,member)))
#define to_delayed_work(ptr) container_of(ptr,struct delayed_work,work)
struct work_struct {
    void (*func)(struct work_struct *);
    bool pending;
    unsigned delay;
};
struct delayed_work { struct work_struct work; };
#define INIT_WORK(w,fn) (*(w) = (struct work_struct){.func=(fn)})
#define INIT_DELAYED_WORK(w,fn) INIT_WORK(&(w)->work,fn)
static int schedule_work(struct work_struct *w)
{
    bool was_pending = w->pending;
    w->pending = true;
    return !was_pending;
}
static int mod_delayed_work(int wq, struct delayed_work *w, unsigned delay)
{
    (void)wq;
    w->work.delay = delay;
    return schedule_work(&w->work);
}
static int schedule_delayed_work(struct delayed_work *w, unsigned delay)
{
    if (w->work.pending) return 0;
    return mod_delayed_work(0,w,delay);
}
static void (*cancel_hook)(struct work_struct *w);
static void cancel_work_sync(struct work_struct *w)
{
    if (cancel_hook) cancel_hook(w);
    w->pending = false;
}
static void cancel_delayed_work_sync(struct delayed_work *w) { cancel_work_sync(&w->work); }
static unsigned long test_xchg(unsigned long *p, unsigned long v)
{
    unsigned long old = *p;
    *p = v;
    return old;
}
#define xchg(p,v) test_xchg(p,v)
#define set_bit(n,p) (*(p) |= BIT(n))
struct pmic_glink_hdr { u32 owner, type, opcode; };
/* SHARED_STRUCTURES */
struct oplus_chg_chip {
    bool adsp_notify_ap_suspend;
    int charger_type;
    struct delayed_work check_pd_svooc_work;
};
struct test_vooc_chip { bool adspvoocphy_fastchg_start; };
struct battery_chg_dev {
    struct { unsigned usb_in_status, usb_suspend_pmic, usb_subtype; } usb;
    struct oplus_chg_chip *chip;
    struct test_vooc_chip *vooc_chip;
    struct qcom_battmgr *battmgr;
    bool chg_en, adsp_voocphy_err_check, hvdcp_detect_ok, pd_svooc, otg_online;
    int real_chg_type;
    struct delayed_work chg_status_send_work, adsp_voocphy_err_work;
    struct delayed_work adsp_crash_recover_work, otg_init_work;
    struct delayed_work usb_type_work, otg_vbus_enable_work, plugin_irq_work;
    unsigned curr_thermal_level, num_thermal_levels;
};
/* QC type detection is unrelated to the VOOC/limit notifications under test. */
static int opchg_get_charger_type(struct battery_chg_dev *b) { (void)b; return 0; }
static int oplus_chg_get_charger_subtype(struct battery_chg_dev *b) { (void)b; return 0; }
struct qcom_battmgr_property_request { struct pmic_glink_hdr hdr; u32 battery, property, value; };
struct power_supply { struct qcom_battmgr *drvdata; };
union power_supply_propval { int intval; const char *strval; };
#define power_supply_get_drvdata(psy) ((psy)->drvdata)
struct qcom_battmgr {
    int lock, event_lock, dev, bat_psy, usb_psy, wls_psy, ack, error, charge_limit_error;
    bool stopping;
    struct work_struct enable_work, notification_work;
    struct delayed_work charge_limit_work;
    unsigned long pending_notifications;
    struct battery_chg_dev *bcdev;
    unsigned pending_opcode;
    bool charge_limit_supported, service_up, gauge_init_done, invert_current;
    struct qcom_battmgr_charge_limit charge_limit;
    bool charge_limit_applied, charge_limit_applied_valid, charge_limit_vooc_disabled;
    bool adsp_suspended_chg;
    unsigned charge_limit_generation, service_generation, pending_property, charge_limit_soc_raw;
    struct qcom_battmgr_usb usb;
    struct qcom_battmgr_info info;
    struct qcom_battmgr_status status;
    struct qcom_battmgr_wireless wireless;
};
static struct qcom_battmgr_property_request trace[256];
static int calls, fail_set, fail_sample, reset_on_set;
static int fail_gauge, gauge_attempts, fail_usb_read, fail_vooc;
static bool down_on_gauge, stop_on_gauge;
static unsigned soc = 10000, adapter = 6;
static u32 hw_input_limit = 3000000;
static bool hw_chg_en, hw_vooc_enable;
static int qcom_battmgr_request(struct qcom_battmgr *b, void *data, size_t len)
{
    struct qcom_battmgr_property_request *r = data;
    assert(b->lock == 1);
    if (b->stopping || !b->service_up) return -EAGAIN;
    b->pending_opcode = r->hdr.opcode;
    assert(calls < 256);
    if (r->hdr.opcode == BC_SET_NOTIFY_REQ) {
        assert(len == sizeof(struct qcom_battmgr_enable_request));
        trace[calls++] = (struct qcom_battmgr_property_request){.hdr=r->hdr};
        return 0;
    }
    assert(len == sizeof(*r)); trace[calls++] = *r;
    if (r->hdr.opcode == BC_BATTERY_STATUS_GET && r->property == BATT_CAPACITY) {
        if (fail_sample) return -ETIMEDOUT;
        b->charge_limit_soc_raw = soc;
    }
    if (r->hdr.opcode == BC_USB_STATUS_GET && r->property == USB_ADAP_TYPE)
        b->usb.usb_type = adapter;
    if (r->hdr.opcode == BC_USB_STATUS_GET && r->property == 26) {
        if (fail_usb_read) return -ETIMEDOUT;
        b->bcdev->usb.usb_suspend_pmic = hw_input_limit == 0;
    }
    if (r->hdr.opcode == BC_USB_STATUS_SET && r->property == USB_INPUT_CURR_LIMIT)
        hw_input_limit = r->value;
    if (r->hdr.opcode == BC_USB_STATUS_SET && r->property == 13) {
        if (fail_vooc) return -EREMOTEIO;
        hw_vooc_enable = r->value;
    }
    if (r->hdr.opcode == BC_BATTERY_STATUS_SET) {
        if (r->property == BATT_ADSP_GAUGE_INIT) {
            gauge_attempts++;
            if (down_on_gauge) { b->service_up = false; b->service_generation++; }
            if (stop_on_gauge) b->stopping = true;
            if (fail_gauge) { fail_gauge--; return -ETIMEDOUT; }
        }
        if (reset_on_set) b->service_generation++;
        if (fail_set) return -EREMOTEIO;
        if (r->property == BATT_CHG_EN) hw_chg_en = r->value;
    }
    return 0;
}
