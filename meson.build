project('param', 'c')
	
conf = configuration_data()
conf.set('PARAM_STORE_FILE', false)
conf.set('PARAM_STORE_VMEM', false)
conf.set('PARAM_STORE_LOG', false)
libparam_h = configure_file(output: 'libparam.h', configuration: conf)

param_deps = []
param_deps += meson.get_compiler('c').find_library('m', required : true)
param_deps += dependency('libcsp', fallback : ['csp', 'csp_dep'])
slash_dep = dependency('slash', fallback : ['slash', 'slash_dep'], required: false)

param_src = files([
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
	param_deps += slash_dep
	param_src += files([
		'src/param/group/param_group_slash.c',	
		'src/param/list/param_list_slash.c',	
		'src/param/param_client_slash.c',
		'src/param/param_slash.c',	
		'src/vmem/vmem_client_slash_ftp.c',
		'src/vmem/vmem_client_slash.c',
	])
endif

param_inc = include_directories('.', 'include')

param = static_library('param',
	param_src,
	include_directories : param_inc,
	dependencies : param_deps,
	install: true
	)
	
param_dep = declare_dependency(
	sources: libparam_h,
	include_directories : param_inc,
	dependencies : param_deps,
	link_with: param,
	)
