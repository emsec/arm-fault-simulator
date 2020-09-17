project('m-ulator', 'cpp',
 version : '0.1',
 default_options : ['warning_level=3', 'cpp_std=c++17'])

incdir = include_directories('include')

src = []
subdir('src')

libmulator = static_library('mulator', sources: src, include_directories : incdir)
libmulator_dep = declare_dependency(include_directories : incdir, link_with : libmulator)
