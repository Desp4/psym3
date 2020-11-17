#ifndef PSYM_OPT_PARSER
#define PSYM_OPT_PARSER

#include <wchar.h>

#define OPT_FLAG (0)
#define OPT_ARGS_NON_ZERO (-1)

#define OPT_FLAG_EXISTS(node) ((node).count)
#define OPT_ARGS_EXISTS OPT_FLAG_EXISTS

typedef struct
{
    wchar_t opt;
    wchar_t** args;
    int count;
} opt_node;

typedef struct opt_ctx opt_ctx;

void delete_opt_ctx(opt_ctx *ctx);
opt_node *find_opt(opt_ctx* ctx, wchar_t opt);
opt_ctx *parse_options(int argc, wchar_t **argv, const wchar_t *opts, int *opt_arg_count, int arg_count);

#endif
