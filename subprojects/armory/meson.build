project('armory', 'cpp',
 version : '0.1',
 default_options : ['warning_level=3', 'cpp_std=c++17'])

libmulator_dep = subproject('m-ulator').get_variable('libmulator_dep')

incdir = include_directories('include')

src = []
subdir('src')

thread_dep = dependency('threads')

libarmory = static_library('armory', sources: src, include_directories : incdir, dependencies: libmulator_dep)
libarmory_dep = declare_dependency(include_directories : incdir, link_with: libarmory, dependencies: [libmulator_dep,thread_dep])
