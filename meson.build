project('ARMORY Example', 'cpp',
 version : '0.1',
 default_options : ['warning_level=3', 'cpp_std=c++17'])

message(get_option('buildtype'), 'build')

libfault_simulator_dep = subproject('fault_simulator').get_variable('libfault_simulator_dep')

executable('armory_example',
    sources: files('src/main.cpp'),
    include_directories : include_directories('include'),
    dependencies: libfault_simulator_dep)

