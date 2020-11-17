#include "opt_parser.h"

#include <stdlib.h>
#include <stdio.h>

struct opt_ctx
{
    opt_node *nodes;
    int count;
};

void delete_opt_ctx(opt_ctx *ctx)
{
    free(ctx->nodes);
    free(ctx);
}

opt_node *find_opt(opt_ctx *ctx, wchar_t opt)
{
    for (int i = 0; i < ctx->count; ++i)
        if (ctx->nodes[i].opt == opt)
            return ctx->nodes + i;
    return NULL;
}

opt_ctx *parse_options(int argc, wchar_t **argv, const wchar_t *opts, int *opt_arg_count, int arg_count)
{
    if (!argc)
        return NULL;

    opt_ctx *ctx = (opt_ctx *)malloc(sizeof(opt_ctx));
    ctx->nodes = (opt_node *)malloc(sizeof(opt_node) * arg_count);
    ctx->count = arg_count;
    for (int i = 0; i < arg_count; ++i)
    {
        ctx->nodes[i].opt = opts[i];
        ctx->nodes[i].count = 0;
    }

    wchar_t **argv_end = argv + argc;
    while (argv != argv_end)
    {
        if (argv[0][0] != L'-')
        {
            fwprintf(stderr, L"unrecognized token %s\n", argv[0]);
            goto err_ret;
        }

        opt_node *node = find_opt(ctx, argv[0][1]);
        if (!node)
        {
            fwprintf(stderr, L"unrecognized option -%c\n", argv[0][1]);
            goto err_ret;
        }
        if (node->count)
        {
            fwprintf(stderr, L"option -%c already specified\n", argv[0][1]);
            goto err_ret;
        }

        if (opt_arg_count[node - ctx->nodes] == OPT_FLAG)
        {
            node->count = 1;
            wchar_t *str_it = argv[0] + 2;
            while (*str_it != L'\0')
            {
                node = find_opt(ctx, *(str_it++));
                if (!node || opt_arg_count[node - ctx->nodes] != OPT_FLAG)
                {
                    fwprintf(stderr, L"unrecognized option -%c\n", argv[0][1]);
                    goto err_ret;
                }

                if (node->count)
                {
                    fwprintf(stderr, L"option -%c already specified\n", argv[0][1]);
                    goto err_ret;
                }

                node->count = 1;
            }
            ++argv;
        }
        else
        {
            if (argv[0][2] != L'\0')
            {
                const int len = wcslen(argv[0] + 2);
                memmove(argv[0], argv[0] + 2, sizeof(wchar_t) * (len + 1));
                node->count = 1;
                node->args = argv++;
            }
            else
            {
                const int ind = node - ctx->nodes;
                while (++argv != argv_end && argv[0][0] != L'-' &&  node->count < opt_arg_count[ind])
                    ++node->count;
                if (!node->count)
                {
                    fwprintf(stderr, L"no arguments provided for option -%c\n", argv[0][1]);
                    goto err_ret;
                }

                node->args = argv - node->count;
            }
        }       
    }
    return ctx;

err_ret:
    delete_opt_ctx(ctx);
    return NULL;
}
