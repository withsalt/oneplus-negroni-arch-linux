static void test_commands(void)
{
    /* The actual SM8450 protocol must retain the previously tested IDs. */
    assert(oplus_battmgr_usb_wire_property(USB_VOOCPHY_ENABLE) == 13);
    assert(oplus_battmgr_usb_wire_property(USB_VOOCPHY_RESET_AGAIN) == 25);
    assert(oplus_battmgr_usb_wire_property(USB_SUSPEND_PMIC) == 26);
    assert(oplus_battmgr_usb_wire_property(USB_TEMP) == 49);
    for (int property = 0; property < USB_PROP_MAX; property++)
        assert(oplus_battmgr_usb_logical_property(
            oplus_battmgr_usb_wire_property(property)) == property);
    struct oplus_chg_chip chip = {0};
    struct battery_chg_dev bcdev = {.chip = &chip};
    struct qcom_battmgr b = {.lock=1, .bcdev=&bcdev, .charge_limit_supported=true, .service_up=true,
        .gauge_init_done=true, .charge_limit={.start=75,.end=80}};
    assert(!oplus_battmgr_refresh_negroni_charge_limit(&b));
    assert(trace[calls-1].value == 1); /* policy defaults off, even at 100% */
    b.charge_limit.enabled = true; b.charge_limit.cutoff = true;
    assert(!oplus_battmgr_refresh_negroni_charge_limit(&b));
    assert(b.charge_limit_applied && trace[calls-1].value == 0);
    calls=0;
    assert(!oplus_battmgr_request_property_locked(&b,BC_BATTERY_STATUS_SET,BATT_CHG_EN,1));
    assert(trace[0].value==0); /* initialization/hotplug cannot override hold */
    assert(!oplus_battmgr_request_property_locked(&b,BC_BATTERY_STATUS_SET,BATT_SEND_CHG_STATUS,1));
    assert(trace[1].value==0);
    assert(!oplus_battmgr_request_property_locked(&b,BC_USB_STATUS_SET,USB_VOOCPHY_ENABLE,1));
    assert(!oplus_battmgr_request_property_locked(&b,BC_USB_STATUS_SET,USB_VOOCPHY_RESET_AGAIN,1));
    assert(!oplus_battmgr_request_property_locked(&b,BC_USB_STATUS_SET,USB_SUSPEND_PMIC,1));
    assert(!oplus_battmgr_request_property_locked(&b,BC_USB_STATUS_SET,USB_INPUT_CURR_LIMIT,0));
    assert(calls==2); /* hold blocks fast-charge retry and retains USB input */
    assert(!oplus_battmgr_request_property_locked(&b,BC_USB_STATUS_SET,USB_INPUT_CURR_LIMIT,3000000));
    assert(trace[2].value==3000000); /* external input is retained */
    calls=0; soc=7500;
    assert(!oplus_battmgr_refresh_negroni_charge_limit(&b));
    assert(!b.charge_limit_applied && trace[calls-1].value==1);
    calls=0; soc=8000; fail_set=1;
    assert(oplus_battmgr_refresh_negroni_charge_limit(&b)==-EREMOTEIO);
    assert(b.charge_limit.cutoff && !b.charge_limit_applied);
    fail_set=0;
    assert(!oplus_battmgr_refresh_negroni_charge_limit(&b)); assert(b.charge_limit_applied);
    soc=7500; assert(!oplus_battmgr_refresh_negroni_charge_limit(&b));
    fail_sample=1;
    assert(oplus_battmgr_refresh_negroni_charge_limit(&b)==-ETIMEDOUT);
    assert(b.charge_limit.cutoff && b.charge_limit_applied);
    fail_sample=0; soc=12000;
    assert(oplus_battmgr_refresh_negroni_charge_limit(&b)==-ENODATA);
    assert(b.charge_limit.cutoff);
    b.service_up=false; b.charge_limit.enabled=false;
    assert(oplus_battmgr_refresh_negroni_charge_limit(&b)==-EAGAIN);
    b.service_up=true; assert(!oplus_battmgr_refresh_negroni_charge_limit(&b));
    assert(!b.charge_limit_applied && trace[calls-1].value==1);
    calls=0; adapter=POWER_SUPPLY_USB_TYPE_DCP; soc=8000; b.charge_limit.enabled=true;
    assert(!oplus_battmgr_refresh_negroni_charge_limit(&b));
    assert(calls==4 && trace[2].property==13 && trace[2].value==0);
    assert(trace[3].property==BATT_CHG_EN && trace[3].value==0);
    calls=0; soc=7500;
    assert(!oplus_battmgr_refresh_negroni_charge_limit(&b));
    assert(calls==3 && trace[1].value==1 && trace[2].property==13 && trace[2].value==1);
    b.service_generation++; calls=0;
    assert(!oplus_battmgr_request_property_locked(&b,BC_BATTERY_STATUS_SET,BATT_CHG_EN,1));
    assert(trace[0].value==0); /* reconnection must wait for fresh SOC */
    assert(!oplus_battmgr_refresh_negroni_charge_limit(&b));
    assert(!b.charge_limit_applied && b.charge_limit_generation==b.service_generation);
    reset_on_set=1; soc=8000;
    assert(oplus_battmgr_refresh_negroni_charge_limit(&b)==-EAGAIN);
    assert(b.charge_limit_generation != b.service_generation);
    b.ack = 0;
    b.pending_opcode = BC_BATTERY_STATUS_GET;
    b.pending_property = BATT_CAPACITY;
    struct qcom_battmgr_message response = {
        .hdr.opcode = BC_BATTERY_STATUS_GET,
        .intval = {.property = BATT_CAPACITY, .value = 8050}
    };
    size_t length = sizeof(response.hdr) + sizeof(response.intval);
    response.intval.property = BATT_STATUS;
    battmgr_callback(&b, &response, length);
    assert(b.ack == 0); /* unrelated replies cannot complete a SOC request */
    response.intval.property = BATT_CAPACITY;
    response.hdr.opcode = BC_USB_STATUS_GET;
    battmgr_callback(&b, &response, length);
    assert(b.ack == 0);
    response.hdr.opcode = BC_BATTERY_STATUS_GET;
    battmgr_callback(&b, &response, length);
    assert(b.ack == 1 && b.charge_limit_soc_raw == 8050 && b.status.percent == 80);
    b.pending_opcode = response.hdr.opcode = BC_BATTERY_STATUS_SET;
    b.pending_property = response.intval.property = BATT_CHG_EN;
    battmgr_callback(&b, &response, sizeof(response.hdr) + sizeof(__le32));
    assert(b.ack == 2 && b.error == -ENODATA);
    response.intval.result = 514;
    battmgr_callback(&b, &response, length);
    assert(b.ack == 3 && b.error == 514);
    response.intval.result = 0;
    b.pending_opcode = response.hdr.opcode = BC_USB_STATUS_GET;
    b.pending_property = response.intval.property = 10; /* SM8450 adapter subtype */
    response.intval.value = 42;
    battmgr_callback(&b, &response, length);
    assert(b.ack == 4 && bcdev.usb.usb_subtype == 42);
    b.pending_property = response.intval.property = 49; /* SM8450 USB temperature */
    response.intval.value = 270;
    battmgr_callback(&b, &response, length);
    assert(b.ack == 5 && b.usb.temp == 270);
    b.pending_opcode = response.hdr.opcode = BC_BATTERY_STATUS_GET;
    b.pending_property = response.intval.property = BATT_TEMP;
    response.intval.value = 4000;
    battmgr_callback(&b, &response, length);
    assert(b.ack == 6 && b.status.temperature == 4000);
    b.charge_limit_supported = false;
    calls = 0;
    assert(!oplus_battmgr_request_property_locked(&b, BC_USB_STATUS_SET, USB_VOOCPHY_ENABLE, 1));
    assert(calls == 1 && trace[0].property == USB_VOOCPHY_ENABLE && trace[0].value == 1);
    puts("driver command tests: power input, retry, ADSP reset, errors, PD/VOOC sequencing passed");
}
