# vim: set ts=4 sw=4 tw=0 ai et:
add_library(argtable2 STATIC
    arg_dbl.c
    arg_end.c
    arg_file.c
    arg_int.c
    arg_lit.c
    arg_rem.c
    arg_str.c
    argtable2.c
    getopt1.c
    getopt.c
    )
add_definitions("-DSTDC_HEADERS -DHAVE_STDLIB_H -DHAVE_STRING_H")
