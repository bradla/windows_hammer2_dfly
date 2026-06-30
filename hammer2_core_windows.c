#include "hammer2_core_windows.h"
#include "hammer2_windows_port.h"

int hammer2_core_windows_probe(void)
{
    return 1;
}

const char *hammer2_core_windows_version(void)
{
    return "HAMMER2 Windows core shim";
}
