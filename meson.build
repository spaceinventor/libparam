project('param', 'c', subproject_dir:'lib', meson_version: '>=0.64.0', version: run_command('./get_pip_ersion.py').stdout().strip(),)

conf = configuration_data()
conf.set('PARAM_HAVE_SYS_QUEUE', get_option('list_dynamic') or get_option('list_pool') > 0)
conf.set('PARAM_LIST_DYNAMIC', get_option('list_dynamic'))
conf.set('PARAM_LIST_POOL', get_option('list_pool'))
conf.set('PARAM_HAVE_SCHEDULER', get_option('scheduler'))
conf.set('PARAM_HAVE_COMMANDS', get_option('commands'))
# From now on, VMEM API is 64bits, breaking earlier ABI. 
# New user code can use the fact that this macro is defined (its value is not relevant, just the fact that it is defined)
# to check that the libparam version included (typically through convoluted dependency paths) does support the 64-bits API
# and #error if it doesnt.
VMEM_64_BITS_API_description = '''From now on, VMEM API is 64bits, breaking earlier ABI. 
New user code can use the fact that this macro is defined (its value is not relevant, just the fact that it is defined)
to check that the libparam version included (typically through convoluted dependency paths) does support the 64-bits API
and #error if it doesnt.'''

conf.set('PARAM_VMEM_64_BITS_API', 1, description: VMEM_64_BITS_API_description)

if get_option('have_float') == false
	conf.set('MPACK_FLOAT', 0)
endif

if get_option('have_fopen') == true
	if get_option('list_dynamic') == true
		conf.set('MPACK_STDIO', 1)
	else
		warning('*********')
		warning('Enabling \'have_fopen\' without also enabling \'list_dynamic\' causes mpack related build failures, setting MPACK_STDIO=0')
		warning('*********')
		conf.set('MPACK_STDIO', 0)
	endif
endif

libparam_h = configure_file(output: 'libparam.h', configuration: conf)

csp_dep = dependency('csp', fallback : ['csp', 'csp_dep'])

clib = meson.get_compiler('c').find_library('c', required: false)
clib_dep = []
if not clib.found()
	clib_dep = dependency('libc', fallback: ['picolibc', 'picolibc_dep'], required: true)
endif

# On linux include bsd libraries for strlcpy function (which is non standard)
bsd_dep = dependency('libbsd-overlay', required: false)

param_src = files([
	'src/param/list/param_list.c',

	'src/param/param_client.c',
		
	'src/param/param_serializer.c',
	'src/param/param_server.c',
	'src/param/param_string.c',
	'src/param/param_queue.c',
	'src/param/param_wildcard.c',
	'src/param/param.c',

	'src/mpack/mpack.c',

	'src/vmem/vmem_client.c',
	'src/vmem/vmem_crc32.c',
	'src/vmem/vmem_server.c',
	'src/vmem/vmem.c',
	'src/vmem/vmem_block.c',

	'src/objstore/objstore.c',
])

if get_option('have_fopen') == true
	param_src += files([
		'src/vmem/vmem_file.c',
		'src/vmem/vmem_mmap.c',
		#'src/param/list/param_list_store_vmem.c',
	])
endif

if get_option('scheduler') == true
	param_src += files([
		'src/param/scheduler/param_scheduler.c',
	])	
endif

if get_option('scheduler_client') == true
	param_src += files([
		'src/param/scheduler/param_scheduler_client.c',
	])	
endif

if get_option('commands') == true
	param_src += files([
		'src/param/commands/param_commands.c',
	])	
endif

if get_option('commands_client') == true
	param_src += files([
		'src/param/commands/param_commands_client.c',
	])	
endif

if get_option('vmem_fram') == true
	param_src += files([
		'src/vmem/vmem_fram.c',
		'src/vmem/vmem_fram_cache.c',
	])
endif


if get_option('collector') == true
	param_src += files([
		'src/param/collector/param_collector_config.c',
		'src/param/collector/param_collector.c',
	])
endif

slash_dep = []
if get_option('slash') == true
	slash_dep = dependency('slash', fallback : ['slash', 'slash_dep'], required: false)
	if slash_dep.found()
		param_src += files([
			'src/param/list/param_list_slash.c',	
			'src/param/param_slash.c',	
			'src/vmem/vmem_client_slash.c',
			#'src/objstore/objstore_slash.c',
		])
		if get_option('have_fopen') == true
			param_src += files([
				'src/vmem/vmem_client_slash_ftp.c',
				#'src/param/list/param_list_store_slash.c'
			])
		endif
		if get_option('scheduler_client') == true
			param_src += files([
				'src/param/scheduler/param_scheduler_slash.c',
			])	
		endif
		if get_option('commands_client') == true
			param_src += files([
			'src/param/commands/param_commands_slash.c',
			])	
		endif
	endif
endif

param_inc = include_directories('.', 'include')

param_lib = library('param',
	sources: [param_src, libparam_h],
	include_directories : param_inc,
	dependencies : [clib_dep, bsd_dep, csp_dep, slash_dep],
	install : false
)

param_dep = declare_dependency(include_directories : param_inc, link_with : param_lib)

if not meson.is_subproject()
    subdir('tests')
endif

if get_option('enable_python3_bindings')

	sipyparam_sources = [

		# Module itself
		'src/bindings/python/sipyparam.c',

		# Parameter classes
		'src/bindings/python/parameter/parameter.c',
		'src/bindings/python/parameter/parameterarray.c',
		'src/bindings/python/parameter/parameterlist.c',
		'src/bindings/python/parameter/pythonarrayparameter.c',
		'src/bindings/python/parameter/pythongetsetarrayparameter.c',
		'src/bindings/python/parameter/pythongetsetparameter.c',
		'src/bindings/python/parameter/pythonparameter.c',

		# Wrapper functions
		'src/bindings/python/wrapper/param_py.c',
		'src/bindings/python/wrapper/spaceboot_py.c',
		'src/bindings/python/wrapper/param_list_py.c',
		'src/bindings/python/wrapper/vmem_client_py.c',

		# Utilities
		'src/bindings/python/utils.c',
	]

	pycsh_config_h = configure_file(output: 'sipyparamconfig.h', configuration: conf, install_dir: 'include/param/', install: false)

	# Add .pyi file for type-hints
	pyi = configure_file(input: 'src/bindings/python/sipyparam.pyi', output: 'sipyparam.pyi', copy: true)

	# Also add __init__.py for high-level APIs
	__init__py = configure_file(input: 'src/bindings/python/__init__.py', output: '__init__.py', copy: true)

	# "pure : false" allows the __init__.py package to contain both 'native' Python .pyi and a compiled .so files.
	py = import('python').find_installation('python3', pure : false)
	# py.dependency() doesn't work with version constraint. Use plain
	# dependency() instead
	pydep = dependency('python3', version : '>=3.10', required : true)
	py.extension_module('sipyparam', sipyparam_sources,
                    	dependencies : [param_dep, csp_dep, pydep],
						subdir : 'sipyparam',
                    	install : true)
	py.install_sources([pyi, __init__py], subdir: 'sipyparam')

	# If we mark libparam itself as install: true,
	#	it will be installed in a seperate site-packages/.sipyparam.mesonpy.libs/ directory when installing with pip.
	#	This directory will also work for the purpose of resolving symbols for the extension module.
	# Alternativly we can use the following custom_target() to bundle libparam.so into the .whl.
	#	This allows us to write the __init__.py to prioritize whether to use the bundled or system package/symbols.
	#   Using a custom target for this takes inpiration from: https://github.com/mesonbuild/meson-python/discussions/556
	copy_csp_lib = custom_target('libparam_pip',
		output: 'libparam_pip.so',
		install: true,
		install_dir: py.get_install_dir() / 'sipyparam',
		input: param_lib,  # TODO Kevin: Would it ever make sense to include slash here?
		command: ['cp', '@INPUT@', '@OUTPUT@'],
		build_by_default: true
	)
endif

