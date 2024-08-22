if not meson.is_subproject()
    cmake = import('cmake')
    gtest_proj = cmake.subproject('gtest')
    gtest_dep = gtest_proj.get_variable('gtest_dep') 
    gtest_main_dep = gtest_proj.get_variable('gtest_main_dep') 
    gmock_dep = gtest_proj.get_variable('gmock_dep') 
    gtest_lib = static_library('gtest', install : false)
    gmock_lib = static_library('gmock', install: false) 
endif

executable(
    'vmem_block_tests',
    sources: [
        'vmem_block_tests.cpp',
    ],
    dependencies: [gtest_dep, gmock_dep, gtest_main_dep],
    include_directories : param_inc,
    link_with : param_lib
)