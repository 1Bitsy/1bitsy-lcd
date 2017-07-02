             D := examples/line-test

          PROG := line-test
        CFILES := main.c
     $D_LDLIBS := -lgfx -lm $(LDLIB_OPENCM3)
       BUTTONS := Smooth Fade Color

     $D_CFILES := $(CFILES:%=$D/%)
     $D_OFILES := $($D_CFILES:%.c=%.o)
        $D_ELF := $D/$(PROG).elf
    # LC_BUTTONS := $(shell echo "$(BUTTONS)" |  tr A-Z a-z)
    # BTN_HFILES := $(LC_BUTTONS:%=$D/%-button-data.h)

        DFILES += $($D_CFILES:%.c=%.d)
          DIRT += $($D_ELF) $($D_OFILES)
 EXAMPLE_ELVES += $($D_ELF)


$($D_ELF): LDLIBS := $($D_LDLIBS)
$($D_ELF): $($D_OFILES) $(LIBGFX)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) $(POST_LDFLAGS) -o $@

$(eval $(call define-button,Smooth,smooth,main.o))
$(eval $(call define-button,Fade,fade,main.o))
$(eval $(call define-button,Color,color,main.o))
