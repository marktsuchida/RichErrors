# This file is part of RichErrors.
# Copyright 2019-2022 Board of Regents of the University of Wisconsin System
# SPDX-License-Identifier: BSD-2-Clause

test_src = [
    'DynArrayTests.cpp',
    'Err2CodeCppTests.cpp',
    'Err2CodeTests.cpp',
    'InfoMapCppTests.cpp',
    'InfoMapTests.cpp',
    'RichErrorsCppTests.cpp',
    'RichErrorsTests.cpp',
    'main.cpp',
]

catch_dep = dependency('catch2',
    required: get_option('tests'),
    disabler: true,
)

test_exe = executable('tests', test_src,
    include_directories: public_inc,
    link_with: richerrors_lib,
    dependencies: [catch_dep],
)

test('RichErrors Tests', test_exe)
