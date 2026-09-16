date -Is
printf '\n== FSA4480 registers ==\n'
for d in /sys/kernel/debug/regmap/*; do case "$d" in *3-0042*) echo "$d"; cat "$d/registers";; esac; done
printf '\n== xHCI port status ==\n'
for f in /sys/kernel/debug/usb/xhci/*/ports/*/portsc; do echo "$f"; cat "$f"; done
printf '\n== Receiver driver availability ==\n'
modinfo hid_logitech_dj 2>&1
modinfo hid_logitech_hidpp 2>&1
modprobe --resolve-alias 'hid:b0003g0001v0000046Dp0000C52B'
printf '\n== Journal HID events ==\n'
journalctl -k -b -o short-monotonic --no-pager | grep -i -E '046d|c52b|hid|receiver|usb 1-'
printf '\n== Current devices ==\n'
cat /sys/kernel/debug/usb/devices
