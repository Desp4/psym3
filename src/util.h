#ifndef PSYM_UTIL
#define PSYM_UTIL

#include <time.h>
#include <wchar.h>
#include <Windows.h>

int rand_range(int min, int max);
time_t file_modify_time(const FILETIME *filetime);
time_t wcstot_t(const wchar_t *str);
int create_dir_dupsafe(wchar_t *out_dir, const wchar_t *dir);

#endif
