EXAMPLES := line-test simple touch

include $(EXAMPLES:%=examples/%/Dir.make)
