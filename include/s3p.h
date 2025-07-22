#ifndef _S3P_H
#define _S3P_H

#include <stdint.h>
#include <stdbool.h>

// Version
#define S3P_VERSION           0x0100    // 1.00

#define S3P_MAX_FRAME_SIZE    1024
#define S3P_MAX_PKT_SIZE      1018
#define S3P_MAX_DATA_SIZE     1010
#define S3P_MAX_CHUNK_SIZE    1004
#define S3P_COBS_DELIM        0x00
#define S3P_PKT_NACK          0x80
#define S3P_SEQ_MASKED(_s)    ((_s) & 0x0F)
#define S3P_SER_ITEM_SIZE     7
#define S3P_MAX_NAME_SIZE     32

#define S3P_ERR_NONE          0
#define S3P_ERR_VMEM_XLATE    100
#define S3P_ERR_NO_REG        101
#define S3P_ERR_NO_LOCK       102
#define S3P_ERR_TYPE          103
#define S3P_ERR_SIZE          104
#define S3P_ERR_NO_WRITE      105
#define S3P_ERR_NO_VMEM       106
#define S3P_ERR_NO_CMD        107

typedef struct {
    uint8_t src_id;     // Source node id
    uint8_t dst_id;     // Destination node id
    uint8_t flags_seq;  // Flags(RESERVED) & sequence
    uint8_t type;       // Request/response type
    uint16_t data_len;  // Length of the Data section (payload)
    uint8_t *buf;       // Pointer to packet buffer to be encoded or decoded
    uint8_t *data;      // Pointer to the Data section of packet
} s3p_packet_t;

typedef enum {
    PT_NONE               = 0,
    PT_EXEC_CMD           = 0x10,
    PT_EXEC_CMD_RESP      = 0x11,
    PT_READ_REGS          = 0x12,
    PT_READ_REGS_RESP     = 0x13,
    PT_WRITE_REG          = 0x14,
    PT_WRITE_REG_RESP     = 0x15,
    PT_READ_VMEM          = 0x16,
    PT_READ_VMEM_RESP     = 0x17,
    PT_WRITE_VMEM         = 0x18,
    PT_WRITE_VMEM_RESP    = 0x19,
    PT_READ_STR_REG       = 0x1A,
    PT_READ_STR_REG_RESP  = 0x1B,
    PT_WRITE_STR_REG      = 0x1C,
    PT_WRITE_STR_REG_RESP = 0x1D,
    PT_S3P_INFO           = 0x30,
    PT_S3P_INFO_RESP      = 0x31,
    PT_REG_INFO           = 0x32,
    PT_REG_INFO_RESP      = 0x33,
    PT_VMEM_INFO          = 0x34,
    PT_VMEM_INFO_RESP     = 0x35,
} pkt_type_t;

typedef enum {
    CT_PING   = 0x10,
    CT_REBOOT = 0x11,
} cmd_type_t;

extern void s3p_set_debug_level(const int level);
extern bool s3p_parse_frame(s3p_packet_t *pkt, const uint8_t dst_id,
        const uint8_t *frame_buf, uint16_t len);
extern void s3p_init_pkt_out(s3p_packet_t *pkt_out, uint8_t *pkt_buf,
        const uint8_t src_id, const uint8_t dst_id,
        const uint8_t flags_seq);
extern uint16_t s3p_make_frame(uint8_t *buf, const s3p_packet_t *pkt_out);
extern const char *s3p_err_str(const uint8_t code);

#endif // _S3P_H

