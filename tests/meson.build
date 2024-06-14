if get_option('slash') == true
    param_slash_parse_array_tests = executable(
        'param_slash_parse_array_tests',
        'param_slash_parse_array_tests.c',
        include_directories: '../src/param',
        dependencies: [param_dep]
    )

    test('param_slash_parse_array_tests', param_slash_parse_array_tests)
endif