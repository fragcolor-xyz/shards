TARGETS := all clean rebuild
SUBDIRS := bump cubes instancing

$(TARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: all $(SUBDIRS) 
