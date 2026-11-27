#定义编译器和文件的输出目录

#export STAGING_DIR=/home/turbo/imx6/openwrt_ebyte/ebyte_verify/staging_dir/toolchain-arm_cortex-a7+neon-vfpv4_gcc-12.3.0_musl_eabi/bin:$STAGING_DIR
export STAGING_DIR=/home/wxg/dwork/apptoolchain/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin:$STAGING_DIR

ARCH ?= arm
TARGET = test_uart 


#存放中间文件的路径
BUILD_DIR = build_$(ARCH)
#存放源文件的文件夹
SRC_DIR = src
#存放头文件的文件夹
INC_DIR = inc . /home/wxg/dwork/apptoolchain/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf/include 
#源文件
SRCS = $(wildcard $(SRC_DIR)/*.c)
#目标文件（*.o）
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(SRCS)))
#头文件
DEPS = $(wildcard $(INC_DIR)/*.h)

#指定头文件的路径
CFLAGS = $(patsubst %, -I%, $(INC_DIR))
#依赖的库函数
LIB= -lpthread  -lm   -lrt -O2 -Wall -lgpiod

#根据输入的ARCH变量来选择编译器
#ARCH=x86，使用gcc
#ARCH=arm，使用arm-gcc
ifeq ($(ARCH),x86)
CC = gcc
else
CC =/home/wxg/dwork/apptoolchain/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-gcc
endif

#目标文件
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(LIB) -o $@ $^ $(CFLAGS)
	/home/wxg/dwork/rk3506app/autotxfile/scp_auto.sh ./$(BUILD_DIR)/$(TARGET) /home/wxg/dwork/rk3506app/autotxfile/scp_config.ini

#*.o文件的生成规则
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS)
#创建一个编译目录，用于存放过程文件 #命令前带“@”,表示不在终端上输出
	@mkdir -p $(BUILD_DIR)
	$(CC) -g -c -o $@ $< $(CFLAGS)

#伪目标
.PHONY: clean cleanall
#按架构删除
clean:
	rm -rf $(BUILD_DIR)

#全部删除
cleanall:
	rm -rf build_x86 build_arm
