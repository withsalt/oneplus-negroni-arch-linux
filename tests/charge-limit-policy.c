/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "../linux/drivers/power/supply/qcom_battmgr_charge_limit.h"

int main(void)
{
    struct qcom_battmgr_charge_limit p = { .start = 75, .end = 80 };
    unsigned int soc;
    /* Fresh firmware must never limit, including above the configured end. */
    for (soc = 0; soc <= 100; soc++) {
        qcom_battmgr_charge_limit_sample(&p, soc, true);
        assert(!p.cutoff);
    }
    p.enabled = true;
    p.cutoff = true;
    qcom_battmgr_charge_limit_sample(&p, 78, true);
    assert(p.cutoff); /* conservative first activation within the band */
    qcom_battmgr_charge_limit_sample(&p, 75, true);
    assert(!p.cutoff); /* user's inclusive lower boundary */
    for (soc = 76; soc < 80; soc++) {
        qcom_battmgr_charge_limit_sample(&p, soc, true);
        assert(!p.cutoff);
    }
    qcom_battmgr_charge_limit_sample(&p, 80, true);
    assert(p.cutoff);
    qcom_battmgr_charge_limit_sample(&p, 79, true);
    assert(p.cutoff);
    qcom_battmgr_charge_limit_sample(&p, 75, true);
    assert(!p.cutoff);
    qcom_battmgr_charge_limit_sample(&p, 0, false);
    assert(p.cutoff); /* stale/failed reading must not enable charging */
    qcom_battmgr_charge_limit_sample(&p, 101, true);
    assert(p.cutoff);
    qcom_battmgr_charge_limit_sample(&p, 0, true);
    assert(!p.cutoff); /* zero is a legitimate fresh SOC */
    p.start = 40; p.end = 60;
    qcom_battmgr_charge_limit_sample(&p, 60, true); assert(p.cutoff);
    qcom_battmgr_charge_limit_sample(&p, 50, true); assert(p.cutoff);
    qcom_battmgr_charge_limit_sample(&p, 40, true); assert(!p.cutoff);
    p.enabled = false; p.cutoff = true;
    qcom_battmgr_charge_limit_sample(&p, 100, false); assert(!p.cutoff);
    assert(qcom_battmgr_charge_limit_valid(0, 100));
    assert(qcom_battmgr_charge_limit_valid(99, 100));
    assert(!qcom_battmgr_charge_limit_valid(80, 80));
    assert(!qcom_battmgr_charge_limit_valid(81, 80));
    assert(!qcom_battmgr_charge_limit_valid(75, 101));
    assert(!qcom_battmgr_charge_limit_valid((unsigned)-1, 80));
    puts("charge-limit policy: boundaries, hysteresis, invalid samples, disable passed");
}
