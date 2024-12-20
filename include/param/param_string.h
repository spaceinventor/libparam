#pragma once

#include <param/param.h>

void param_value_str(param_t *param, unsigned int i, char * out, int len);
int param_str_to_value(param_type_e type, char * in, void * out);
void param_type_str(param_type_e type, char * out, int len);
void param_print(param_t * param, int offset, int nodes[], int nodes_count, int verbose, uint32_t ref_timestamp);
uint32_t param_maskstr_to_mask(const char * str);
uint32_t param_umaskstr_to_mask(const char * str);
uint32_t param_typestr_to_typeid(const char * str);
const char * param_mask_color(const param_t *param);
const char * param_mask_color_off(void);
