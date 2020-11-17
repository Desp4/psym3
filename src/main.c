#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "opt_parser.h"
#include "util.h"

/*
* ===FILE STRUCTURE===
* 5b: "PSYM"
* 1b: unit size
* 1b: extension count
* Sb: extensions
* 1b: directory count
* Sb: directories(full paths)
* 4b: position in file
* Ub: units
* 
* Sb = 2b: length without null term, 2b(wchar_t) * length: string without null term
* Ub = 1b: unit size, 8b(time_t): date, Fb: files
* Fb = 1b: directory index, 1b: extension index, Sb: filename
*/

#define DEF_UNIT_SIZE 5
static const wchar_t* _DEF_EXTENSIONS[] = { L"jpg", L"jpeg", L"png" };

typedef struct
{
    wchar_t *name;
    time_t date;
    uint8_t ext;
    uint8_t dir_ind;
} psym_file;

static int log_err_and_return(const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    fwprintf(stderr, format, args);
    va_end(args);
    return -1;
}

static void strip_file_extension(wchar_t *name)
{
    for (int i = wcslen(name) - 1; i; --i)
    {
        if (name[i] == L'.')
        {
            name[i] = '\0';
            return;
        }
    }
}

static void sort_quick_desc(psym_file *arr, int n)
{
    if (n < 2) return;

    const time_t pivot = arr[n / 2].date;
    int i = 0, j = n - 1;
    while (1)
    {
        while (arr[i].date > pivot) ++i;
        while (arr[j].date < pivot) --j;
        if (i >= j) break;

        const psym_file tmp = arr[i];
        arr[i++] = arr[j];
        arr[j--] = tmp;
    }

    sort_quick_desc(arr, i);
    sort_quick_desc(arr + i, n - i);
}

static void write_wstr_to_file(FILE *file, const wchar_t *str)
{
    const uint16_t len = wcslen(str);
    fwrite(&len, sizeof len, 1, file);
    fwrite(str, sizeof(wchar_t), len, file);
}

static void read_wstrs_w_lengths(wchar_t** str_arr, uint16_t* len_arr, int count, FILE* file)
{
    for (int i = 0; i < count; ++i)
    {
        fread(len_arr + i, sizeof(uint16_t), 1, file);
        str_arr[i] = (wchar_t *)malloc(sizeof(wchar_t) * (len_arr[i] + 1));
        str_arr[i][len_arr[i]] = L'\0';
        fread(str_arr[i], sizeof(wchar_t), len_arr[i], file);
    }
}

static int write_bin(const wchar_t **dirs, uint8_t dir_count, const wchar_t** exts, uint8_t ext_count, 
                     const wchar_t* output, uint8_t unit_size, const psym_file *files, int file_count)
{
    FILE *file = _wfopen(output, L"wb");
    if (!file)
        return log_err_and_return(L"could not open file %s\n", output);

    fwrite("PSYM3", sizeof(char), 5, file);
    fwrite(&unit_size, sizeof unit_size, 1, file);

    fwrite(&ext_count, sizeof ext_count, 1, file);
    for (int i = 0; i < ext_count; ++i)
        write_wstr_to_file(file, exts[i]);

    fwrite(&dir_count, sizeof dir_count, 1, file);
    for (int i = 0; i < dir_count; ++i)
        write_wstr_to_file(file, dirs[i]);

    uint32_t file_iter = ftell(file) + sizeof file_iter;
    fwrite(&file_iter, sizeof file_iter, 1, file);

    for (int i = 0; i < file_count; i += unit_size)
    {
        const uint8_t unit_size_curr = (file_count - i) > unit_size ? unit_size : file_count - i;
        fwrite(&unit_size_curr, sizeof unit_size_curr, 1, file);
        fwrite(&files[i].date, sizeof(time_t), 1, file);
        for (int j = 0; j < unit_size_curr; ++j)
        {
            fwrite(&files[i + j].dir_ind, sizeof(uint8_t), 1, file);
            fwrite(&files[i + j].ext, sizeof(uint8_t), 1, file);
            write_wstr_to_file(file, files[i + j].name);
        }
    }
    fclose(file);
    return 0;
}

static int gen(const wchar_t **dirs, uint8_t dir_count, const wchar_t** exts, uint8_t ext_count,
               const wchar_t *output, uint8_t unit_size, time_t bound_lower, time_t bound_upper)
{
    for (int i = 0; i < dir_count; ++i)
    {
        DWORD attribs = GetFileAttributesW(dirs[i]);
        if (attribs == INVALID_FILE_ATTRIBUTES || !(attribs & FILE_ATTRIBUTE_DIRECTORY))
            return log_err_and_return(L"could not find directory %s\n", dirs[i]);
    }

    wchar_t dir_path[MAX_PATH];
    int alloc_size = 512;
    int file_count = 0;
    psym_file *files = (psym_file *)malloc(alloc_size * sizeof(psym_file));
    memset(files, 0, sizeof(psym_file) * alloc_size);

    // get files
    for (uint8_t i = 0; i < dir_count; ++i)
    {
        wprintf(L"directory %s\n", dirs[i]);
        const int dir_beg_count = file_count;
        for (uint8_t ext = 0; ext < ext_count; ++ext)
        {
            wprintf(L"\t.%s files: ", exts[ext]);
            const int ext_beg_count = file_count;

            WIN32_FIND_DATAW find_data;
            HANDLE find;

            wsprintfW(dir_path, L"%s\\*.%s", dirs[i], exts[ext]);

            if ((find = FindFirstFileW(dir_path, &find_data)) == INVALID_HANDLE_VALUE)
            {
                wprintf(L"0\n");
                continue;
            }

            do
            {
                time_t modify_time = file_modify_time(&find_data.ftLastWriteTime);
                if (modify_time > bound_lower && modify_time < bound_upper)
                {
                    if (file_count >= alloc_size)
                    {
                        alloc_size *= 2;
                        files = (psym_file*)realloc(files, alloc_size * sizeof(psym_file));
                        memset(files + file_count, 0, sizeof(psym_file) * (alloc_size - file_count));
                    }
                    strip_file_extension(find_data.cFileName);
                    files[file_count].name = _wcsdup(find_data.cFileName);
                    files[file_count].ext = ext;
                    files[file_count].dir_ind = i;
                    files[file_count].date = modify_time;
                    ++file_count;
                }
            } while (FindNextFileW(find, &find_data));
            FindClose(find);
            wprintf(L"%i\n", file_count - ext_beg_count);
        }
        wprintf(L"\ttotal: %i\n", file_count - dir_beg_count);
    }
    wprintf(L"total files: %i\n", file_count);
    if (!file_count)
    {
        wprintf(L"nothing to write\n");
        return 0;
    }

    sort_quick_desc(files, file_count);

    // shuffle
    const int count = file_count / unit_size - 1; // round down, appendix is left in place
    const int unit_byte_size = sizeof(psym_file) * unit_size;
    psym_file *tmp = (psym_file *)malloc(unit_byte_size);
    for (int i = 0; i < count; ++i)
    {
        const int ind = rand_range(i + 1, count) * unit_size;
        memcpy(tmp, files + ind, unit_byte_size);
        memcpy(files + ind, files + i * unit_size, unit_byte_size);
        memcpy(files + i * unit_size, tmp, unit_byte_size);
    }
    free(tmp);
    
    // get full dir paths
    wchar_t **full_dirs = (wchar_t **)malloc(sizeof(wchar_t *) * dir_count);
    for (int i = 0; i < dir_count; ++i)
    {
        const DWORD len = GetFullPathNameW(dirs[i], 0, NULL, NULL);
        full_dirs[i] = malloc(sizeof(wchar_t) * len);
        GetFullPathNameW(dirs[i], len, full_dirs[i], NULL);
    }

    int ret = write_bin(full_dirs, dir_count, exts, ext_count, output, unit_size, files, file_count);

    // cleanup
    for (int i = 0; i < dir_count; ++i)
        free(full_dirs[i]);
    free(full_dirs);
    for (int i = 0; i < file_count; ++i)
        free(files[i].name);
    free(files);
    return ret;
}

static int extract(const wchar_t *input, const wchar_t *output, int count, char keep_pos)
{
    typedef struct
    {
        wchar_t* name;
        uint8_t dir_ind;
        uint8_t ext_ind;
    } psym_cmp_file;
    typedef struct
    {
        psym_cmp_file *files;
        time_t date;
        uint8_t count;
    } psym_cmp_unit;

    FILE *file = _wfopen(input, keep_pos ? L"rb" : L"rb+");
    if (!file)
        return log_err_and_return(L"could not open file %s\n", input);

    uint8_t unit_size, ext_count, dir_count;
    char id_buf[6] = { '\0' };

    fread(id_buf, sizeof(char), 5, file);
    if (strcmp(id_buf, "PSYM3"))
    {
        fclose(file);
        return log_err_and_return(L"could not identify input file %s\n", input);
    }
    // unit size
    fread(&unit_size, sizeof unit_size, 1, file);

    // exts
    fread(&ext_count, sizeof ext_count, 1, file);
    wchar_t **exts = (wchar_t **)malloc(sizeof(wchar_t *) * ext_count);
    uint16_t *ext_lengths = (uint16_t *)malloc(sizeof(uint16_t) * ext_count);
    read_wstrs_w_lengths(exts, ext_lengths, ext_count, file);

    // dirs
    fread(&dir_count, sizeof dir_count, 1, file);
    wchar_t **dirs = (wchar_t **)malloc(sizeof(wchar_t *) * dir_count);
    uint16_t*dir_lengths = (uint16_t *)malloc(sizeof(uint16_t) * dir_count);
    read_wstrs_w_lengths(dirs, dir_lengths, dir_count, file);

    // seek to pos
    const uint32_t file_iter_pos = ftell(file);
    uint32_t file_iter;
    fread(&file_iter, sizeof file_iter, 1, file);
    fseek(file, file_iter, SEEK_SET);

    // read
    psym_cmp_unit *units = (psym_cmp_unit *)malloc(sizeof(psym_cmp_unit) * count);
    memset(units, 0, sizeof(psym_cmp_unit) * count);
    int unit_count = 0;

    for (; unit_count < count; ++unit_count)
    {
        if (!fread(&units[unit_count].count, sizeof(uint8_t), 1, file))
            break;
        fread(&units[unit_count].date, sizeof(time_t), 1, file);

        units[unit_count].files = (psym_cmp_file *)malloc(sizeof(psym_cmp_file) * units[unit_count].count);
        for (uint8_t j = 0; j < units[unit_count].count; ++j)
        {
            uint16_t len;
            fread(&units[unit_count].files[j].dir_ind, sizeof(uint8_t), 1, file);
            fread(&units[unit_count].files[j].ext_ind, sizeof(uint8_t), 1, file);
            read_wstrs_w_lengths(&units[unit_count].files[j].name, &len, 1, file);
        }
    }

    // update pos
    if (!keep_pos)
    {
        file_iter = ftell(file);
        fseek(file, file_iter_pos, 0);
        fwrite(&file_iter, sizeof file_iter, 1, file);
    }
    fclose(file);

    wchar_t dir_path[MAX_PATH], dir_path_unit[MAX_PATH], dst_path[MAX_PATH], src_path[MAX_PATH];
    if (output)
        wsprintfW(src_path, L"%s\\%s", output, L"psym_extr"); // tmp usage of src_path
    else
        wsprintfW(src_path, L"%s", L"psym_extr");

    int ret = create_dir_dupsafe(dir_path, src_path);

    if (!ret)
    {
        for (int i = 0; i < unit_count; ++i)
        {
            struct tm* time = localtime(&units[i].date);
            wsprintfW(dir_path_unit, L"%s\\[%i]%i.%i.%i", dir_path,
                i + 1, time->tm_mday, time->tm_mon + 1, time->tm_year - 100);

            CreateDirectoryW(dir_path_unit, NULL);
            for (int j = 0; j < units[i].count; ++j)
            {
                wsprintfW(src_path, L"%s\\%s.%s",
                    dirs[units[i].files[j].dir_ind],
                    units[i].files[j].name,
                    exts[units[i].files[j].ext_ind]);
                wsprintfW(dst_path, L"%s\\%s.%s",
                    dir_path_unit,
                    units[i].files[j].name,
                    exts[units[i].files[j].ext_ind]);
                if (!CopyFileW(src_path, dst_path, FALSE))
                    fwprintf(stderr, L"could not copy file %s\n", src_path);
            }
        }
    }
    else
        fwprintf(stderr, L"could not create output directory %s\n", dir_path);

    // cleanup
    for (int i = 0; i < unit_count; ++i)
    {
        for (int j = 0; j < units[i].count; ++j)
            free(units[i].files[j].name);
        free(units[i].files);
    }
    free(units);
    for (int i = 0; i < dir_count; ++i)
        free(dirs[i]);
    free(dir_lengths);
    free(dirs);
    for (int i = 0; i < ext_count; ++i)
        free(exts[i]);
    free(ext_lengths);
    free(exts);

    return ret;
}

int rst(const wchar_t *input)
{
    FILE* file = _wfopen(input, L"rb+");
    if (!file)
        return log_err_and_return(L"could not open file %s\n", input);

    uint16_t int_buf;
    char id_buf[6] = { '\0' };

    fread(id_buf, sizeof(char), 5, file);
    if (strcmp(id_buf, "PSYM3"))
    {
        fclose(file);
        return log_err_and_return(L"could not identify input file %s\n", input);
    }

    fseek(file, 1, SEEK_CUR);
    // both exts and dirs
    for (int i = 0; i < 2; ++i)
    {
        fread((uint8_t *)&int_buf, sizeof(uint8_t), 1, file);
        for (uint8_t i = *(uint8_t *)&int_buf; i; --i)
        {
            fread(&int_buf, sizeof int_buf, 1, file);
            fseek(file, int_buf * sizeof(wchar_t), SEEK_CUR);
        }
    }
    uint32_t pos_buf = ftell(file) + sizeof pos_buf;
    fwrite(&pos_buf, sizeof pos_buf, 1, file);
    fclose(file);
    return 0;
}

int main(int argc, char **argv)
{
    srand(time(NULL));
    wchar_t **wargv_mem = CommandLineToArgvW(GetCommandLineW(), &argc);
    wchar_t **wargv = wargv_mem;

    int ret = -1;
    opt_ctx* ctx = NULL;

    if (argc == 2 && !wcscmp(wargv[1], L"--help"))
    {
        wprintf(
            L"usage: psym {gen <dirs...> | ext <num> | rst} [options...] <file>\n" \
            L"<dirs..>              \tdirectories to cycle through\n" \
            L"<num>                 \tnumber of entries to extract\n" \
            L"<file>                \tfile to operate on/save to\n" \
            L"mode description:\n" \
            L"gen                   \tgenerate reference file\n" \
            L"ext                   \textract entries\n" \
            L"rst                   \treset position in reference file\n" \
            L"gen options:\n" \
            L"-e <ext...> , -e<ext> \tspecify accepted file extensions(without leading .)\n" \
            L"-s <size> , -s<size>  \tspecify generated entry size\n" \
            L"-l <date> , -l<date>  \tspecify the lower file date bound in dd.mm.yy format\n" \
            L"-u <date> , -u<date>  \tspecify the upper file date bound in dd.mm.yy format\n" \
            L"ext options:\n" \
            L"-o <out> -o<out>      \tspecify output directory\n" \
            L"-k                    \tdon't update the reading position\n" \
            L"rst options:\n" \
            L"no options\n\n" \

            L"defaults:\n" \
            L"-e:\t");
        const int def_ext_count = sizeof(_DEF_EXTENSIONS) / sizeof(wchar_t *) - 1;
        for (int i = 0; i < def_ext_count; ++i)
            wprintf(L"%s, ", _DEF_EXTENSIONS[i]);
        wprintf(L"%s\n", _DEF_EXTENSIONS[def_ext_count]);
        wprintf(
            L"-s:\t%i\n" \
            L"-l:\tlowest possible\n" \
            L"-u:\thighest possible\n" \
            L"-o:\tcurrent directory\n"
            , DEF_UNIT_SIZE);
        ret = 0;
        goto ret_point;
    }

    if (argc < 3)
    {
        ret = log_err_and_return(L"not enough arguments\n");
        goto ret_point;
    }

    const wchar_t *file = wargv[argc - 1];
    if (!wcscmp(wargv[1], L"rst"))
    {
        ret = argc == 3 ? 
            rst(file) :
            log_err_and_return(L"too many arguments\n");
    }
    else if (!wcscmp(wargv[1], L"gen"))
    {
        argc -= 3;
        wargv += 2;
        uint8_t dir_count = 0, ext_count = sizeof(_DEF_EXTENSIONS) / sizeof(wchar_t *);
        uint8_t unit_size = DEF_UNIT_SIZE;
        time_t l_bound = 0, u_bound = LLONG_MAX;
        const wchar_t **dirs = wargv, **exts = _DEF_EXTENSIONS;

        while (wargv[dir_count][0] != '-' && dir_count < argc)
            ++dir_count;

        int opt_counts[4] = { OPT_ARGS_NON_ZERO, 1, 1, 1 };
        ctx = parse_options(argc - dir_count, wargv + dir_count, L"eslu", opt_counts, 4);
        if (ctx)
        {
            opt_node* opt = find_opt(ctx, L'e');
            if (OPT_ARGS_EXISTS(*opt))
            {
                exts = opt->args;
                ext_count = opt->count;
            }
            opt = find_opt(ctx, L's');
            if (OPT_ARGS_EXISTS(*opt))
            {
                unit_size = wcstol(opt->args[0], NULL, 10);
                if (unit_size == UCHAR_MAX || unit_size <= 0)
                {
                    fwprintf(stderr, L"invalid/out of range value: -s\n");
                    goto ret_point;
                }
            }
            opt = find_opt(ctx, L'l');
            if (OPT_ARGS_EXISTS(*opt))
            {
                l_bound = wcstot_t(opt->args[0]);
                if (l_bound == -1)
                {
                    fwprintf(stderr, L"invalid/out of range value: -l\n");
                    goto ret_point;
                }
            }
            opt = find_opt(ctx, L'u');
            if (OPT_ARGS_EXISTS(*opt))
            {
                u_bound = wcstot_t(opt->args[0]);
                if (u_bound == -1)
                {
                    fwprintf(stderr, L"invalid/out of range value: -u\n");
                    goto ret_point;
                }
            }
        }

        ret = gen(dirs, dir_count, exts, ext_count, file, unit_size, l_bound, u_bound);
    }
    else if (!wcscmp(wargv[1], L"ext"))
    {
        argc -= 3;
        wargv += 2;
        int unit_count = -1;
        char keep_pos = 0;
        wchar_t *output = NULL;

        unit_count = wcstol(wargv[0], NULL, 10);
        if (unit_count == INT_MAX || unit_count <= 0)
            return log_err_and_return(L"invalid/out of range value: unit count\n");

        int opt_counts[2] = { 1, OPT_FLAG };
        ctx = parse_options(argc - 1, wargv + 1, L"ok", opt_counts, 2);
        if (ctx)
        {
            opt_node *opt = find_opt(ctx, L'o');
            if (OPT_ARGS_EXISTS(*opt))
                output = opt->args[0];
            opt = find_opt(ctx, L'k');
            if (OPT_FLAG_EXISTS(*opt))
                keep_pos = 1;
        }

        ret = extract(file, output, unit_count, keep_pos);
    }
    else
        ret = log_err_and_return(L"invalid usage, type --help to learn more\n");

ret_point:
    LocalFree(wargv_mem);
    if (ctx)
        delete_opt_ctx(ctx);
    return ret;
}
