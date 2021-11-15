TARGETS := all clean rebuild
SUBDIRS := shaders fonts

$(TARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
	
.PHONY: all $(SUBDIRS) 
