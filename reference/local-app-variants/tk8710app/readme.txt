8710读写寄存器测试指令：
./Test8710W_R_Reg --testreg

8710射频发送功率测试指令：
./Test8710RFTxTone 6 509100000 46

8710接收灵敏度测试指令：
./Test8710RxSensitivity 6 509100000

8710回环测试指令：
./Test8710Loopback 6 3 17 20 20 20

8710正常工作指令：
./tk8710_gw -w /userdata/ -g 0x2e --driver-log-level warn --trm-log-level info

关闭守护进程指令：
/etc/init.d/S99tdaemon_job stop

打开守护进程指令：
/etc/init.d/S99tdaemon_job start



# 扫描 0x50～0x57 的 EEPROM
i2cdetect -y 0 0x50 0x57

# 逐个地址从偏移 0x00 读取前 16 字节
for addr in 0x50 0x51 0x52 0x53 0x54 0x55 0x56 0x57; do
    echo "==== $addr ===="
    i2ctransfer -y 0 w1@$addr 0x00 r16
done

# 写入然后读取0x53（rf4的校准结果：0x025004B8）
i2ctransfer -y 0 w5@0x53 0x01 0xb8 0x04 0x50 0x02
sleep 0.02
i2ctransfer -y 0 w1@0x53 0x00 r16