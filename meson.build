project('libparam', 'c')
	
conf = configuration_data()
conf.set('PARAM_STORE_FILE', true)
conf.set('PARAM_STORE_VMEM', false)
conf.set('PARAM_STORE_LOG', false)
libparam_h = configure_file(output: 'libparam.h', configuration: conf)

libparam_deps = []
libparam_deps += meson.get_compiler('c').find_library('m', required : true)
libparam_deps += dependency('libcsp', fallback : ['csp', 'libcsp_dep'])
slash_dep = dependency('slash', fallback : ['slash', 'slash_dep'], required: false)

libparam_sources = files([
	'src/param/collector/param_collector.c',
	'src/param/group/param_group_beacon.c',
	'src/param/group/param_group_store_file.c',
	'src/param/group/param_group_store_vmem.c',
	'src/param/group/param_group.c',

	'src/param/list/param_list_store_file.c',
	'src/param/list/param_list_store_vmem.c',
	'src/param/list/param_list.c',

	'src/param/param_client.c',
	
#   TODO: Finish log system
#	'src/param/param_log_slash.c',
#	'src/param/param_log.c',
	
	'src/param/param_serializer.c',
	'src/param/param_server.c',
	'src/param/param_string.c',
	'src/param/param.c',

	'src/vmem/vmem_client.c',
	'src/vmem/vmem_ram.c',
	'src/vmem/vmem_server.c',
	'src/vmem/vmem.c',
])

if slash_dep.found()
	libparam_deps += slash_dep
	libparam_sources += files([
		'src/param/group/param_group_slash.c',	
		'src/param/list/param_list_slash.c',	
		'src/param/param_client_slash.c',
		'src/param/param_slash.c',	
		'src/vmem/vmem_client_slash_ftp.c',
		'src/vmem/vmem_client_slash.c',
	])
endif
	
#   TODO: Move these vmem backends to same place where the driver resides and depend on libparam
#libparam_sources += files([
#	'src/vmem/vmem_fram_secure.c',
#	'src/vmem/vmem_fram.c',
#]) 

libparam_inc = include_directories('.', 'include')

libparam = static_library('libparam',
	libparam_sources,
	include_directories : libparam_inc,
	dependencies : libparam_deps,
	install: true
	)
	
libparam_dep = declare_dependency(
	sources: libparam_h,
	include_directories : libparam_inc,
	dependencies : libparam_deps,
	link_with: libparam,
	)
