/* Exercise the real notification, recovery and policy workers together. */
static struct oplus_chg_chip worker_chip;
static struct battery_chg_dev worker_bcdev;
static struct qcom_battmgr worker_battmgr;

static void reset_workers(void)
{
    worker_chip = (struct oplus_chg_chip){0};
    worker_bcdev = (struct battery_chg_dev){.chip=&worker_chip, .battmgr=&worker_battmgr};
    worker_battmgr = (struct qcom_battmgr){.bcdev=&worker_bcdev, .service_up=true,
        .charge_limit_supported=true, .gauge_init_done=true,
        .charge_limit={.start=75, .end=80, .enabled=true, .cutoff=true}};
    INIT_WORK(&worker_battmgr.enable_work, oplus_battmgr_enable_worker);
    INIT_WORK(&worker_battmgr.notification_work, oplus_battmgr_notification_worker);
    INIT_DELAYED_WORK(&worker_battmgr.charge_limit_work, oplus_battmgr_charge_limit_worker);
    INIT_DELAYED_WORK(&worker_bcdev.adsp_crash_recover_work, oplus_adsp_crash_recover_func);
    INIT_DELAYED_WORK(&worker_bcdev.chg_status_send_work, oplus_chg_status_send_adsp_work);
    INIT_DELAYED_WORK(&worker_bcdev.adsp_voocphy_err_work, oplus_adsp_voocphy_err_work);
    calls=0; fail_set=0; fail_sample=0; reset_on_set=0; fail_gauge=0;
    gauge_attempts=0; fail_usb_read=0; fail_vooc=0; down_on_gauge=false; stop_on_gauge=false;
    soc=7500; adapter=6; hw_chg_en=false; hw_vooc_enable=false; hw_input_limit=3000000;
}

static void run_work(struct work_struct *work)
{
    assert(work->pending && work->func);
    work->pending = false;
    work->func(work);
    assert(!worker_battmgr.lock);
}

static int refresh_worker_policy(void)
{
    int ret;
    mutex_lock(&worker_battmgr.lock);
    ret = oplus_battmgr_refresh_negroni_charge_limit(&worker_battmgr);
    mutex_unlock(&worker_battmgr.lock);
    return ret;
}

static void notify_worker(unsigned notification)
{
    struct qcom_battmgr_message message = {
        .hdr.opcode=BC_NOTIFY_IND, .notification=notification
    };
    oplus_battmgr_callback(&message, sizeof(message.hdr)+sizeof(message.notification),
                           &worker_battmgr);
}

static void reply_charging_status(unsigned expected)
{
    notify_worker(BC_CHG_STATUS_GET);
    run_work(&worker_bcdev.chg_status_send_work.work);
    assert(trace[calls-1].hdr.opcode==BC_BATTERY_STATUS_SET);
    assert(trace[calls-1].property==BATT_SEND_CHG_STATUS && trace[calls-1].value==expected);
}

static int vooc_enables(void)
{
    int count=0;
    for (int i=0; i<calls; i++)
        if (trace[i].hdr.opcode==BC_USB_STATUS_SET && trace[i].property==13 && trace[i].value)
            count++;
    return count;
}

static void reconnect_workers(void)
{
    qcom_battmgr_pdr_notify(&worker_battmgr, SERVREG_SERVICE_STATE_DOWN);
    qcom_battmgr_pdr_notify(&worker_battmgr, SERVREG_SERVICE_STATE_UP);
    run_work(&worker_battmgr.enable_work);
    assert(!worker_battmgr.gauge_init_done && !worker_bcdev.chg_en);
    assert(worker_bcdev.adsp_crash_recover_work.work.pending);
}

static void test_charge_status(void)
{
    reset_workers();
    assert(!refresh_worker_policy());
    assert(hw_chg_en && worker_bcdev.chg_en);
    reply_charging_status(1);
    soc=8000;
    assert(!refresh_worker_policy());
    assert(!hw_chg_en && !worker_bcdev.chg_en);
    assert(!smbchg_charging_enable(&worker_battmgr));
    assert(!hw_chg_en && !worker_bcdev.chg_en); /* use the overridden value */
    reply_charging_status(0);
    soc=7500;
    assert(!refresh_worker_policy());
    reply_charging_status(1);
    fail_usb_read=1;
    reply_charging_status(0); /* an unreadable suspend state cannot grant permission */
    fail_usb_read=0; fail_set=1;
    assert(smbchg_charging_enable(&worker_battmgr)==-EREMOTEIO);
    assert(!worker_bcdev.chg_en); /* no confirmed enable after an error */
    fail_set=0;
    reply_charging_status(0);
    assert(!refresh_worker_policy());
    assert(worker_bcdev.chg_en && hw_chg_en);
    reply_charging_status(1);
    worker_battmgr.adsp_suspended_chg=true;
    assert(!smbchg_charging_enable(&worker_battmgr));
    assert(!worker_bcdev.chg_en && !hw_chg_en);
    reply_charging_status(0);
}

static void test_usb_input(void)
{
    reset_workers(); soc=8000; adapter=POWER_SUPPLY_USB_TYPE_DCP;
    assert(!refresh_worker_policy());
    notify_worker(BC_VOOC_VBUS_ADC_ENABLE);
    run_work(&worker_battmgr.notification_work);
    assert(hw_input_limit==3000000 && !hw_chg_en);
    run_work(&worker_battmgr.charge_limit_work.work);
    assert(worker_battmgr.charge_limit_applied);
    run_work(&worker_bcdev.adsp_voocphy_err_work.work);
    assert(hw_input_limit && !hw_chg_en && !worker_bcdev.chg_en);
    assert(!vooc_enables());
    for (int i=0; i<calls; i++)
        assert(!(trace[i].hdr.opcode==BC_USB_STATUS_SET &&
                 trace[i].property==USB_INPUT_CURR_LIMIT && !trace[i].value));
    notify_worker(BC_CHG_STATUS_SET);
    run_work(&worker_battmgr.notification_work);
    assert(hw_input_limit);

    reset_workers(); worker_battmgr.charge_limit.enabled=false;
    notify_worker(BC_VOOC_VBUS_ADC_ENABLE);
    run_work(&worker_battmgr.notification_work);
    assert(!hw_input_limit); /* preserve the vendor handshake when policy is off */
    run_work(&worker_bcdev.adsp_voocphy_err_work.work);
    assert(hw_input_limit && hw_chg_en && worker_bcdev.chg_en);
}

static void test_vooc_replay(void)
{
    reset_workers(); adapter=POWER_SUPPLY_USB_TYPE_DCP;
    reconnect_workers();
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    assert(worker_battmgr.gauge_init_done);
    assert(!hw_vooc_enable && worker_battmgr.charge_limit_vooc_disabled);
    run_work(&worker_battmgr.charge_limit_work.work);
    assert(hw_chg_en && hw_vooc_enable && vooc_enables()==1);
    run_work(&worker_battmgr.charge_limit_work.work);
    assert(vooc_enables()==1); /* do not restart a successful handshake every poll */

    reset_workers(); adapter=POWER_SUPPLY_USB_TYPE_DCP;
    worker_battmgr.gauge_init_done=false;
    assert(!oplus_ap_init_adsp_gague(&worker_battmgr));
    run_work(&worker_battmgr.charge_limit_work.work); /* opposite scheduling order */
    assert(!oplus_adsp_voocphy_enable(&worker_battmgr,true));
    assert(hw_chg_en && hw_vooc_enable && vooc_enables()==1);

    reset_workers(); adapter=POWER_SUPPLY_USB_TYPE_DCP; soc=8000;
    reconnect_workers();
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    run_work(&worker_battmgr.charge_limit_work.work);
    assert(!hw_chg_en && !hw_vooc_enable && !vooc_enables());
    soc=7500;
    run_work(&worker_battmgr.charge_limit_work.work);
    assert(hw_chg_en && hw_vooc_enable && vooc_enables()==1);

    reset_workers(); adapter=POWER_SUPPLY_USB_TYPE_DCP;
    reconnect_workers();
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    worker_battmgr.charge_limit.enabled=false;
    worker_battmgr.adsp_suspended_chg=true;
    run_work(&worker_battmgr.charge_limit_work.work);
    assert(!hw_chg_en && !hw_vooc_enable && worker_battmgr.charge_limit_work.work.pending);
    worker_battmgr.adsp_suspended_chg=false;
    run_work(&worker_battmgr.charge_limit_work.work);
    assert(hw_chg_en && hw_vooc_enable); /* restoration survives a firmware veto */

    fail_vooc=1;
    assert(oplus_adsp_voocphy_enable(&worker_battmgr,true)==-EREMOTEIO);
    fail_vooc=0;
    assert(!refresh_worker_policy()); /* cached release must not hide a pending restore */
    assert(!worker_battmgr.charge_limit_vooc_disabled);
}

static void test_gauge_retry(void)
{
    reset_workers(); soc=8000; fail_gauge=1;
    reconnect_workers();
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    assert(gauge_attempts==1 && !worker_battmgr.gauge_init_done);
    assert(worker_bcdev.adsp_crash_recover_work.work.pending);
    assert(worker_bcdev.adsp_crash_recover_work.work.delay==5000);
    assert(!worker_bcdev.otg_init_work.work.pending && !vooc_enables());
    assert(refresh_worker_policy()==-EAGAIN);
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    assert(gauge_attempts==2 && worker_battmgr.gauge_init_done);
    run_work(&worker_battmgr.charge_limit_work.work);
    assert(worker_battmgr.charge_limit_applied && !hw_chg_en);

    reset_workers(); worker_battmgr.charge_limit.enabled=false; fail_gauge=1;
    reconnect_workers();
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    run_work(&worker_battmgr.charge_limit_work.work);
    assert(gauge_attempts==2 && worker_battmgr.gauge_init_done && hw_chg_en);

    reset_workers(); fail_gauge=1;
    reconnect_workers();
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    worker_battmgr.stopping=true;
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    assert(gauge_attempts==1 && !worker_bcdev.adsp_crash_recover_work.work.pending);

    reset_workers(); fail_gauge=1; down_on_gauge=true;
    reconnect_workers();
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    assert(!worker_battmgr.gauge_init_done && !worker_bcdev.adsp_crash_recover_work.work.pending);
    down_on_gauge=false;
    qcom_battmgr_pdr_notify(&worker_battmgr,SERVREG_SERVICE_STATE_UP);
    run_work(&worker_battmgr.enable_work);
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    assert(gauge_attempts==2 && worker_battmgr.gauge_init_done);

    reset_workers(); stop_on_gauge=true;
    reconnect_workers();
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    assert(!worker_battmgr.gauge_init_done && !worker_bcdev.adsp_crash_recover_work.work.pending);

    reset_workers(); reset_on_set=1;
    reconnect_workers();
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    assert(!worker_battmgr.gauge_init_done && worker_bcdev.adsp_crash_recover_work.work.pending);
    reset_on_set=0;
    run_work(&worker_bcdev.adsp_crash_recover_work.work);
    assert(gauge_attempts==2 && worker_battmgr.gauge_init_done);
}

int main(int argc, char **argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"commands")) test_commands();
    else if (!strcmp(argv[1],"charge-status")) test_charge_status();
    else if (!strcmp(argv[1],"usb-input")) test_usb_input();
    else if (!strcmp(argv[1],"vooc-replay")) test_vooc_replay();
    else if (!strcmp(argv[1],"gauge-retry")) test_gauge_retry();
    else return 2;
    printf("driver regression: %s passed\n",argv[1]);
}
