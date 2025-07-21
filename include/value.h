#ifndef _VALUE_H
#define _VALUE_H

/* value helper types/funtions include
 * version: 1.0
 * date   : 2025-07-07
 */

#include <stdint.h>

#define VALUE_SCALAR_MAX_SIZE   16
#define VALUE_STR_MAX_SIZE      255

#define VALUE_TYPE_IS_SCALAR(_vt)   (_vt != VT_STR)

typedef enum {
    VT_EMPTY = 0,
    VT_U8,
    VT_I8,
    VT_X8,
    VT_U16,
    VT_I16,
    VT_X16,
    VT_U32,
    VT_I32,
    VT_X32,
    VT_FLT,
    VT_STR,
    // Last
    VT_CNT
} value_type_t;

typedef struct {
    char * const ptr;
    uint8_t max_size;        // max len + null byte
} str_t;

typedef struct {
    union {
        uint8_t u8;
        int8_t i8;
        uint16_t u16;
        int16_t i16;
        uint32_t u32;
        int32_t i32;
        float flt;
        str_t *str;
    } val;
    value_type_t vt;
} value_t;

extern void value_dump(char *buf, const value_t * const value, const int max_size);
extern const char *value_type_str(const value_type_t vt);
extern value_type_t value_type_from_str(const char *str);

#endif // _VALUE_H

