#pragma once 

#include <param/param_queue.h>

extern param_queue_t param_queue;

/**
 * @brief Load the slash commands defined in libparam
 * Those are stored in a specific ELF section (default: "param_cmds", see meson.build)
 */
extern void param_load_slash_cmds();