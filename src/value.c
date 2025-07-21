#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "value.h"

/* value helper types/funtions source code
 * version: 1.0
 * date   : 2025-07-07
 */

#define VALUE_MIN(_x,_y)        ( ( (_x) > (_y) ) ? (_y) : (_x) )
#define VALUE_SWITCH_SNPRINTF(_casename, _strtype, _cast, _field) \
    case _casename: \
    snprintf(buf, max_size, _strtype, (_cast)value->val._field); \
    break;
void value_dump(char *buf, const value_t * const value, const int max_size)
{
    int size;

    switch (value->vt) {
        VALUE_SWITCH_SNPRINTF(VT_U8,  "%"PRIu8,    uint8_t,  u8)
        VALUE_SWITCH_SNPRINTF(VT_I8,  "%"PRId8,    int8_t,   i8)
        VALUE_SWITCH_SNPRINTF(VT_X8,  "0x%"PRIX8,  uint8_t,  u8)
        VALUE_SWITCH_SNPRINTF(VT_U16, "%"PRIu16,   uint16_t, u16)
        VALUE_SWITCH_SNPRINTF(VT_I16, "%"PRId16,   int16_t,  i16)
        VALUE_SWITCH_SNPRINTF(VT_X16, "0x%"PRIX16, uint16_t, u16)
        VALUE_SWITCH_SNPRINTF(VT_U32, "%"PRIu32,   uint32_t, u32)
        VALUE_SWITCH_SNPRINTF(VT_I32, "%"PRId32,   int32_t,  i32)
        VALUE_SWITCH_SNPRINTF(VT_X32, "0x%"PRIX32, uint32_t, u32)
        VALUE_SWITCH_SNPRINTF(VT_FLT, "%.3f",      float,    flt)

        case VT_EMPTY:
            snprintf(buf, max_size, "%s", "EMPTY");
            break;
        case VT_STR:
            size = VALUE_MIN(value->val.str->max_size, max_size);
            snprintf(buf, size, "%s", value->val.str->ptr);
            //buf[size-1] = '\0';
            break;
        default:
            break;
    }
}

const char *value_type_str(const value_type_t vt)
{
    switch (vt) {
    case VT_EMPTY:  return "ee";
    case VT_U8:     return "u8";
    case VT_I8:     return "i8";
    case VT_X8:     return "x8";
    case VT_U16:    return "u16";
    case VT_I16:    return "i16";
    case VT_X16:    return "x16";
    case VT_U32:    return "u32";
    case VT_I32:    return "i32";
    case VT_X32:    return "x32";
    case VT_FLT:    return "flt";
    case VT_STR:    return "str";
    default: break;
    }
    return "nn";
}

value_type_t value_type_from_str(const char *str)
{
    for (int id=0; id<VT_CNT; id++) {
        if (!strcmp(str, value_type_str(id)))
            return (value_type_t)id;
    }
    return VT_EMPTY;
}

