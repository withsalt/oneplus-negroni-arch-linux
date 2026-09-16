date -Is
ls -la /sys/bus/i2c/devices/3-0042/
ls -la /sys/bus/i2c/drivers/fsa4480/
cat /sys/kernel/debug/devices_deferred
cat /sys/bus/i2c/devices/3-0042/uevent
ls -l /sys/class/typec_mux/*
ls -l /sys/bus/platform/devices/a98000.i2c/driver
journalctl -b --no-pager | grep -i -E 'fsa4480|3-0042|i2cget|i2cset|unbind|usb-role-switch|sud[o].*(usb|qcom|host|echo)' | tail -45
