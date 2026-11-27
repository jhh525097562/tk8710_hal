#定义编译器和文件的输出目录

ARCH ?= arm
APP ?= server
TARGET = $(if $(filter $(APP),client),ipc_client_demo,$(if $(filter $(APP),acm_server),acm_server_demo,ipc_server_demo))


#存放中间文件的路径
BUILD_DIR = build_$(ARCH)
#存放源文件的文件夹
SRC_DIR = src
#存放头文件的文件夹
INC_DIR = inc . /home/wxg/dwork/apptoolchain/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf/include 
#源文件
COMMON_SRCS = $(SRC_DIR)/ipc_command_client.c $(SRC_DIR)/ipc_command_server.c
SERVER_SRCS = $(SRC_DIR)/ipc_server_demo.c
ACM_SERVER_SRCS = $(SRC_DIR)/acm_server_demo.c
CLIENT_SRCS = $(SRC_DIR)/ipc_client_demo.c
SRCS = $(COMMON_SRCS) $(if $(filter $(APP),client),$(CLIENT_SRCS),$(if $(filter $(APP),acm_server),$(ACM_SERVER_SRCS),$(SERVER_SRCS)))
#目标文件（*.o）
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(SRCS)))
#头文件
DEPS = $(wildcard $(INC_DIR)/*.h)

SYSROOT = /home/wxg/dwork/rk3506sdk/rk3506_linux6.1_sdk_v1.2.0/buildroot/output/rockchip_hd_rk3506b_iot_emmc/host/arm-buildroot-linux-gnueabihf/sysroot


# 运行库与链接器搜索路径
LDFLAGS = --sysroot=$(SYSROOT) -Wl,-rpath-link=$(SYSROOT)/lib:$(SYSROOT)/usr/lib -L$(SYSROOT)/lib -L$(SYSROOT)/usr/lib -L$(CURDIR)/inc
LIB= -lpthread  -lm   -lrt -O2 -Wall -lgpiod
#指定头文件的路径
CFLAGS = $(patsubst %, -I%, $(INC_DIR))  -I$(SYSROOT)/usr/include -Wall
#依赖的库函数
LIB += -lmosquitto -ljson-c  -lmongoose  -lmbedcrypto

ifeq ($(APP),client)
LIB +=
endif

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
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIB)
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
