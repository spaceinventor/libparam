project('libparam', 'c')

libparam_sources = files([
	'src/param/param.c',
	'src/param/param_string.c',
	'src/param/param_serializer.c',
	'src/param/group/param_group.c',
	'src/param/group/param_group_store_file.c',
	'src/param/group/param_group_slash.c',
	'src/param/group/param_group_beacon.c',
	'src/param/list/param_list.c',
	'src/param/list/param_list_slash.c',
	'src/param/list/param_list_store_file.c',
	'src/param/param_slash.c',
	'src/param/param_client.c',
	'src/param/param_client_slash.c',
	'src/param/param_server.c',
	'src/vmem/vmem.c',
	'src/vmem/vmem_ram.c',
	'src/vmem/vmem_client.c',
	'src/vmem/vmem_client_slash.c',
	'src/vmem/vmem_client_slash_ftp.c',
	'src/vmem/vmem_server.c',
	])
	
conf = configuration_data()
conf.set('PARAM_STORE_FILE', true)
conf.set('PARAM_STORE_VMEM', false)
conf.set('PARAM_STORE_LOG', false)
libparam_h = configure_file(output: 'libparam.h', configuration: conf)

libparam_deps = []
libparam_deps += meson.get_compiler('c').find_library('m', required : true)
libparam_deps += dependency('libcsp', fallback : ['csp', 'libcsp_dep'])
libparam_deps += dependency('slash', fallback : ['slash', 'slash_dep'])

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
