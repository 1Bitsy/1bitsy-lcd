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
$D/make-button: LDFLAGS :=
$D/make-button: TARGET_ARCH :=

$($D_OFILES): CXX := $(HOSTCXX)
$($D_OFILES): CPPFLAGS := -I$(AGG_DIR)/include
$($D_OFILES): TARGET_ARCH :=
$($D_OFILES): CXXFLAGS := -O3
