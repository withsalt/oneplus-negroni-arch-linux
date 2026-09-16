date -Is
printf '\n== Whole boot journal ==\n'
journalctl -k -b -o short-monotonic --no-pager
printf '\n== DWC3 and xHCI registers ==\n'
cat /sys/kernel/debug/usb/a600000.usb/mode
cat /sys/kernel/debug/usb/a600000.usb/regdump
for f in /sys/kernel/debug/usb/xhci/*/reg-op /sys/kernel/debug/usb/xhci/*/ports/*/portsc; do echo "$f"; cat "$f"; done
printf '\n== PHY regulators and GPIOs ==\n'
cat /sys/kernel/debug/gpio
cat /sys/kernel/debug/regulator/regulator_summary
printf '\n== PHY addresses and I2C peers ==\n'
for f in /sys/bus/i2c/devices/*/name; do echo "$f"; cat "$f"; done
printf '\n== Power supply status ==\n'
for d in /sys/class/power_supply/*; do echo "$d"; cat "$d/uevent"; done
printf '\n== Relevant services and overrides ==\n'
grep -R -n -E 'usb_role|data_role|power_role|usb.*host|qcom_battmgr' /etc/systemd/system /etc/udev/rules.d /usr/local/bin /usr/local/sbin /etc/modprobe.d 2>/dev/null
