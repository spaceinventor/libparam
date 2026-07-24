plt_405_exe = executable('plt_405', 'plt_405.c', dependencies: [param_dep, csp_dep])
test('plt_405', plt_405_exe)
plt_406_exe = executable('plt_406', 'plt_406.c', dependencies: [param_dep, csp_dep], link_args : ['-Wl,-Map=plt_406.map'])
test('plt_406', plt_406_exe)