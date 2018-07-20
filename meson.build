project('param', 'c')
	
conf = configuration_data()
conf.set('PARAM_STORE_FILE', false)
conf.set('PARAM_STORE_VMEM', false)
conf.set('PARAM_STORE_LOG', false)
libparam_h = configure_file(output: 'libparam.h', configuration: conf)

param_deps = []
param_deps += dependency('csp', fallback : ['csp', 'csp_dep'])

param_src = files([
	'src/param/collector/param_collector.c',
	'src/param/group/param_group_beacon.c',
	'src/param/group/param_group_store_file.c',
	'src/param/group/param_group_store_vmem.c',
	'src/param/group/param_group.c',

#	'src/param/list/param_list_store_file.c',
#	'src/param/list/param_list_store_vmem.c',
	'src/param/list/param_list.c',

	'src/param/param_client.c',
		
	'src/param/param_serializer.c',
	'src/param/param_server.c',
	'src/param/param_string.c',
	'src/param/param_queue.c',
	'src/param/param.c',
	
	'lib/mpack/mpack.c',

	'src/vmem/vmem_client.c',
	'src/vmem/vmem_fram.c',
	'src/vmem/vmem_fram_secure.c',
	'src/vmem/vmem_ram.c',
	'src/vmem/vmem_server.c',
	'src/vmem/vmem.c',
])

if get_option('slash') == true
	slash_dep = dependency('slash', fallback : ['slash', 'slash_dep'], required: false)
	if slash_dep.found()
		param_deps += slash_dep
		param_src += files([
			'src/param/group/param_group_slash.c',	
			'src/param/list/param_list_slash.c',	
			'src/param/param_slash.c',	
			'src/vmem/vmem_client_slash_ftp.c',
			'src/vmem/vmem_client_slash.c',
		])
	endif
endif
param_inc = include_directories('.', 'include', 'lib')

param_dep = declare_dependency(
	sources: [param_src, libparam_h],
	include_directories : param_inc,
	dependencies : param_deps,
	)
