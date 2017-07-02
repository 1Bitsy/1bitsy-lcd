            AR := arm-none-eabi-ar
            CC := arm-none-eabi-gcc
        HOSTCC := cc
       HOSTCXX := c++
        HOSTLD := c++

    SUBMODULES := libopencm3
   OPENCM3_DIR := submodules/libopencm3

      CPPFLAGS := -DSTM32F4
      CPPFLAGS += -Isrc -Iinclude -I$(OPENCM3_DIR)/include
   TARGET_ARCH := -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16
        CFLAGS := -MD -std=gnu99                                        \
                  -Wall -Wundef -Wextra -Wshadow -Werror                \
                  -Wimplicit-function-declaration -Wredundant-decls     \
                  -Wmissing-prototypes -Wstrict-prototypes              \
                  -g -O3
       LDFLAGS := --static -nostartfiles                                \
                  -Lsrc -L$(OPENCM3_DIR)/lib                            \
                  -Tstm32f4-1bitsy.ld -Wl,--gc-sections
  POST_LDFLAGS += -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group
 LDLIB_OPENCM3 := -lopencm3_stm32f4

# Included makefiles populate these.
        DFILES :=
          DIRT := 
 EXAMPLE_ELVES :=


all: opencm3 lib examples

include src/Dir.make
#include pixmaps/Dir.make
include examples/Dir.make

clean:
	$(RM) -r $(DIRT) $(DFILES)

realclean: clean
	$(MAKE) -C $(OPENCM3_DIR) clean

opencm3:
#       # XXX libopencm3 can't stop rebuilding the world.
	@ [ -f $(OPENCM3_DIR)/lib/libopencm3_stm32f4.a ] || \
	  $(MAKE) -C $(OPENCM3_DIR) TARGETS=stm32/f4

examples: $(EXAMPLE_ELVES)

ifeq ($(wildcard $(OPENCM3_DIR)/*),)
    missing_submodule := libopencm3
endif

ifdef missing_submodule
    # Hack: newline variable
    # https://stackoverflow.com/questions/17055773
    define n


    endef
    $(error $(missing_submodule) submodule is not initialized.$n\
            please run:$n\
            $$ git submodule init$n\
            $$ git submodule update$n\
            before running make)
endif

-include $(DFILES)
