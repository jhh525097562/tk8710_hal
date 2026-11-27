#定义编译器和文件的输出目录

ARCH ?= arm
TARGET = tk8710_gw


#存放中间文件的路径
BUILD_DIR = build_$(ARCH)
#存放源文件的文件夹
SRC_DIR = src
#存放头文件的文件夹
INC_DIR = inc . ../common/inc
#源文件
SRCS = $(wildcard $(SRC_DIR)/*.c)
COMMON_SRCS = ../common/src/file_logger.c
#目标文件（*.o）
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS)) $(patsubst ../common/src/%.c, $(BUILD_DIR)/%.o, $(COMMON_SRCS))
#头文件
DEPS = $(wildcard $(addsuffix /*.h,$(INC_DIR)))

#根据输入的ARCH变量来选择编译器
#ARCH=x86，使用gcc
#ARCH=arm，使用arm-gcc
ifeq ($(ARCH),x86)
CC = gcc
SYSROOT ?=
else
TOOLCHAIN_ROOT ?= $(HOME)/arm-buildroot-linux-gnueabihf_sdk-buildroot
CC = $(TOOLCHAIN_ROOT)/bin/arm-buildroot-linux-gnueabihf-gcc
SYSROOT ?= $(shell $(CC) --print-sysroot 2>/dev/null)
endif

# 运行库与链接器搜索路径
ifeq ($(SYSROOT),)
LDFLAGS = -L$(CURDIR)/inc
CFLAGS = $(patsubst %, -I%, $(INC_DIR)) -Wall
else
LDFLAGS = --sysroot=$(SYSROOT) -Wl,-rpath-link=$(SYSROOT)/lib:$(SYSROOT)/usr/lib -L$(SYSROOT)/lib -L$(SYSROOT)/usr/lib -L$(CURDIR)/inc
CFLAGS = $(patsubst %, -I%, $(INC_DIR)) -I$(SYSROOT)/usr/include -Wall
endif
LIB= -lpthread  -lm   -lrt -O2 -Wall -lgpiod
#依赖的库函数
LIB += -lipc_smp

#目标文件
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIB)
	@if [ -x /home/wxg/dwork/rk3506app/autotxfile/scp_auto.sh ]; then /home/wxg/dwork/rk3506app/autotxfile/scp_auto.sh ./$(BUILD_DIR)/$(TARGET) /home/wxg/dwork/rk3506app/autotxfile/scp_config.ini; fi
#*.o文件的生成规则
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS)
#创建一个编译目录，用于存放过程文件 #命令前带“@”,表示不在终端上输出
	@mkdir -p $(BUILD_DIR)
	$(CC) -g -c -o $@ $< $(CFLAGS)

$(BUILD_DIR)/%.o: ../common/src/%.c $(DEPS)
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
