project('param', 'c')
	
conf = configuration_data()
conf.set('PARAM_STORE_FILE', false)
conf.set('PARAM_STORE_VMEM', false)
conf.set('PARAM_STORE_LOG', false)
conf.set('PARAM_HAVE_SYS_QUEUE', get_option('list_dynamic'))
conf.set('PARAM_LIST_DYNAMIC', get_option('list_dynamic'))
conf.set('PARAM_HAVE_SCHEDULER', get_option('scheduler'))
if get_option('have_fopen') == true
	conf.set('MPACK_STDIO', 1)
endif

libparam_h = configure_file(output: 'libparam.h', configuration: conf)

param_deps = []
param_deps += dependency('csp', fallback : ['csp', 'csp_dep'])

clib = meson.get_compiler('c').find_library('c', required: false)
if not clib.found()
	clib = dependency('libc', fallback: ['picolibc', 'picolibc_dep'], required: true)
endif

param_deps += clib

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
	'src/objstore/objstore_slash.c'
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
		'src/param/scheduler/param_scheduler_client.c',
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

if get_option('slash') == true
	slash_dep = dependency('slash', fallback : ['slash', 'slash_dep'], required: false)
	if slash_dep.found()
		param_deps += slash_dep
		param_src += files([
			'src/param/list/param_list_slash.c',	
			'src/param/param_slash.c',	
			'src/vmem/vmem_client_slash.c',
			'src/param/scheduler/param_scheduler_slash.c',
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
	dependencies : param_deps,
)

param_dep = declare_dependency(include_directories : param_inc, link_with : param_lib)
param_dep_whole = declare_dependency(include_directories : param_inc, link_whole : param_lib)
