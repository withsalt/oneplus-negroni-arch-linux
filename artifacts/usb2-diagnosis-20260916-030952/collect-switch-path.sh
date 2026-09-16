date -Is
printf '\n== Regmap directories ==\n'
ls /sys/kernel/debug/regmap
printf '\n== FSA4480 binding ==\n'
ls -l /sys/bus/i2c/devices/3-0042
printf '\n== Type-C alternate modes ==\n'
find -L /sys/class/typec -maxdepth 4 -type f \( -name active -o -name mode -o -name svid -o -name pin_assignment -o -name configuration -o -name data_role -o -name power_role -o -name role \) -print -exec cat {} \;
printf '\n== FSA4480 register access availability ==\n'
command -v i2cget
command -v i2ctransfer
ls -l /dev/i2c-*
printf '\n== Power attributes ==\n'
cat /sys/class/usb_role/*/role
cat /sys/class/typec/port0/data_role
cat /sys/class/typec/port0/power_role
