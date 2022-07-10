# This file is part of RichErrors.
# Copyright 2019-2022 Board of Regents of the University of Wisconsin System
# SPDX-License-Identifier: BSD-2-Clause

main_srcs = [
    'DynArray.c',
    'DynArray.h',
    'Err2Code.c',
    'InfoMap.c',
    'RichErrors.c',
    'Threads.c',
    'Threads.h',
]

# Public headers are included solely to make them show up in the Visual Studio
# project
main_lib = static_library('RichErrors',
    [
        main_srcs,
        public_c_headers,
        public_cpp_headers,
    ],
    include_directories: public_inc,
    dependencies: threads_dep,
    install: true,
)
