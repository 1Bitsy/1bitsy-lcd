EXAMPLES := line-test simple touch

include $(EXAMPLES:%=examples/%/Make.inc)
