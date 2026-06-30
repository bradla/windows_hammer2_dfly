#include "hammer2_core_windows.h"
#include "hammer2_windows_port.h"
#include <stdio.h>

int main(void)
{
    printf("HAMMER2 Windows core shim: %s\n", hammer2_core_windows_version());
    printf("probe=%d\n", hammer2_core_windows_probe());
    return 0;
}
