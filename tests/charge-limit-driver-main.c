int main(void)
{
    struct qcom_battmgr b = {.charge_limit_supported=true, .service_up=true,
        .gauge_init_done=true, .charge_limit={.start=75,.end=80}};
    assert(!qcom_battmgr_refresh_negroni_charge_limit(&b));
    assert(trace[calls-1].value == 1); /* policy defaults off, even at 100% */
    b.charge_limit.enabled = true; b.charge_limit.cutoff = true;
    assert(!qcom_battmgr_refresh_negroni_charge_limit(&b));
    assert(b.charge_limit_applied && trace[calls-1].value == 0);
    calls=0;
    assert(!qcom_battmgr_request_property(&b,BATTMGR_BAT_PROPERTY_SET,BATT_OPLUS_CHG_EN,1));
    assert(trace[0].value==0); /* initialization/hotplug cannot override hold */
    assert(!qcom_battmgr_request_property(&b,BATTMGR_BAT_PROPERTY_SET,BATT_OPLUS_SEND_CHG_STATUS,1));
    assert(trace[1].value==0);
    assert(!qcom_battmgr_request_property(&b,BATTMGR_USB_PROPERTY_SET,USB_OPLUS_VOOCPHY_ENABLE,1));
    assert(calls==2); /* suppress fast-charge retry while holding */
    assert(!qcom_battmgr_request_property(&b,BATTMGR_USB_PROPERTY_SET,USB_INPUT_CURR_LIMIT,3000000));
    assert(trace[2].value==3000000); /* external input is retained */
    calls=0; soc=7500;
    assert(!qcom_battmgr_refresh_negroni_charge_limit(&b));
    assert(!b.charge_limit_applied && trace[calls-1].value==1);
    calls=0; soc=8000; fail_set=1;
    assert(qcom_battmgr_refresh_negroni_charge_limit(&b)==-EREMOTEIO);
    assert(b.charge_limit.cutoff && !b.charge_limit_applied);
    fail_set=0;
    assert(!qcom_battmgr_refresh_negroni_charge_limit(&b)); assert(b.charge_limit_applied);
    soc=7500; assert(!qcom_battmgr_refresh_negroni_charge_limit(&b));
    fail_sample=1;
    assert(qcom_battmgr_refresh_negroni_charge_limit(&b)==-ETIMEDOUT);
    assert(b.charge_limit.cutoff && b.charge_limit_applied);
    fail_sample=0; soc=12000;
    assert(qcom_battmgr_refresh_negroni_charge_limit(&b)==-ENODATA);
    assert(b.charge_limit.cutoff);
    b.service_up=false; b.charge_limit.enabled=false;
    assert(qcom_battmgr_refresh_negroni_charge_limit(&b)==-EAGAIN);
    b.service_up=true; assert(!qcom_battmgr_refresh_negroni_charge_limit(&b));
    assert(!b.charge_limit_applied && trace[calls-1].value==1);
    calls=0; adapter=POWER_SUPPLY_USB_TYPE_DCP; soc=8000; b.charge_limit.enabled=true;
    assert(!qcom_battmgr_refresh_negroni_charge_limit(&b));
    assert(calls==4 && trace[2].property==USB_OPLUS_VOOCPHY_ENABLE && trace[2].value==0);
    assert(trace[3].property==BATT_OPLUS_CHG_EN && trace[3].value==0);
    calls=0; soc=7500;
    assert(!qcom_battmgr_refresh_negroni_charge_limit(&b));
    assert(calls==3 && trace[1].value==1 && trace[2].property==USB_OPLUS_VOOCPHY_ENABLE && trace[2].value==1);
    b.service_generation++; calls=0;
    assert(!qcom_battmgr_request_property(&b,BATTMGR_BAT_PROPERTY_SET,BATT_OPLUS_CHG_EN,1));
    assert(trace[0].value==0); /* reconnection must wait for fresh SOC */
    assert(!qcom_battmgr_refresh_negroni_charge_limit(&b));
    assert(!b.charge_limit_applied && b.charge_limit_generation==b.service_generation);
    reset_on_set=1; soc=8000;
    assert(qcom_battmgr_refresh_negroni_charge_limit(&b)==-EAGAIN);
    assert(b.charge_limit_generation != b.service_generation);
    puts("driver command tests: power input, retry, ADSP reset, errors, PD/VOOC sequencing passed");
}
