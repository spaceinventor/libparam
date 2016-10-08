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

    if not ctx.options.vmem:
    	ctx.env.append_unique('EXCL_PARAM', 'src/vmem.c')
    	ctx.env.append_unique('EXCL_PARAM', 'src/vmem_slash.c')
    	
    if not ctx.options.vmem_fram:
        ctx.env.append_unique('EXCL_PARAM', 'src/vmem_fram.c')
        ctx.env.append_unique('EXCL_PARAM', 'src/vmem_fram_secure.c')
        ctx.env.append_unique('EXCL_PARAM', 'src/vmem_fram_secure_slash.c')
        
    if not ctx.options.vmem_ltc:
        ctx.env.append_unique('EXCL_PARAM', 'src/vmem_ltc.c')
        

    	

def build(ctx):
    ctx.objects(
        source   = ctx.path.ant_glob(['src/*.c'], excl = ctx.env.EXCL_PARAM),
        includes = ['src', 'include'],
        export_includes = ['src', 'include'],
        target   = APPNAME,
        use      = ['csp', 'slash', 'driver-include'])

def dist(ctx):
    ctx.base_name = 'lib' + APPNAME + '-' + VERSION
    ctx.algo      = 'tar.xz'
    ctx.excl      = '**/.* wscript'
