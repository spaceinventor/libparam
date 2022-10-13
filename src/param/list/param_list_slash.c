/*
 * param_list_slash.c
 *
 *  Created on: Dec 14, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <vmem/vmem_client.h>

#include <csp/csp_crc32.h>
#include <libparam.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_string.h>
#include "param_list.h"
#include "../param_slash.h"

static int list(struct slash *slash)
{
    int node = slash_dfl_node;
    int verbosity = 1;
    char * maskstr = NULL;

    optparse_t * parser = optparse_new("list", "[name wildcard=*]");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_int(parser, 'v', "verbosity", "NUM", 0, &verbosity, "verbosity (default = 1, max = 3)");
    optparse_add_string(parser, 'm', "mask", "STR", &maskstr, "mask string");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    /* Interpret maskstring */
    uint32_t mask = 0xFFFFFFFF;
    if (maskstr != NULL) {
        mask = param_maskstr_to_mask(maskstr);
    }

    char * globstr = NULL;
    if (++argi < slash->argc) {
        globstr = slash->argv[argi];
    }

    param_list_print(mask, node, globstr, verbosity);

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command(list, list, "[OPTIONS...] [name wildcard=*]", "List parameters");


static int list_download(struct slash *slash)
{
    unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int version = 2;

    optparse_t * parser = optparse_new("list download", NULL);
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 2)");
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (++argi < slash->argc) {
        printf("Node as argument, is deprecated\n");
        node = atoi(slash->argv[argi]);
    }

    param_list_download(node, timeout, version);

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(list, download, list_download, "[node]", NULL);

static int list_upload(struct slash *slash)
{
    unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int list_version = 2;
    unsigned int vmem_version = 1;
    unsigned int remote_only = 1;
    int prio_only = 1;

    optparse_t * parser = optparse_new("list upload <address>", NULL);
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    // optparse_add_set(parser, 'p', "prio", 0, &prio_only, "upload params with priority configured only (default true)");
    optparse_add_unsigned(parser, 'r', "remote_only", "NUM", 0, &remote_only, "upload only remote parameters (default true)");
    optparse_add_unsigned(parser, 'l', "list_version", "NUM", 0, &vmem_version, "list version (default = 2)");
    optparse_add_unsigned(parser, 'v', "vmem_version", "NUM", 0, &vmem_version, "vmem version (default = 1)");
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    const uint32_t header_size = 4;

    char data[0x4000+header_size];

    int num_params = param_list_pack(data+header_size, sizeof(data)-header_size, prio_only, remote_only, list_version);

	uint32_t crc = csp_crc32_memory((uint8_t*)data+header_size, param_list_packed_size(list_version) * num_params);
    *(uint32_t*)data = htobe32(crc);

	/* Expect address */
	if (++argi >= slash->argc) {
		printf("missing address\n");
		return SLASH_EINVAL;
	}

	char * endptr;
	uint64_t address = strtoul(slash->argv[argi], &endptr, 16);
	if (*endptr != '\0') {
		printf("Failed to parse address\n");
		return SLASH_EUSAGE;
	}

	printf("About to upload %u bytes\n", param_list_packed_size(list_version) * num_params + header_size);
	printf("Type 'yes' + enter to continue: ");
	char * c = slash_readline(slash);
    
    if (strcmp(c, "yes") != 0) {
        printf("Abort\n");
        return SLASH_EUSAGE;
    }

    vmem_upload(node, timeout, address, data, param_list_packed_size(list_version) * num_params + header_size, vmem_version);

    printf("Uploaded %i parameters\n", num_params);
    printf("Please configure remote module to enable new list\n");

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(list, upload, list_upload, "", NULL);

static int list_forget(struct slash *slash)
{

    int node = slash_dfl_node;

    optparse_t * parser = optparse_new("list forget", NULL);
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (++argi < slash->argc) {
        printf("Node as argument, is deprecated\n");
        node = atoi(slash->argv[argi]);
    }

    printf("Removed %i parameters\n", param_list_remove(node, 1));

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(list, forget, list_forget, "[node]", "Forget remote parameters. Omit or set node to -1 to include all.");


static int list_add(struct slash *slash)
{
    unsigned int node = slash_dfl_node;
    unsigned int array_len = 0;
    char * helpstr = NULL;
    char * unitstr = NULL;
    char * maskstr = NULL;

    optparse_t * parser = optparse_new("list add", "<name> <id> <type>");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 'a', "array", "NUM", 0, &array_len, "array length (default = none)");
    optparse_add_string(parser, 'c', "comment", "STRING", (char **) &helpstr, "help text");
    optparse_add_string(parser, 'u', "unit", "STRING", (char **) &unitstr, "unit text");
	optparse_add_string(parser, 'm', "emask", "STRING", &maskstr, "mask (param letters)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing parameter name\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * name = slash->argv[argi];

	if (++argi >= slash->argc) {
		printf("missing parameter id\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

    char * endptr;
    unsigned int id = strtoul(slash->argv[argi], &endptr, 10);

	if (++argi >= slash->argc) {
		printf("missing parameter type\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

    char * type = slash->argv[argi];

    unsigned int typeid = param_typestr_to_typeid(type);

    if (typeid == PARAM_TYPE_INVALID) {
        printf("Invalid type %s\n", type);
        optparse_del(parser);
        return SLASH_EINVAL;
    }


	uint32_t mask = 0;

	if (maskstr)
		mask = param_maskstr_to_mask(maskstr);

    //printf("name %s, id %u, type %s, typeid %u, mask %x, arraylen %u, help %s, unit %s\n", name, id, type, typeid, mask, array_len, helpstr, unitstr);

    param_t * param = param_list_create_remote(id, node, typeid, mask, array_len, name, unitstr, helpstr, -1);
    if (param == NULL) {
        printf("Unable to create param\n");
        optparse_del(parser);
        return SLASH_EINVAL;
    }

    param_list_add(param);

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(list, add, list_add, "<name> <id> <type>", NULL);


static int list_save(struct slash *slash) {

    char * filename = NULL;
    int node = slash_dfl_node;

    optparse_t * parser = optparse_new("list save", "[name wildcard=*]");
    optparse_add_help(parser);
	optparse_add_string(parser, 'f', "filename", "PATH", &filename, "write to file");
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    FILE * out = stdout;

    if (filename) {
	    FILE * fd = fopen(filename, "w");
        if (fd) {
            out = fd;
            printf("Writing to file %s\n", filename);
        }
    }

    param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

        if ((node >= 0) && (param->node != node)) {
			continue;
		}

        fprintf(out, "list add ");
        if (param->array_size > 1) {
            fprintf(out, "-a %u ", param->array_size);
        }
        if ((param->docstr != NULL) && (strlen(param->docstr) > 0)) {
            fprintf(out, "-c \"%s\" ", param->docstr);
        }
        if ((param->unit != NULL) && (strlen(param->unit) > 0)) {
            fprintf(out, "-u \"%s\" ", param->unit);
        }
        if (param->node != 0) {
            fprintf(out, "-n %u ", param->node);
        }
        
		if (param->mask > 0) {
			unsigned int mask = param->mask;

			fprintf(out, "-m \"");

			if (mask & PM_READONLY) {
				mask &= ~ PM_READONLY;
				fprintf(out, "r");
			}

			if (mask & PM_REMOTE) {
				mask &= ~ PM_REMOTE;
				fprintf(out, "R");
			}

			if (mask & PM_CONF) {
				mask &= ~ PM_CONF;
				fprintf(out, "c");
			}

			if (mask & PM_TELEM) {
				mask &= ~ PM_TELEM;
				fprintf(out, "t");
			}

			if (mask & PM_HWREG) {
				mask &= ~ PM_HWREG;
				fprintf(out, "h");
			}

			if (mask & PM_ERRCNT) {
				mask &= ~ PM_ERRCNT;
				fprintf(out, "e");
			}

			if (mask & PM_SYSINFO) {
				mask &= ~ PM_SYSINFO;
				fprintf(out, "i");
			}

			if (mask & PM_SYSCONF) {
				mask &= ~ PM_SYSCONF;
				fprintf(out, "C");
			}

			if (mask & PM_WDT) {
				mask &= ~ PM_WDT;
				fprintf(out, "w");
			}

			if (mask & PM_DEBUG) {
				mask &= ~ PM_DEBUG;
				fprintf(out, "d");
			}

			if (mask & PM_ATOMIC_WRITE) {
				mask &= ~ PM_ATOMIC_WRITE;
				fprintf(out, "o");
			}

			if (mask & PM_CALIB) {
				mask &= ~ PM_CALIB;
				fprintf(out, "q");
			}

            switch(mask & PM_PRIO_MASK) {
                case PM_PRIO1: fprintf(out, "1"); mask &= ~ PM_PRIO_MASK; break;
                case PM_PRIO2: fprintf(out, "2"); mask &= ~ PM_PRIO_MASK; break;
                case PM_PRIO3: fprintf(out, "3"); mask &= ~ PM_PRIO_MASK; break;				
			}

			//if (mask)
			//	fprintf(out, "+%x", mask);

            fprintf(out, "\" ");

		}
		
        fprintf(out, "%s %u ", param->name, param->id);

        char typestr[10];
        param_type_str(param->type, typestr, 10);
        fprintf(out, "%s\n", typestr);

	}

    if (out != stdout) {
        fflush(out);
        fclose(out);
    }


    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(list, save, list_save, "", "Save parameters");
