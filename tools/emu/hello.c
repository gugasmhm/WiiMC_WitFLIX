#include <gccore.h>
int main(void) { SYS_Report("EMUTEST HELLO %d\n", 42); SYS_Report("EMUTEST FIM\n"); for (;;) VIDEO_WaitVSync(); }
