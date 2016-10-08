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
    gr.add_option('--vmem-ltc', action='store_true')

def configure(ctx):

    if ctx.options.vmem:
    	ctx.env.append_unique('FILES_VMEM', 'src/vmem.c')
    	ctx.env.append_unique('FILES_VMEM', 'src/vmem_ram.c')
    	ctx.env.append_unique('FILES_VMEM', 'src/vmem_slash.c')
    	
    if ctx.options.vmem_fram:
        ctx.env.append_unique('FILES_VMEM', 'src/vmem_fram.c')
        ctx.env.append_unique('FILES_VMEM', 'src/vmem_fram_secure.c')
        ctx.env.append_unique('FILES_VMEM', 'src/vmem_fram_secure_slash.c')
        ctx.env.append_unique('USE_VMEM', 'driver-fram')
        
    if ctx.options.vmem_ltc:
        ctx.env.append_unique('FILES_VMEM', 'src/vmem_ltc.c')
        ctx.env.append_unique('USE_VMEM', 'driver-ltc')

def build(ctx):
    ctx.objects(
        source = ctx.env.FILES_VMEM,
        export_includes = 'include',
        target = 'vmem',
        use = ctx.env.USE_VMEM + ['slash'])
        
    ctx.objects(
        source = ctx.path.ant_glob('src/param/*.c'),
        includes = 'include',
        export_includes = 'include',
        target = 'param',
        use = 'csp slash')

def dist(ctx):
    ctx.base_name = 'lib' + APPNAME + '-' + VERSION
    ctx.algo      = 'tar.xz'
    ctx.excl      = '**/.* wscript'
