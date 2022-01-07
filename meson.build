project('param', 'c')
	
conf = configuration_data()
conf.set('PARAM_STORE_FILE', false)
conf.set('PARAM_STORE_VMEM', false)
conf.set('PARAM_STORE_LOG', false)
conf.set('PARAM_HAVE_SYS_QUEUE', get_option('list_dynamic'))
conf.set('PARAM_LIST_DYNAMIC', get_option('list_dynamic'))
conf.set('PARAM_HAVE_SCHEDULER', get_option('scheduler'))
conf.set('PARAM_HAVE_COMMANDS', get_option('commands'))
if get_option('have_fopen') == true
	conf.set('MPACK_STDIO', 1)
endif

libparam_h = configure_file(output: 'libparam.h', configuration: conf)

csp_dep = dependency('csp', fallback : ['csp', 'csp_dep'])

clib = meson.get_compiler('c').find_library('c', required: false)
clib_dep = []
if not clib.found()
	clib_dep = dependency('libc', fallback: ['picolibc', 'picolibc_dep'], required: true)
endif

param_src = files([
	'src/param/list/param_list.c',

	'src/param/param_client.c',
		
	'src/param/param_serializer.c',
	'src/param/param_server.c',
	'src/param/param_string.c',
	'src/param/param_queue.c',
	'src/param/param.c',

	'lib/mpack/mpack.c',

	'src/vmem/vmem_client.c',
	'src/vmem/vmem_ram.c',
	'src/vmem/vmem_server.c',
	'src/vmem/vmem.c',

	'src/objstore/objstore.c',
])

if get_option('have_fopen') == true
	param_src += files([
		'src/vmem/vmem_file.c',
		'src/param/list/param_list_store_vmem.c',
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
		'src/vmem/vmem_fram_secure.c',
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
			#'src/param/scheduler/param_scheduler_slash.c',
			#'src/param/commands/param_commands_slash.c',
		])
		if get_option('have_fopen') == true
			param_src += files([
				'src/vmem/vmem_client_slash_ftp.c',
				'src/param/list/param_list_store_slash.c'
			])
		endif
	endif
endif

param_inc = include_directories('.', 'include', 'lib')

param_lib = static_library('param',
	sources: [param_src, libparam_h],
	include_directories : param_inc,
	dependencies : [clib_dep, csp_dep, slash_dep],
	install : false
)

param_dep = declare_dependency(include_directories : param_inc, link_with : param_lib)

if get_option('enable_python3_bindings')

	# The Python version specified in .find_installtion(),
	# may be changed freely between 3.8, 3.9 and 3.10,
	# such that it matches the version expected to be used.
	# This is required for the extension module to be importable.
	py = import('python').find_installation('python3.9')
	# py.dependency() doesn't work with version constraint. Use plain
	# dependency() instead
	pydep = dependency('python3', version : '>=3.5', required : true)
	py.extension_module('libparam_py3', 'src/bindings/python/pyparam.c',
                    	dependencies : [param_dep, pydep, csp_dep],
                    	install : true)
endif
