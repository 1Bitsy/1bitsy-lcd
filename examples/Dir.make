       D := examples

EXAMPLES := button line-test munch simple touch

include $(EXAMPLES:%=$D/%/Dir.make)
