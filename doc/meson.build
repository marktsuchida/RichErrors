# This file is part of RichErrors.
# Copyright 2019-2022 Board of Regents of the University of Wisconsin System
# SPDX-License-Identifier: BSD-2-Clause

doxygen = find_program('doxygen', required: false)

if doxygen.found()
    # The only way to specify the input and output location for doxygen is by
    # editing the Doxyfile.
    doxygen_conf = configuration_data()
    doxygen_conf.set('DOXYGEN_INPUT', meson.source_root() + '/include')
    doxygen_conf.set('DOXYGEN_OUTPUT', meson.current_build_dir() + '/doxygen')

    doxyfile = configure_file(configuration: doxygen_conf,
        input: 'Doxyfile.in', output: 'Doxyfile')

    custom_target('doc',
        build_by_default: true,
        command: [doxygen, doxyfile],
        depend_files: [public_c_headers, public_cpp_headers],
        output: ['doxygen'])
endif
