#ifndef _CLIENT_UTILS_H
#define _CLIENT_UTILS_H

#include <stdint.h>
#include <stdbool.h>

uint32_t client_utils_get_ms(void);
uint32_t client_utils_elapsed_ms(const uint32_t last_ms);
bool client_utils_is_elapsed_ms(const uint32_t last_ms, const uint32_t period_ms);

#endif // _CLIENT_UTILS_H

