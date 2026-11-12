Trackfix is a small tool to force reinitialization of Synaptics devices. This will hopefully fix frozen TrackPoint on devices such as ThinkPad T14, P14s, X13 series etc. without requiring a reboot.

Just run the tool and hopefully your frozen device will be found and reinitialized.

Or build it yourself.

**MinGW-w64 GCC:**

gcc -std=c11 -O2 -Wall -Wextra -Werror trackfix.c -o trackfix.exe -lole32 -loleaut32 -luuid -luser32

**VS x64 Native Tools Command Prompt:**

cl /nologo /TC /std:c11 /O2 /W4 /WX /Fe:trackfix.exe trackfix.c ole32.lib oleaut32.lib uuid.lib

Compile for the architecture of the registered Synaptics COM server.

This tool is **FREEWARE**. Do with it as you wish. Or don't. I'm not your mom.

(c)2026 Marcus Hoffren
