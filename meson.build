# This file is part of RichErrors.
# Copyright 2019-2022 Board of Regents of the University of Wisconsin System
# SPDX-License-Identifier: BSD-2-Clause

project(
    'RichErrors',
    'c',
    'cpp',
    default_options: [
        'c_std=c11',
        'cpp_std=c++14',
        'default_library=static',
        'warning_level=3',
    ],
)

threads_dep = dependency('threads')

public_inc = include_directories('include')

subdir('include')
subdir('src')
subdir('test')
subdir('doc')

richerrors_dep = declare_dependency(
    include_directories: public_inc,
    link_with: richerrors_lib,
)

meson.override_dependency('RichErrors', richerrors_dep)
