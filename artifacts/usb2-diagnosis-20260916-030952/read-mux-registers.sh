date -Is
printf '\n== FSA4480 device ID register 0x00 ==\n'
timeout 5 i2ctransfer -f -y 3 w1@0x42 0x00 r1
printf '\n== FSA4480 switch enable/select registers 0x04/0x05 ==\n'
timeout 5 i2ctransfer -f -y 3 w1@0x42 0x04 r2
printf '\n== FSA4480 switch status registers 0x06/0x07 ==\n'
timeout 5 i2ctransfer -f -y 3 w1@0x42 0x06 r2
