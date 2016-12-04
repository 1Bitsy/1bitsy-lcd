LIB_OFILES := src/gfx.o

lib:     src/libgfx.a

src/libgfx.a: $(LIB_OFILES)
	rm -f $@
	$(AR) cr $@ $(LIB_OFILES)

        
src/gfx.o: src/gfx.c
