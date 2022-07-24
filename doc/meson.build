# This file is part of RichErrors.
# Copyright 2019-2022 Board of Regents of the University of Wisconsin System
# SPDX-License-Identifier: BSD-2-Clause

doxygen = find_program(
    'doxygen',
    required: get_option('docs'),
)

doxyfile = configure_file(
    configuration: {
        'DOXYGEN_INPUT': join_paths(
            meson.current_source_dir(),
            '..',
            'include',
        ),
        'DOXYGEN_OUTPUT': join_paths(
            meson.current_build_dir(),
            'apidoc',
        ),
    },
    input: 'Doxyfile.in',
    output: 'Doxyfile',
)

doc_install_dir = join_paths(get_option('datadir'), 'doc', 'RichErrors')

if doxygen.found()
    custom_target(
        'docs',
        command: [doxygen, doxyfile],
        depend_files: [public_c_headers, public_cpp_headers],
        output: ['apidoc'],
        install: true,
        install_dir: doc_install_dir,
    )
endif
