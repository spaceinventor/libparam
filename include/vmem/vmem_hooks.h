#pragma once

#include <stdint.h>

void vmem_server_user_handler(uint8_t type, void *conn, void *packet);
