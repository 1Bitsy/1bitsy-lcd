EXAMPLES := button line-test munch simple touch

include examples/pixmaps/Dir.make
include $(EXAMPLES:%=examples/%/Dir.make)
