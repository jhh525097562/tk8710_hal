#定义编译器和文件的输出目录

ARCH ?= arm
TARGET = web_server 

#存放中间文件的路径
BUILD_DIR = build_$(ARCH)
#存放源文件的文件夹
SRC_DIR = src
#存放头文件的文件夹
INC_DIR = inc . ../common/inc ../mqtt/inc
DEPS_ROOT ?= $(CURDIR)/../third_party/$(ARCH)
#源文件
SRCS = $(wildcard $(SRC_DIR)/*.c)
COMMON_SRCS = ../common/src/file_logger.c
#目标文件（*.o）
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(SRCS))) $(patsubst ../common/src/%.c, $(BUILD_DIR)/%.o, $(COMMON_SRCS)) $(BUILD_DIR)/gateway_telemetry.o
#头文件
DEPS = $(wildcard $(addsuffix /*.h,$(INC_DIR))) $(wildcard $(DEPS_ROOT)/include/*.h $(DEPS_ROOT)/include/*/*.h)
# 自动生成的资源头文件
GEN_HEADER = inc/web_resources.h

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

DEPS_CFLAGS =
DEPS_LDFLAGS =
ifneq ($(wildcard $(DEPS_ROOT)),)
DEPS_CFLAGS = -I$(DEPS_ROOT)/include
DEPS_LDFLAGS = -L$(DEPS_ROOT)/lib -Wl,-rpath-link=$(DEPS_ROOT)/lib
endif

# 运行库与链接器搜索路径
ifeq ($(SYSROOT),)
LDFLAGS = $(DEPS_LDFLAGS)
CFLAGS = $(patsubst %, -I%, $(INC_DIR)) $(DEPS_CFLAGS) -Wall
else
LDFLAGS = --sysroot=$(SYSROOT) -Wl,-rpath-link=$(SYSROOT)/lib:$(SYSROOT)/usr/lib -L$(SYSROOT)/lib -L$(SYSROOT)/usr/lib $(DEPS_LDFLAGS)
CFLAGS = $(patsubst %, -I%, $(INC_DIR)) -I$(SYSROOT)/usr/include $(DEPS_CFLAGS) -Wall
endif
LIB= -lpthread  -lm   -lrt -O2 -Wall -lgpiod
#依赖的库函数
LIB += -ljson-c -lmongoose

#目标文件
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIB)

# 生成资源头文件规则
$(GEN_HEADER): tools/gen_resources.py $(wildcard web_resources/html/*) $(wildcard web_resources/css/*)
	@echo "Generating web resources header..."
	python3 tools/gen_resources.py

#*.o文件的生成规则
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS) $(GEN_HEADER)
#创建一个编译目录，用于存放过程文件 #命令前带“@”,表示不在终端上输出
	@mkdir -p $(BUILD_DIR)
	$(CC) -g -c -o $@ $< $(CFLAGS)

$(BUILD_DIR)/%.o: ../common/src/%.c $(DEPS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -g -c -o $@ $< $(CFLAGS)

#伪目标
.PHONY: clean cleanall
$(BUILD_DIR)/gateway_telemetry.o: ../mqtt/src/gateway_telemetry.c ../mqtt/inc/gateway_telemetry.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -g -c -o $@ $< $(CFLAGS)
#按架构删除
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(GEN_HEADER)

#全部删除
cleanall:
	rm -rf build_x86 build_arm
