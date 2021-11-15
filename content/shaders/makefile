include include.mk

TARGETS := all clean rebuild
SUBDIRS := tests

$(TARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
	
.PHONY: all $(SUBDIRS) 
