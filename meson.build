project('param', 'c', subproject_dir:'lib')

add_project_arguments(['-Wall'], language: 'c')

conf = configuration_data()
conf.set('PARAM_HAVE_SYS_QUEUE', get_option('list_dynamic') or get_option('list_pool') > 0)
conf.set('PARAM_LIST_DYNAMIC', get_option('list_dynamic'))
conf.set('PARAM_LIST_POOL', get_option('list_pool'))
conf.set('PARAM_NUM_PUBLISHQUEUES', get_option('num_publishqueues'))

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

libparam_h = configure_file(output: 'libparam.h', configuration: conf, install_dir: 'include', install_tag: 'devel')

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

param_inc = include_directories('.', 'include')
deps = [clib_dep, bsd_dep, csp_dep]
param_lib = library('param',
	sources: [param_src, libparam_h],
	include_directories : param_inc,
	dependencies : [clib_dep, bsd_dep, csp_dep],
	install : true,
	soversion: meson.project_version()
)
install_subdir('include', install_dir : '.', install_tag: 'devel')

pkg = import('pkgconfig')
pkg.generate(
	param_lib,
	requires: ['csp'],
	subdirs : ['.'],
)

param_dep = declare_dependency(include_directories : param_inc, link_with : param_lib, dependencies: [clib_dep, bsd_dep, csp_dep])
if get_option('default_library') == 'static'
	param_link_whole_dep = declare_dependency(
		include_directories : param_inc,
		link_with : param_lib,
		link_whole: [param_lib]
	)
endif

if get_option('test') == true
    subdir('tests')
endif
