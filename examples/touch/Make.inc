             D := examples/touch

          PROG := touch
        CFILES := main.c
     $D_LDLIBS := -lgfx -lm $(LDLIB_OPENCM3)

     $D_CFILES := $(CFILES:%=$D/%)
     $D_OFILES := $($D_CFILES:%.c=%.o)
        $D_ELF := $D/$(PROG).elf

        DFILES += $($D_CFILES:%.c=%.d)
          DIRT += $($D_ELF) $($D_OFILES)
 EXAMPLE_ELVES += $($D_ELF)


$($D_ELF): LDLIBS := $($D_LDLIBS)
$($D_ELF): $($D_OFILES) $(LIBGFX)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) $(POST_LDFLAGS) -o $@
