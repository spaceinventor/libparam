#!/usr/bin/env python
# encoding: utf-8

APPNAME = 'param'
VERSION = '0.1'

top = '.'
out = 'build'

def options(ctx):
    gr = ctx.add_option_group('libparam options')
    gr.add_option('--vmem', action='store_true')
    gr.add_option('--vmem-fram', action='store_true')
    gr.add_option('--vmem-server', action='store_true')
    gr.add_option('--vmem-client', action='store_true')
    
    gr.add_option('--rparam-client', action='store_true')
    gr.add_option('--rparam-client-slash', action='store_true')
    gr.add_option('--rparam-server', action='store_true')
    gr.add_option('--rparam-store-file', action='store_true')
    

def configure(ctx):

    if ctx.options.vmem:
        ctx.env.append_unique('FILES_VMEM', 'src/vmem/vmem.c')
        ctx.env.append_unique('FILES_VMEM', 'src/vmem/vmem_ram.c')
            
    if ctx.options.vmem_client:
        ctx.env.append_unique('FILES_VMEM', 'src/vmem/vmem_client.c')
        if ctx.env.SLASH_ENABLED:
            ctx.env.append_unique('FILES_VMEM', 'src/vmem/vmem_client_slash.c')
            
    if ctx.options.vmem_server:
        ctx.env.append_unique('FILES_VMEM', 'src/vmem/vmem_server.c')
    	
    if ctx.options.vmem_fram:
        ctx.env.append_unique('FILES_VMEM', 'src/vmem/vmem_fram.c')
        ctx.env.append_unique('FILES_VMEM', 'src/vmem/vmem_fram_secure.c')
        if ctx.env.SLASH_ENABLED:
            ctx.env.append_unique('FILES_VMEM', 'src/vmem/vmem_fram_secure_slash.c')
        ctx.env.append_unique('USE_VMEM', 'driver-fram')
            
    ctx.env.append_unique('FILES_PARAM', 'src/param/param.c')
    ctx.env.append_unique('FILES_PARAM', 'src/param/param_string.c')
    ctx.env.append_unique('FILES_PARAM', 'src/param/param_serializer.c')
    
    if ctx.env.SLASH_ENABLED:
        ctx.env.append_unique('FILES_PARAM', 'src/param/param_slash.c')
        if ctx.options.rparam_client_slash:
            ctx.env.append_unique('FILES_PARAM', 'src/param/rparam_slash.c')
    
    if ctx.options.rparam_server:	
        ctx.env.append_unique('FILES_PARAM', 'src/param/rparam_server.c')
        
    if ctx.options.rparam_client:
        ctx.env.append_unique('FILES_PARAM', 'src/param/rparam.c')
        ctx.env.append_unique('FILES_PARAM', 'src/param/rparam_list.c')
    
    if ctx.options.rparam_store_file: 
        ctx.env.append_unique('FILES_PARAM', 'src/param/rparam_list_store_file.c')


def build(ctx):
    ctx.objects(
        source = ctx.env.FILES_VMEM,
        includes = 'include',
        export_includes = 'include',
        target = 'vmem',
        use = ctx.env.USE_VMEM + ['slash', 'param'])
    
    ctx.objects(
        source = ctx.env.FILES_PARAM,
        includes = 'include', 
        export_includes = 'include',
        target = 'param',
        use = 'csp slash')

def dist(ctx):
    ctx.base_name = 'lib' + APPNAME + '-' + VERSION
    ctx.algo      = 'tar.xz'
    ctx.excl      = '**/.* wscript'
