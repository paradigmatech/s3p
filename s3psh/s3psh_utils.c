#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>

uint32_t client_utils_get_ms(void)
{
    struct timeval  tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec*1000U + tv.tv_usec/1000U;
}

uint32_t client_utils_elapsed_ms(const uint32_t last_ms)
{
    return (uint32_t)(client_utils_get_ms() - last_ms);
}

bool client_utils_is_elapsed_ms(const uint32_t last_ms, const uint32_t period_ms)
{
    return client_utils_elapsed_ms(last_ms) >= period_ms;
}

