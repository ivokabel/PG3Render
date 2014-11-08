#include "debugging.hxx"

#include <windows.h>

void SetProcessPriority()
{
    if (!SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS)) //PROCESS_MODE_BACKGROUND_BEGIN))
    {
        DWORD dwError = GetLastError();
        //if (ERROR_PROCESS_MODE_ALREADY_BACKGROUND != dwError)
            PG3_WARNING("Failed to set process priority class (error %d)\n", dwError);
    }
}
