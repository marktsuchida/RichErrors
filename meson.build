project('RichErrors', 'c', 'cpp',
        default_options: ['c_std=c11', 'cpp_std=c++14'])

threads_dep = dependency('threads')

public_inc = include_directories('include')

subdir('include')
subdir('src/RichErrors')
subdir('test/RichErrorsTests')
subdir('doc')
