AR := arm-none-eabi-ar
CC := arm-none-eabi-gcc

OPENCM3_DIR := submodules/libopencm3

CPPFLAGS := -DSTM32F4
CPPFLAGS += -Isrc -Iinclude -I$(OPENCM3_DIR)/include

CFLAGS   := $(CPPFLAGS) -MD -std=gnu99 -Wall -Wundef -Werror
CFLAGS   += -O3 -g
CFLAGS   += -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16

all: opencm3 lib examples

opencm3:
#       # libopencm3 can't stop rebuilding the world.
	[ -f $(OPENCM3_DIR)/lib/libopencm3_stm32f4.a ] || \
	$(MAKE) -C $(OPENCM3_DIR) TARGETS=stm32/f4

include src/Make.inc
include examples/Make.inc

