       D := examples

EXAMPLES := munch simple touch
#EXAMPLES += line-test button # disabled for now

include $(EXAMPLES:%=$D/%/Dir.make)
