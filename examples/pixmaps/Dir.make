D := examples/pixmaps

   # used in examples' makefiles
   MAKE_BUTTON := $D/make-button

          PROG := make-button
      CXXFILES := make-button.cpp

   $D_CXXFILES := $(CXXFILES:%=$D/%)
     $D_OFILES := $($D_CXXFILES:%.cpp=%.o)
        $D_ELF := $D/$(PROG)

        DFILES += $($D_CXXFILES:%.cpp=%.d)
          DIRT += $($D_ELF) $($D_OFILES)

$D/make-button: CC := $(HOSTCXX)
$D/make-button: LDFLAGS := -L$(AGG_DIR)/src -L/opt/local/lib
$D/make-button: LDLIBS := $(AGG_DIR)/font_freetype/agg_font_freetype.o
$D/make-button: LDLIBS += -lagg -lfreetype
$D/make-button: TARGET_ARCH :=

$($D_OFILES): CXX := $(HOSTCXX)
$($D_OFILES): CPPFLAGS := -I$(AGG_DIR)/include -I$(AGG_DIR)/font_freetype
$($D_OFILES): CPPFLAGS += -I/opt/local/include/freetype2
$($D_OFILES): TARGET_ARCH :=
$($D_OFILES): CXXFLAGS := -g 
