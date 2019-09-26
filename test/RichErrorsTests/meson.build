test_srcs = [
    'DynArrayTests.cpp',
    'Err2CodeCppTests.cpp',
    'Err2CodeTests.cpp',
    'InfoMapCppTests.cpp',
    'InfoMapTests.cpp',
    'RichErrorsCppTests.cpp',
    'RichErrorsTests.cpp',
    'main.cpp',
]

test_exe = executable('RichErrorsTests', test_srcs,
        include_directories: public_inc,
        link_with: main_lib)

test('RichErrors Tests', test_exe)
