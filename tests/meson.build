add_languages('cpp')
gtest_dep = dependency('gtest', main : true)
gtest_main_dep = dependency('gtest_main')
gmock_dep = dependency('gmock')
vmem_block_tests = executable(
    'vmem_block_tests',
    sources: [
        'vmem_block_tests.cpp',
    ],
    dependencies: [gtest_dep, gmock_dep, gtest_main_dep],
    include_directories : param_inc,
    link_with : param_lib
)

test('vmem_block_tests', vmem_block_tests)
