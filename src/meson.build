# This file is part of RichErrors.
# Copyright 2019-2022 Board of Regents of the University of Wisconsin System
# SPDX-License-Identifier: BSD-2-Clause

richerrors_src = [
    'DynArray.c',
    'Err2Code.c',
    'InfoMap.c',
    'RichErrors.c',
    'Threads.c',
]

richerrors_lib = library('RichErrors',
    richerrors_src,
    include_directories: public_inc,
    dependencies: threads_dep,
    install: true,
)
