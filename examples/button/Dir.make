             D := examples/button

          PROG := button
        CFILES := main.c
     $D_LDLIBS := -lgfx -lm $(LDLIB_OPENCM3)

     $D_CFILES := $(CFILES:%=$D/%)
     $D_OFILES := $($D_CFILES:%.c=%.o)
        $D_ELF := $D/$(PROG).elf

        DFILES += $($D_CFILES:%.c=%.d)
          DIRT += $($D_ELF) $($D_OFILES)
          DIRT += $D/toggle-button.h
 EXAMPLE_ELVES += $($D_ELF)


$($D_ELF): LDLIBS := $($D_LDLIBS)
$($D_ELF): $($D_OFILES) $(LIBGFX)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) $(POST_LDFLAGS) -o $@

$D/main.o: $D/toggle-button.h

$D/toggle-button-%.ppm: $(MAKE_BUTTON_IMG)
	$(MAKE_BUTTON_IMG) Toggle $* -o $@
