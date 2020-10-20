#!/usr/bin/env python3

# pre-requisites:
#   pip3 install pyelftools

import sys, hexdump, struct

from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

def find_symbol(elffile, names = []):
    symbol_tables = [s for s in elffile.iter_sections() if isinstance(s, SymbolTableSection)]

    result = {}

    for section in symbol_tables:
        if section['sh_entsize'] == 0:
            continue

        for nsym, symbol in enumerate(section.iter_symbols()):
        	if symbol.name in names:
        		result[symbol.name] = symbol
    return result

param_definition_v1 = {
	'size': 44,
	'definition': '<HBBLLLLLllLLL',
	'fields': [
		'id', # 2 uint16_t
		'node', # 1 uint8_t
		'type', # 1 param_type_e
		'mask', # 4 uint32_t
		'name', # 4 char *
		'unit', # 4 char *

		# Storage
		'addr', # 4 void *
		'vmem', # 4 const struct vmem_s *
		'array_size', # 4 int
		'array_step', # 4 int

		# Local info
		'callback', # 4 void (*callback)(const struct param_s * param, int offset);
		'timestamp', # 4 uint32_t timestamp;
		'next',
		# TOTAL: 44 bytes
	]
}

param_definition_v2 = {
	'size': 52,
	'definition': '<HBBLLLLLllllLLL',
	'fields': [
		'id', # 2 uint16_t
		'node', # 1 uint8_t
		'type', # 1 param_type_e
		'mask', # 4 uint32_t
		'name', # 4 char *
		'unit', # 4 char *

		# Storage
		'addr', # 4 void *
		'vmem', # 4 const struct vmem_s *
		'group_size', # 4 int
		'group_step', # 4 int
		'array_size', # 4 int
		'array_step', # 4 int

		# Local info
		'callback', # 4 void (*callback)(const struct param_s * param, int offset);
		'timestamp', # 4 uint32_t timestamp;
		'next',
		# TOTAL: 52 bytes
	]
}

def get_null_terminated_string(data, ptr):
	result = ''

	# Max string length is 256
	for i in range(256):
		if data[ptr+i] == 0:
			break
		result += chr(data[ptr+i])
	return result

def decode_param(definition, data, offset, section_data):
	try:
		ar = struct.unpack(definition['definition'], data[:definition['size']])
		result = {definition['fields'][i]: ar[i] for i in range(len(definition['fields']))}
		for field in ['name', 'unit']:
			if result[field] == 0:
				result[field] = ''
			else:
				result[field] = get_null_terminated_string(section_data, result[field] - offset)
	except:
		print("Failed to decode parameter definition from:", hexdump.hexdump(data))
	return result

def get_params(definition, filename):
	with open(filename, 'rb') as f:
	    elffile = ELFFile(f)
	    pointers = find_symbol(elffile, ['__start_param', '__stop_param'])
	    text = elffile.get_section_by_name('.text')
	    data = text.data()
	    start = pointers['__start_param']['st_value'] - text.header.sh_addr
	    stop = pointers['__stop_param']['st_value'] - text.header.sh_addr
	    param_data = data[start:stop]
	    param_size = definition['size']

	    return [decode_param(definition, param_data[i*param_size:(i+1)*param_size], text.header.sh_addr, data) for i in range(len(param_data) // param_size)]

import argparse
opts = argparse.ArgumentParser()
opts.add_argument('--v2', action='store_true', default=False, help='Use new parameter definition (with groups).')
args,rest = opts.parse_known_args()

if args.v2:
	definition = param_definition_v2
else:
	definition = param_definition_v1

for filename in rest:
	print("Extracting parameters from: ", filename)
	params = get_params(definition, filename)
	for param in params:
		print(', '.join([field + ':' + str(param[field]) for field in definition['fields']]))
