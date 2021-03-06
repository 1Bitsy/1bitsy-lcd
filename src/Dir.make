         D := src

    LIBGFX := $D/libgfx.a
    CFILES := button.c gfx.c lcd.c gpio.c i2c.c pixtile.c systick.c touch.c

   $D_LIBS := $(LIBGFX)
 $D_CFILES := $(CFILES:%=$D/%)
 $D_OFILES := $($D_CFILES:%.c=%.o)

    DFILES += $($D_CFILES:%.c=%.d)
      DIRT += $($D_LIBS) $($D_OFILES)


lib:	$($D_LIBS)

$($D_LIBS): $(src_OFILES)
	rm -f $@
	$(AR) cr $@ $(src_OFILES)

# XXX compile this file with optimization and DMA gets unreliable.
# Don't know why.
$D/lcd.o: CFLAGS := $(CFLAGS:-O%=-O0)
