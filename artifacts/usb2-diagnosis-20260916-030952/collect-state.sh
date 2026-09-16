date -Is
uname -a
cat /proc/cmdline
printf '\n== USB device tree via sysfs ==\n'
for d in /sys/bus/usb/devices/*; do
 [ -f "$d/idVendor" ] || continue
 echo "DEVICE $d"
 for a in idVendor idProduct product manufacturer speed bDeviceClass authorized bConfigurationValue bMaxPower power/control power/runtime_status; do [ ! -f "$d/$a" ] || { printf '%s=' "$a"; cat "$d/$a"; }; done
done
printf '\n== Type-C and USB roles ==\n'
for d in /sys/class/typec/* /sys/class/usb_role/*; do
 [ -d "$d" ] || continue
 echo "DEVICE $d"
 for a in data_role power_role port_type orientation power_operation_mode accessory_mode supports_usb_power_delivery usb_power_delivery_revision role uevent; do [ ! -f "$d/$a" ] || { printf '%s=' "$a"; cat "$d/$a"; }; done
done
printf '\n== PHY and DWC3 runtime state ==\n'
for d in /sys/bus/platform/devices/88e3000.phy /sys/bus/platform/devices/88e4000.phy /sys/bus/platform/devices/a600000.usb /sys/bus/platform/devices/a6f8800.usb /sys/class/phy/*; do
 [ -d "$d" ] || continue
 echo "DEVICE $d"
 for a in uevent power/control power/runtime_status power/runtime_active_time power/runtime_suspended_time; do [ ! -f "$d/$a" ] || { printf '%s=' "$a"; cat "$d/$a"; }; done
done
printf '\n== Input devices ==\n'
cat /proc/bus/input/devices
printf '\n== Kernel configuration ==\n'
zcat /proc/config.gz | grep -E 'CONFIG_(USB_HID|HID_GENERIC|HID_LOGITECH|HID_MICROSOFT|INPUT_EVDEV|USB_DWC3|TYPEC|PHY_QCOM|USB_XHCI)'
printf '\n== USB/HID modules ==\n'
ls /sys/bus/usb/drivers
lsmod | grep -E 'usb|hid|typec|pmic|battmgr|dwc3'
printf '\n== Debugfs USB files ==\n'
find /sys/kernel/debug/usb -maxdepth 4 -type f 2>/dev/null
printf '\n== Full kernel log ==\n'
dmesg
