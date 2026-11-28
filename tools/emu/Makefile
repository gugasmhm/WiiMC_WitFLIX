include $(DEVKITPRO)/libogc2/wii_rules
M := ../../source/mplayer
audio.elf: audio.c
	$(CC) -O2 -std=gnu99 $(MACHDEP) -DGEKKO -DHAVE_AV_CONFIG_H -I$(M) -I$(M)/ffmpeg -I$(LIBOGC_INC) $< $(MACHDEP) \
	  -Wl,--start-group $(M)/ffmpeg/libavcodec/libavcodec.a $(M)/ffmpeg/libavutil/libavutil.a -Wl,--end-group -L$(LIBOGC_LIB) -logc -lz -L$(PORTLIBS_PATH)/ppc/lib -lm -o $@
audio.dol: audio.elf
	elf2dol $< $@
