# This file is part of RichErrors.
# Copyright 2019-2022 Board of Regents of the University of Wisconsin System
# SPDX-License-Identifier: BSD-2-Clause

test_srcs = [
    'DynArrayTests.cpp',
    'Err2CodeCppTests.cpp',
    'Err2CodeTests.cpp',
    'InfoMapCppTests.cpp',
    'InfoMapTests.cpp',
    'RichErrorsCppTests.cpp',
    'RichErrorsTests.cpp',
    'TestDefs.h',
    'main.cpp',
]

catch_dep = dependency('catch2',
    required: get_option('tests'),
    disabler: true,
)

test_exe = executable('tests', test_srcs,
    include_directories: public_inc,
    link_with: main_lib,
    dependencies: [catch_dep],
)

test('RichErrors Tests', test_exe)
