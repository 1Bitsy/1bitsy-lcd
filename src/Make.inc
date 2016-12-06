LIB := src/libgfx.a

LIB_OFILES := src/gfx.o src/lcd.o

DIRT += $(LIB) $(LIB_OFILES)

lib:     src/libgfx.a

$(LIB): $(LIB_OFILES)
	rm -f $@
	$(AR) cr $@ $(LIB_OFILES)

        
# src/gfx.o: src/gfx.c
