/*
 * param_slash.c
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <slash/completer.h>

#include <csp/csp.h>
#include <csp/csp_hooks.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include <param/param_string.h>

#include "param_slash.h"
#include "param_wildcard.h"
#include "time.h"

static char queue_buf[PARAM_SERVER_MTU];
param_queue_t param_queue = { .buffer = queue_buf, .buffer_size = PARAM_SERVER_MTU, .type = PARAM_QUEUE_TYPE_EMPTY, .version = 2 };

enum {
	NO_SLICE = 1,
	EMPTY_ARRAY_SLICE = -2,
	PARAM_NOT_FOUND = -3,
	MISSING_CLOSE_BRACKET = -4,
	SLICE_INPUT_ERROR = -5,
};

static int param_slash_parse_slice(char * arg, int *start_index, int *end_index, int *slice_detected) {
	/**
	 * Function to find offsets and check if slice delimitor is active.
	 * @return Returns an array of three values. Defaults to INT_MIN if no value.
	 * @return Last value is 1 if found.
	*/

	/* Search for the '@' symbol:
	 * Call strtok twice in order to skip the stuff head of '@' */

	char _slice_delimitor;
	char * saveptr;
	char * token;

	int first_scan = 0;
	int second_scan = 0;

	/* Search for the '[' symbol: */
	strtok_r(arg, "[", &saveptr);
	token = strtok_r(NULL, "[", &saveptr);
	if (token != NULL) {
		// Check if close bracket exists as last element in token.
		// If not, then return an error.
		if(token[strlen(token)-1] != ']'){
			fprintf(stderr, "Missing close bracket on array slice.\n");
			return MISSING_CLOSE_BRACKET;
		}

		// Check if slice input error, by checking if there are more characters after closing bracket. 
		// Niche case, since closing bracket is detected as last element in the check above.
		// Case: set test_array_param[3]:5] | set test_array_param[3:]] 4
		int previous_token_length = strlen(token);
		strtok_r(token, "]", &saveptr);
		if ((previous_token_length - strlen(token)) > 1) {
			fprintf(stderr, "Slice input error. Cannot parse characters after closing bracket.\n");
			return SLICE_INPUT_ERROR;
		}

		// Searches for the format [digit:digit] and [digit:].
		first_scan = sscanf(token, "%d%c%d", start_index, &_slice_delimitor, end_index);

		if (first_scan <= 0) {
			// If the input was ":4" then no offsets will be set by the first sscanf, 
			// so we check for this and try again with another format to match.
			// second scan will never be 0, since a digit can still be loaded into %c.
			second_scan = sscanf(token, "%c%d", &_slice_delimitor, end_index);
		}

		if (_slice_delimitor == ':') {
			*slice_detected = 1;

			// Handle if second slice arg is invalid
			if(first_scan == 2 || second_scan == 1){
				// First_scan == 2:
				// Token could either be a parsable value such as '2:' 
				// or an invalid value such as '2:abc'.
				// '2:' Should pass. first_slice_arg is 2. second_slice_arg is NULL 
				// '2:abc' Should fail. first_slice_arg is 2. second_slice_arg is abc

				// Second_scan == 1
				// Token could either be a parsable value such as ':'
				// or an invalid value such as ':a'
				// ':' Should pass. first_slice_arg is NULL. second_slice_arg is NULL 
				// ':a' Should fail. first_slice_arg is a. second_slice_arg is NULL

				char * saveptr2;				
				char * first_slice_arg = strtok_r(token, ":", &saveptr2);
				char * second_slice_arg = strtok_r(NULL, ":", &saveptr2);

				if (first_slice_arg && !second_slice_arg) {
					// values such as: '2:' and ':a'.

					// Check if first slice arg is not a number
					// if it isnt a number, then value is invalid format such as: ':a'
					// If it is a number the value is valid format such as: '2:'
					char * endptr;
					if (strtoul(first_slice_arg, &endptr, 10) == 0 && strcmp(first_slice_arg, "0") != 0) {
						fprintf(stderr, "Second slice arg is invalid.\nCan only parse integers as indexes to slice by.\n");
						return SLICE_INPUT_ERROR;
					}
				}
				else if(first_slice_arg && second_slice_arg){
					// Values such as: '2:abc'.
					// If both values are set and first_scan is 2 or second_scan is 1,
					// then an error has occured.
					fprintf(stderr, "Second slice arg is invalid.\nCan only parse integers as indexes to slice by.\n");
					return SLICE_INPUT_ERROR;
				}

			}

		}
		else if (second_scan > 0) {
			// If slice_delimitor is not ':' but second_scan still found something, then it must invalid input such as letters.
			fprintf(stderr, "Can only parse integers as indexes to slice by and ':' as delimitor.\n");
			return SLICE_INPUT_ERROR;
		}
		else if (first_scan >= 2) {
			// First scan found a number and a delimitor. But if the delimitor is not ':', then the input could be a decimal value, which is not allowed.
			fprintf(stderr, "Can only parse integers as indexes to slice by.\nSlice delimitor has to be ':'.\n");
			return SLICE_INPUT_ERROR;
		}

		if (second_scan > 0 && _slice_delimitor == ']') {
			// This is an error, example: set test_array[] 4
			fprintf(stderr, "Cannot set empty array slice.\n");
			return EMPTY_ARRAY_SLICE;
		}

		*token = '\0';
	}
	else {
		return NO_SLICE;
	}

	// 5 outcomes:
	// 1. [4]    ->    first_scan == 2 | second_scan == 0
	// 2. [4:]   ->    first_scan == 2 | second_scan == 0 
	// 3. [4:7]  ->    first_scan == 3 | second_scan == 0
 	// 4. [:]    ->    first_scan == 0 | second_scan == 1
	// 5. [:7]   ->    first_scan == 0 | second_scan == 2
	// 1st and 2nd outcome we need to check on _slice_delimitor, if it is : then set slice_detected to 1
	return 0;
}

static int param_parse_from_str(int node, char * arg, param_t **param) {
	char *endptr;
	int id = strtoul(arg, &endptr, 10);
	// If strtoul has an error, then it will return ULONG_MAX, so we check on that.
	if (id != ULONG_MAX && *endptr == '\0') {
		*param = param_list_find_id(node, id);
	} else {
		*param = param_list_find_name(node, arg);
	}

	if (*param == NULL) {
		return -1;
	}

	return 0;
}

// Find out if amount of values is equal to any of the parsed amounts.
static int parse_slash_values(char ** args, int start_index, int expected_values_amount, ...) {
	va_list expected_values;
	va_start(expected_values, expected_values_amount);
	
	int amount_of_values = 0;

	for (int i = start_index; args[i] != NULL; i++) {
		amount_of_values++;

		if (amount_of_values > 99) {
			break;
		}
	}

	for (int i = 0; i < expected_values_amount; i++) {
		if (amount_of_values == va_arg(expected_values, int)) {
			va_end(expected_values);
			return 1;
		}

	}

	va_end(expected_values);
	va_start(expected_values, expected_values_amount);
	fprintf(stderr, "Value amount error. Got %d values. Expected:", amount_of_values);

	for (int i = 0; i < expected_values_amount; i++) {
		int val = va_arg(expected_values, int);
		fprintf(stderr, " %d", val);
		if (i < expected_values_amount - 1) {
			fprintf(stderr, ",");
		}

	}

	fprintf(stderr, ".\n");
	va_end(expected_values);
	return 0;
}

static int parse_param_array(char * arg, int node, param_t **param, int *start_index, int *end_index, int *slice_detected) {
	if (param_slash_parse_slice(arg, start_index, end_index, slice_detected) < 0) {
		return -1;
	}

	if (param_parse_from_str(node, arg, param) < 0) {
		if (*param == NULL) {
			fprintf(stderr, "%s not found.\n", arg);
			return PARAM_NOT_FOUND;
		}

		return -1;
	}

	return 0;
}

static void param_slash_parse(char * arg, int node, param_t **param, int *offset) {

	/* Search for the '@' symbol:
	 * Call strtok twice in order to skip the stuff head of '@' */
	char * saveptr;
	char * token;

	/* Search for the '[' symbol: */
	strtok_r(arg, "[", &saveptr);
	token = strtok_r(NULL, "[", &saveptr);
	if (token != NULL) {
		sscanf(token, "%d", offset);
		*token = '\0';
	}

	char *endptr;
	int id = strtoul(arg, &endptr, 10);

	if (*endptr == '\0') {
		*param = param_list_find_id(node, id);
	} else {
		*param = param_list_find_name(node, arg);
	}

	return;

}

static void param_completer(struct slash *slash, char * token) {

	int matches = 0;
	size_t prefixlen = -1;
	param_t *prefix = NULL;
	char * orig_slash_buf = NULL;
	char *skip_prefix = NULL;

	/* TODO: find better way than hardcoding the command names */
	if (!strncmp(slash->buffer, "get", strlen("get"))) {
		skip_prefix = "get";
	} else if (!strncmp(slash->buffer, "set", strlen("set"))) {
		skip_prefix = "set";
	} else if (!strncmp(slash->buffer, "cmd add", strlen("cmd add"))) {
		skip_prefix = "cmd add";
	}

	if (skip_prefix) {
		orig_slash_buf = slash->buffer;
		slash_completer_skip_flagged_prefix(slash, skip_prefix);
		token = slash->buffer;
	}

	size_t tokenlen = strlen(token);

	param_t * param;
	param_list_iterator i = { };
	if (has_wildcard(token, strlen(token))) {
		// Only print parameters when globbing is involved.
		while ((param = param_list_iterate(&i)) != NULL)
			if (strmatch(param->name, token, strlen(param->name), strlen(token)))
				param_print(param, -1, NULL, 0, 2, 0);
		return;
	}

	while ((param = param_list_iterate(&i)) != NULL) {

		if (tokenlen > strlen(param->name))
			continue;

		if (strncmp(token, param->name,
				slash_min(strlen(param->name), tokenlen)) == 0) {

			/* Count matches */
			matches++;

			/* Find common prefix */
			if (prefixlen == (size_t) -1) {
				prefix = param;
				prefixlen = strlen(prefix->name);
			} else {
				size_t new_prefixlen = slash_prefix_length(prefix->name,
						param->name);
				if (new_prefixlen < prefixlen)
					prefixlen = new_prefixlen;
			}

			/* Print newline on first match */
			if (matches == 1)
				slash_printf(slash, "\n");

			/* Print param */
			param_print(param, -1, NULL, 0, 2, 0);

		}

	}

	if (!matches) {
		slash_bell(slash);
	} else {
		strncpy(token, prefix->name, prefixlen);
		token[prefixlen] = 0;
		slash->cursor = slash->length = (token - slash->buffer) + prefixlen;
	}

	if (skip_prefix) slash_completer_revert_skip(slash, orig_slash_buf);
}

static int cmd_get(struct slash *slash) {

	int node = slash_dfl_node;
	int paramver = 2;
	int server = 0;

    optparse_t * parser = optparse_new("get", "<name>");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_int(parser, 's', "server", "NUM", 0, &server, "server to get parameters from (default = node))");
    optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system verison (default = 2)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Check if name is present */
	if (++argi >= slash->argc) {
        optparse_del(parser);
		printf("missing parameter name\n");
		return SLASH_EINVAL;
	}

	char * name = slash->argv[argi];
	int offset = -1;
	param_t * param = NULL;

	/* Go through the list of parameters */
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

		/* Name match (with wildcard) */
		if (strmatch(param->name, name, strlen(param->name), strlen(name)) == 0) {
			continue;
		}

		/* Node match */
		if (param->node != node) {
			continue;
		}

		/* Local parameters are printed directly */
		if ((param->node == 0) && (server == 0)) {
			param_print(param, -1, NULL, 0, 0, 0);
			continue;
		}

		/* Select destination, host overrides parameter node */
		int dest = node;
		if (server > 0)
			dest = server;

		if (param_pull_single(param, offset, CSP_PRIO_HIGH, 1, dest, slash_dfl_timeout, paramver) < 0) {
			printf("No response\n");
            optparse_del(parser);
			return SLASH_EIO;
		}
		
	}

    optparse_del(parser);
	return SLASH_SUCCESS;

}
slash_command_completer(get, cmd_get, param_completer, "<param>", "Get");

static int cmd_set(struct slash *slash) {
	
	int node = slash_dfl_node;
	int paramver = 2;
	int server = 0;
	int ack_with_pull = true;
	int force = false;

    optparse_t * parser = optparse_new("set", "<name>[offset] <value>");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_int(parser, 's', "server", "NUM", 0, &server, "server to set parameters on (default = node)");
    optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");
	optparse_add_set(parser, 'a', "no_ack_push", 0, &ack_with_pull, "Disable ack with param push queue");
    optparse_add_set(parser, 'f', "force", true, &force, "force setting readonly params");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Check if name is present */
	if (++argi >= slash->argc) {
		printf("missing parameter name\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * name = slash->argv[argi];

	// offset array, since 2 offsets are possible, i.e.: [2:5].
	// Default set to INT_MIN to determine if they've been set or not, since an offset can be < 0.
	// int offsets[2] = {INT_MIN, INT_MIN};
	// int slice_detected = false;
	param_t * param = NULL;
	int start_index = INT_MIN;
	int end_index = INT_MIN;
	int slice_detected = 0;

	int param_parse = parse_param_array(name, node, &param, &start_index, &end_index, &slice_detected);

	if (param_parse < 0) {
		fprintf(stderr, "Param parsing error\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	
	if (param == NULL) {
		printf("%s not found\n", name);
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (param->array_size == 1) {
		if (start_index != INT_MIN || end_index != INT_MIN || slice_detected) {
			fprintf(stderr, "Cannot do array and slicing operations on a non-array parameter.\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}

	}

	// Set flag if only a single index was entered.
	int single_offset_flag = start_index != INT_MIN && end_index == INT_MIN && !slice_detected ? true : false;

	// Make sure start and end index is set to something NOT INT_MIN.
	start_index = start_index != INT_MIN ? start_index : 0;
	end_index = end_index != INT_MIN ? end_index : param->array_size;

	// And index can be negative, if so we should translate it into a not negative index. 
	// i.e.: [-1] is the same as [7] in array [1 2 3 4 5 6 7 8].
	// param->array_size usually == 8, then 8 + -1 = 7.
	if (start_index < 0) {
		start_index = param->array_size + start_index;
		// If the index is still less than 0, then we have an error.
		if (start_index < 0) {
			optparse_del(parser);
			return SLASH_EINVAL;
		}

	}

	// Same goes for the end index.
	if (end_index < 0) {
		end_index = param->array_size + end_index;
		if (end_index < 0) {
			optparse_del(parser);
			return SLASH_EINVAL;
		}

	}


	if (start_index >= end_index) {
		fprintf(stderr, "start index is greater than end index in array slice.\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}
	
	if (end_index > param->array_size) {
		fprintf(stderr, "End index in slice is greater than param array size.\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}


	if (param->mask & PM_READONLY && !force) {
		printf("--force is required to set a readonly parameter\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Check if Value is present */
	if (++argi >= slash->argc) {
		printf("missing parameter value\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
	
	int expected_value_amount = end_index - start_index;

	// Parse expected amount of values
	// We expect either a single value or the amount defined by the slice.
	int arg_parse_values_amount = parse_slash_values(slash->argv, argi, 2, 1, expected_value_amount);
	if (arg_parse_values_amount == 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	// Create a queue, so that we can set the param in a single packet.
	param_queue_t queue;
	char queue_buf[PARAM_SERVER_MTU];
	param_queue_init(&queue, queue_buf, PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, 2);

	// We should iterate until we find an ending bracket ']'. Therefore we use the 'should_break' flag.
	int should_break = 1;
	int iterations = 0;
	int single_value_flag = 0;
	for (int i = argi; should_break == 1; i++) {
		char valuebuf[128] __attribute__((aligned(16))) = { };

		char *arg = slash->argv[i];
		if (!arg) {
			fprintf(stderr, "Missing closing bracket\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		// Check if we can find a start bracket '['.
		// If we can, then we're dealing with a value array.
		if (strchr(arg, '[')) {
			// If we're dealing with a value array, but only a single offset, then we should throw an error.
			if (single_offset_flag) {
				fprintf(stderr, "cannot set array into indexed parameter value\n");
				optparse_del(parser);
				return SLASH_EINVAL;
			}

			arg++;
			// A value array with a single element can also be passed.
			if (arg[strlen(arg)-1] == ']') {
				arg[strlen(arg)-1] = '\0';
				should_break = 0;
			}

		}
		// Check if we can find an ending brakcet ']'.
		// If we can, then we should stop looping after the current iteration.
		else if (arg[strlen(arg)-1] == ']') {
			arg[strlen(arg)-1] = '\0';
			should_break = 0;
		}
		// If no bracket was found and the current iteration is the first iteration, then we are dealing with a single value. 
		else if (iterations == 0) {
			single_value_flag = 1;
		}

		// Convert the valuebuf str to a value of the param type. 
		if (param_str_to_value(param->type, arg, valuebuf) < 0) {
			fprintf(stderr, "invalid parameter value\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		// If we're dealing with a single value, we should loop the sliced array instead of the values.
		// Breaking afterwards, since this means we're done setting params.
		if (single_value_flag) {
			if (single_offset_flag) {
				param_queue_add(&queue, param, start_index, valuebuf);
				break;
			}
			for (int j = start_index; j < end_index; j++) {
				param_queue_add(&queue, param, j, valuebuf);
			}

			break;
		}

		// If we're not dealing with a single value, we should look at start_index.
		// If start_index ever becomes equal to end_index, it means the amount of values are greater than the slice. 
		if (start_index == end_index) {
			optparse_del(parser);
			fprintf(stderr, "Values array is longer than specified slice\n");
			return SLASH_EINVAL;
		}

		// Add to the queue.
		if (param_queue_add(&queue, param, start_index++, valuebuf) < 0) {
			optparse_del(parser);
			fprintf(stderr, "Param_queue_add failed\n");
			return SLASH_EINVAL;
		}

		iterations++;

		// If we hit the end value by detecting ']', and start index and end index are not equal, 
		// then the slice length must be greater than the value array length.
		// This shouldnt be possible, so we throw an error.
		if (should_break == 0 && start_index != end_index) {
			optparse_del(parser);
			fprintf(stderr, "Array slice is longer than value array\n");
			return SLASH_EINVAL;
		}

	}

	/* Local parameters are set directly */
	if (param->node == 0) {
		param_queue_apply(&queue, 1, 0);
		param_print(param, -1, NULL, 0, 2, 0);
	} else {

		/* Select destination, host overrides parameter node */
		int dest = node;
		if (server > 0)
			dest = server;
		csp_timestamp_t time_now;
		csp_clock_get_time(&time_now);
		*param->timestamp = 0;
		if (param_push_queue(&queue, 3, dest, slash_dfl_timeout, 0, ack_with_pull) < 0) {
			printf("No response\n");
			optparse_del(parser);
			return SLASH_EIO;
		}
		param_print(param, -1, NULL, 0, 2, time_now.tv_sec);
	}


    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_completer(set, cmd_set, param_completer, "<param> <value>", "Set");



static int cmd_add(struct slash *slash) {
	
	int node = slash_dfl_node;
	char * include_mask_str = NULL;
	char * exclude_mask_str = NULL;
	int force = false;

    optparse_t * parser = optparse_new("cmd add", "<name>[offset] [value]");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_string(parser, 'm', "imask", "MASK", &include_mask_str, "Include mask (param letters) (used for get with wildcard)");
	optparse_add_string(parser, 'e', "emask", "MASK", &exclude_mask_str, "Exclude mask (param letters) (used for get with wildcard)");
    optparse_add_set(parser, 'f', "force", true, &force, "force setting readonly params");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_HWREG;

	if (include_mask_str)
		include_mask = param_maskstr_to_mask(include_mask_str);
	if (exclude_mask_str)
	    exclude_mask = param_maskstr_to_mask(exclude_mask_str);

	/* Check if name is present */
	if (++argi >= slash->argc) {
		printf("missing parameter name\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (param_queue.type == PARAM_QUEUE_TYPE_SET) {

		char * name = slash->argv[argi];
		int offset = -1;
		param_t * param = NULL;
		param_slash_parse(name, node, &param, &offset);

		if (param == NULL) {
			printf("%s not found\n", name);
            optparse_del(parser);
			return SLASH_EINVAL;
		}

		if (param->mask & PM_READONLY && !force) {
			printf("--force is required to set a readonly parameter\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		/* Check if Value is present */
		if (++argi >= slash->argc) {
			printf("missing parameter value\n");
            optparse_del(parser);
			return SLASH_EINVAL;
		}
		
		char valuebuf[128] __attribute__((aligned(16))) = { };
		if (param_str_to_value(param->type, slash->argv[argi], valuebuf) < 0) {
			printf("invalid parameter value\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}
		/* clear param timestamp so we dont set timestamp flag when serialized*/
		*param->timestamp = 0;

		if (param_queue_add(&param_queue, param, offset, valuebuf) < 0)
			printf("Queue full\n");

	}

	if (param_queue.type == PARAM_QUEUE_TYPE_GET) {

		char * name = slash->argv[argi];
		int offset = -1;
		param_t * param = NULL;

		/* Go through the list of parameters */
		param_list_iterator i = {};
		while ((param = param_list_iterate(&i)) != NULL) {

			/* Name match (with wildcard) */
			if (strmatch(param->name, name, strlen(param->name), strlen(name)) == 0) {
				continue;
			}

			/* Node match */
			if (param->node != node) {
				continue;
			}

			if (param->mask & exclude_mask) {
				continue;
			}

			if ((param->mask & include_mask) == 0) {
				continue;
			} 

			/* Queue */
			if (param_queue_add(&param_queue, param, offset, NULL) < 0)
				printf("Queue full\n");
			continue;	
			
		}

	}

	param_queue_print(&param_queue);
    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub_completer(cmd, add, cmd_add, param_completer, "<param>[offset] [value]", "Add a new parameter to a command");

static int cmd_run(struct slash *slash) {

	unsigned int timeout = slash_dfl_timeout;
	unsigned int server = slash_dfl_node;
	unsigned int hwid = 0;
	int ack_with_pull = true;

    optparse_t * parser = optparse_new("run", "");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_unsigned(parser, 's', "server", "NUM", 0, &server, "server to push parameters to (default = <env>))");
	optparse_add_unsigned(parser, 'h', "hwid", "NUM", 16, &hwid, "include hardware id filter (default = off)");
	optparse_add_set(parser, 'a', "no_ack_push", 0, &ack_with_pull, "Disable ack with param push queue");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (param_queue.type == PARAM_QUEUE_TYPE_SET) {

		csp_timestamp_t time_now;
		csp_clock_get_time(&time_now);
		if (param_push_queue(&param_queue, 0, server, timeout, hwid, ack_with_pull) < 0) {
			printf("No response\n");
            optparse_del(parser);
			return SLASH_EIO;
		}
		param_queue_print_params(&param_queue, time_now.tv_sec);

	}

	if (param_queue.type == PARAM_QUEUE_TYPE_GET) {
		if (param_pull_queue(&param_queue, CSP_PRIO_HIGH, 1, server, timeout)) {
			printf("No response\n");
            optparse_del(parser);
			return SLASH_EIO;
		}
	}


    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(cmd, run, cmd_run, "", NULL);

static int cmd_pull(struct slash *slash) {

	unsigned int timeout = slash_dfl_timeout;
	unsigned int server = slash_dfl_node;
	char * include_mask_str = NULL;
	char * exclude_mask_str = NULL;
	int paramver = 2;

    optparse_t * parser = optparse_new("pull", "");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_unsigned(parser, 's', "server", "NUM", 0, &server, "server to pull parameters from (default = <env>))");
	optparse_add_string(parser, 'm', "imask", "MASK", &include_mask_str, "Include mask (param letters)");
	optparse_add_string(parser, 'e', "emask", "MASK", &exclude_mask_str, "Exclude mask (param letters)");
    optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system verison (default = 2)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_REMOTE | PM_HWREG;

	if (include_mask_str)
		include_mask = param_maskstr_to_mask(include_mask_str);
	if (exclude_mask_str)
	    exclude_mask = param_maskstr_to_mask(exclude_mask_str);

	if (param_pull_all(CSP_PRIO_HIGH, 1, server, include_mask, exclude_mask, timeout, paramver)) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EIO;
	}

    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command(pull, cmd_pull, "", "Pull all metrics");

static int cmd_new(struct slash *slash) {

	int paramver = 2;
	char *name = NULL;

    optparse_t * parser = optparse_new("cmd new", "<get/set> <cmd name>");
    optparse_add_help(parser);
    optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system verison (default = 2)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("Must specify 'get' or 'set'\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Set/get */
	if (strncmp(slash->argv[argi], "get", 4) == 0) {
		param_queue.type = PARAM_QUEUE_TYPE_GET;
	} else if (strncmp(slash->argv[argi], "set", 4) == 0) {
		param_queue.type = PARAM_QUEUE_TYPE_SET;
	} else {
		printf("Must specify 'get' or 'set'\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Must specify a command name\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Command name */
	name = slash->argv[argi];
	strncpy(param_queue.name, name, sizeof(param_queue.name)-1);  // -1 to fit NULL byte

	param_queue.used = 0;
	param_queue.version = paramver;
	param_queue.last_timestamp = 0;

	printf("Initialized new command: %s\n", name);

    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(cmd, new, cmd_new, "<get/set> <cmd name>", "Create a new command");


static int cmd_done(struct slash *slash) {
	param_queue.type = PARAM_QUEUE_TYPE_EMPTY;
	return SLASH_SUCCESS;
}
slash_command_sub(cmd, done, cmd_done, "", "Exit cmd edit mode");


static int cmd_print(struct slash *slash) {
	if (param_queue.type == PARAM_QUEUE_TYPE_EMPTY) {
		printf("No active command\n");
	} else {
		printf("Current command size: %d bytes\n", param_queue.used);
		param_queue_print(&param_queue);
	}
	return SLASH_SUCCESS;
}
slash_command(cmd, cmd_print, NULL, "Show current command");
