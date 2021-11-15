include $(BGFX)/scripts/tools.mk
BIN2C:="$(BGFX)/../bx/tools/bin/$(OS)/bin2c"

roboto_regular.ttf.h: roboto-regular.ttf
	$(BIN2C) -f $(<) -o $(@) -n s_robotoRegularTtf

robotomono_regular.ttf.h: robotomono-regular.ttf
	$(BIN2C) -f $(<) -o $(@) -n s_robotoMonoRegularTtf

icons_font_awesome.ttf.h: fontawesome-webfont.ttf
	$(BIN2C) -f $(<) -o $(@) -n s_iconsFontAwesomeTtf

icons_kenney.ttf.h: kenney-icon-font.ttf
	$(BIN2C) -f $(<) -o $(@) -n s_iconsKenneyTtf

fonts: \
	roboto_regular.ttf.h \
	robotomono_regular.ttf.h \
	icons_font_awesome.ttf.h \
	icons_kenney.ttf.h

all: fonts

clean: 
	@echo Cleaning...
	@-rm -vf $(wildcard *.h)

rebuild: fonts
	@make --no-print-directory clean all

.PHONY: all clean rebuild
