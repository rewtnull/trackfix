Trackfix is a small tool to force reinitialization of Synaptics devices. This will hopefully fix frozen TrackPoint on devices such as ThinkPad T14, P14s, X13 series etc. without requiring a reboot. This is for Windows computers. I have not encountered this issue in Linux so I'm unsure if a workaround is even needed there.

Just run the tool and your frozen TrackPoint device should be found and reinitialized.

Or build it yourself.

**MSYS2 MinGW-w64 GCC:**

PS C:\\> Make

Or

gcc -std=c11 -O2 -Wall -Wextra -Werror trackfix.c -o trackfix.exe -lole32 -loleaut32 -luuid -luser32

You will need gcc and make installed through msys2 and in $PATH.

**Visual Studio**

Use the included project files

Or through x64 Native Tools Command Prompt:

cl /nologo /TC /std:c11 /O2 /W4 /WX /Fe:trackfix.exe trackfix.c ole32.lib oleaut32.lib uuid.lib

Compile for the architecture of the registered Synaptics COM server.

This tool is **FREEWARE**. Do with it as you wish. Or don't. I'm not your mom.

By Marcus Hoffren 2026
