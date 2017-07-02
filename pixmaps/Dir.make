	       D := pixmaps

# used in examples' makefiles
 MAKE_BUTTON_IMG := $D/make-button-img
   IMG_TO_BUTTON := $D/img-to-button

	    PROG := make-button-img
	CXXFILES := make-button-img.cpp

     $D_CXXFILES := $(CXXFILES:%=$D/%)
       $D_OFILES := $($D_CXXFILES:%.cpp=%.o)
	  $D_ELF := $D/$(PROG)

	  DFILES += $($D_CXXFILES:%.cpp=%.d)
	    DIRT += $($D_ELF) $($D_OFILES)

$D/make-button-img:          CC := $(HOSTCXX)
$D/make-button-img:     LDFLAGS := -L$(AGG_DIR)/src -L/opt/local/lib
$D/make-button-img:      LDLIBS := $(AGG_DIR)/font_freetype/agg_font_freetype.o
$D/make-button-img:      LDLIBS += -lagg -lfreetype
$D/make-button-img: TARGET_ARCH :=

$($D_OFILES):               CXX := $(HOSTCXX)
$($D_OFILES):          CPPFLAGS := -I$(AGG_DIR)/include
$($D_OFILES):          CPPFLAGS += -I$(AGG_DIR)/font_freetype
$($D_OFILES):          CPPFLAGS += -I/opt/local/include/freetype2
$($D_OFILES):       TARGET_ARCH :=
$($D_OFILES):          CXXFLAGS := -MD -g 

%-button-data.h: %-button-up.ppm %-button-down.ppm $(IMG_TO_BUTTON)
	$(IMG_TO_BUTTON) $(*F) $(wordlist 1, 2, $^) -o $@

# Define a button pixmap.
# Use: $(eval $(call define-button,Label,identifier,dependent))
# e.g., $(eval $(call define-button,Save File,save,main.o))
# Usesa free variable D.

define define-button

    $$D/$(3): $$D/$(2)-button-data.h

    $$D/${2}-button-%.ppm: $(MAKE_BUTTON_IMG)
	$(MAKE_BUTTON_IMG) "$(1)" $$* -o $$@

    DIRT += $$D/$(2)-button-data.h

endef
