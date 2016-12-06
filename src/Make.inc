        LIB := src/libgfx.a

     CFILES := gfx.c lcd.c gpio.c pixtile.c

 LIB_CFILES := $(CFILES:%=src/%)
 LIB_OFILES := $(LIB_CFILES:%.c=%.o)

     DFILES += $(LIB_CFILES:%.c=%.d)
       DIRT += $(LIB) $(LIB_OFILES)


lib:	src/libgfx.a

$(LIB):	$(LIB_OFILES)
	rm -f $@
	$(AR) cr $@ $(LIB_OFILES)

