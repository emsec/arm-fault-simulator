project('fault simulator', 'cpp',
 version : '0.1',
 default_options : ['warning_level=3', 'cpp_std=c++17'])

libarmory_dep = subproject('armory').get_variable('libarmory_dep')

incdir = include_directories('include')

src = []
subdir('src')

libfault_simulator = static_library('fault_simulator', sources: src, include_directories : incdir, dependencies: libarmory_dep)
libfault_simulator_dep = declare_dependency(include_directories : incdir, link_with: libfault_simulator, dependencies: libarmory_dep)
