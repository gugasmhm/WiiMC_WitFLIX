include $(DEVKITPRO)/libogc2/wii_rules
hello.elf: hello.c
	$(CC) -O2 $(MACHDEP) -I$(LIBOGC_INC) $< $(MACHDEP) -L$(LIBOGC_LIB) -logc -lm -o $@
hello.dol: hello.elf
	elf2dol $< $@
