#include "util.h"

int rand_range(int min, int max)
{
    return rand() * (1.0 / RAND_MAX) * (max - min + 1) + min;
}

time_t file_modify_time(const FILETIME* filetime)
{
    SYSTEMTIME stime;

    FileTimeToSystemTime(filetime, &stime);

    struct tm time;
    memset(&time, 0, sizeof(struct tm));
    time.tm_isdst = -1;
    time.tm_year = stime.wYear - 1900;
    time.tm_mon = stime.wMonth - 1;
    time.tm_mday = stime.wDay;
    time.tm_hour = stime.wHour;
    time.tm_min = stime.wMinute;
    time.tm_sec = stime.wSecond;

    return mktime(&time);
}

time_t wcstot_t(const wchar_t* str)
{
    int y, m, d;
    if (swscanf(str, L"%d.%d.%d", &d, &m, &y) == 3)
    {
        struct tm time;
        memset(&time, 0, sizeof(struct tm));
        time.tm_isdst = -1;
        time.tm_year = y + 100;
        time.tm_mon = m - 1;
        time.tm_mday = d;

        return mktime(&time);
    }
    return -1;
}

int create_dir_dupsafe(wchar_t* out_dir, const wchar_t* dir)
{
    wcscpy(out_dir, dir);
    int dir_attempt = 1;
    while (!CreateDirectoryW(out_dir, NULL))
    {
        DWORD err = GetLastError();
        if (err == ERROR_ALREADY_EXISTS)
            wsprintfW(out_dir, L"%s(%i)", dir, dir_attempt++);
        else
            return -1;
    }
    return 0;
}