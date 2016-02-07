#include "debugging.hxx"

#include <windows.h>

void SetProcessPriority()
{
    if (!SetPriorityClass(
            GetCurrentProcess(), 
#ifdef _DEBUG
            //BELOW_NORMAL_PRIORITY_CLASS
            NORMAL_PRIORITY_CLASS
#else
            //BELOW_NORMAL_PRIORITY_CLASS
            //IDLE_PRIORITY_CLASS
            NORMAL_PRIORITY_CLASS
#endif
            //PROCESS_MODE_BACKGROUND_BEGIN
            ))
    {
        DWORD dwError = GetLastError();
        //if (ERROR_PROCESS_MODE_ALREADY_BACKGROUND != dwError)
            PG3_WARNING("Failed to set process priority class (error %d)\n", dwError);
    }
}
