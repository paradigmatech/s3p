/**
@file s3p.h
@brief S3P main header
*/

#ifndef _S3P_H
#define _S3P_H

#include <stdint.h>
#include <stdbool.h>

/** @brief S3P library version */
#define S3P_VERSION           0x0100    // 1.00

/** @brief Max serial frame size */
#define S3P_MAX_FRAME_SIZE    1024
/** @brief Max packet size */
#define S3P_MAX_PKT_SIZE      1018
/** @brief Max data (payload) size */
#define S3P_MAX_DATA_SIZE     1010
/** @brief Max size of an upload/download chunk */
#define S3P_MAX_CHUNK_SIZE    1004
/** @brief Frame COBS delimiter */
#define S3P_COBS_DELIM        0x00
/** @brief Macro to mask sequence from flags_seq packet field */
#define S3P_SEQ_MASKED(_s)    ((_s) & 0x0F)
/** @brief Size of a single reg value returned by a PT_READ_REGS_RESP
 * response */
#define S3P_SER_ITEM_SIZE     7
/** @brief Max size of any 'name' field in a request/response */
#define S3P_MAX_NAME_SIZE     32
/** @brief Dummy node id value */
#define S3P_ID_NONE           0
/** @brief Dummy sequence value */
#define S3P_SEQ_NONE          0

/** @brief No error */
#define S3P_ERR_NONE          0
/** @brief Error when translating a virtual VMEM address to a physical
 * addrress in a memory type */
#define S3P_ERR_VMEM_XLATE    100
/** @brief The requested register does not exist */
#define S3P_ERR_NO_REG        101
/** @brief Error locking the register table for read/write */
#define S3P_ERR_NO_LOCK       102
/** @brief Wrong value type in a register write request */
#define S3P_ERR_TYPE          103
/** @brief Packet/payload has wrong size (missing bytes/arguments) */
#define S3P_ERR_SIZE          104
/** @brief Register is read only (non-mutable) */
#define S3P_ERR_NO_WRITE      105
/** @brief The requested VMEM mapping index does not exist */
#define S3P_ERR_NO_VMEM       106
/** @brief The spcified cmd_id in a PT_EXEC_CMD request is not supported */
#define S3P_ERR_NO_CMD        107

/**
 * @brief S3P packet struct definition
*/
typedef struct {
    /// Source node id, 0 and 255 are reserved
    uint8_t src_id;
    /// Destination node id, 0 and 255 are reserved
    uint8_t dst_id;
    /// Flags(RESERVED) & sequence
    uint8_t flags_seq;
    /// Request/response type
    uint8_t type;
    /// Length of the Data section (payload)
    uint16_t data_len;
    /// Pointer to packet buffer to be encoded or decoded to
    uint8_t *buf;
    /// Pointer to the Data section of packet
    uint8_t *data;
} s3p_packet_t;

/**
 * @brief S3P ParadigmaTech custom request/reponse codes
*/
typedef enum {
    /// Dummy value for no type
    PT_NONE               = 0,
    /// Exec command request
    PT_EXEC_CMD           = 0x10,
    /// Exec command response
    PT_EXEC_CMD_RESP      = 0x11,
    /// Read reg(s) request
    PT_READ_REGS          = 0x12,
    /// Read reg(s) response
    PT_READ_REGS_RESP     = 0x13,
    /// Write reg request
    PT_WRITE_REG          = 0x14,
    /// Write reg response
    PT_WRITE_REG_RESP     = 0x15,
    /// VMEM read request
    PT_READ_VMEM          = 0x16,
    /// VMEM read response
    PT_READ_VMEM_RESP     = 0x17,
    /// VMEM write request
    PT_WRITE_VMEM         = 0x18,
    /// VMEM write response
    PT_WRITE_VMEM_RESP    = 0x19,
    /// Read string reg request
    PT_READ_STR_REG       = 0x1A,
    /// Read string reg response
    PT_READ_STR_REG_RESP  = 0x1B,
    /// Write string reg request
    PT_WRITE_STR_REG      = 0x1C,
    /// Write string reg response
    PT_WRITE_STR_REG_RESP = 0x1D,
    /// S3P version, register and VMEM table information request
    PT_S3P_INFO           = 0x30,
    /// S3P version, register and VMEM table information response
    PT_S3P_INFO_RESP      = 0x31,
    /// Reg information request
    PT_REG_INFO           = 0x32,
    /// Reg information reponse
    PT_REG_INFO_RESP      = 0x33,
    /// VMEM mapping entry request
    PT_VMEM_INFO          = 0x34,
    /// VMEM mapping entry response
    PT_VMEM_INFO_RESP     = 0x35,
} pkt_type_t;

/**
 * @brief S3P ParadigmaTech custom #PT_EXEC_CMD cmd ids
*/
typedef enum {
    /// Ping node
    CT_PING   = 0x10,
    /// Reboot node
    CT_REBOOT = 0x11,
} cmd_type_t;

/**
 * @brief Set S3P internal debug level
 * @param level Debug level, 0 (default) to disable all output
*/
extern void s3p_set_debug_level(const int level);

/**
 * @brief Parse received frame into a #s3p_packet_t structure.
 *
 * This function parses a received serial frame into a #s3p_packet_t structure.
 * Packet must have been already initialized by a call to #s3p_init_pkt
 *
 * @param level Debug level, 0 (default) to disable all output
 * @return true if parsing was successful
*/
extern bool s3p_parse_frame(s3p_packet_t *pkt, const uint8_t dst_id,
        const uint8_t *frame_buf, uint16_t len);

/**
 * @brief Initialize a #s3p_packet_t structure to be used for sending or
 * receiving a frame
 * @param pkt Pointer to parsed (decoded) packet structure
 * @param pkt_buf Pointer to store parsed data from received framee (see
 * #s3p_parse_frame) or data to be encoded into a frame for sending (see
 * #s3p_make_frame). This buffer can be allocated dynamically or
 * statically as a global array, and must be at least #S3P_MAX_PKT_SIZE
 * bytes in size.
 * @param src_id Source node id
 * @param dst_id Destination node id
 * @param flags_seq Flags (currently not used and resereved) and
 * sequence values. Use #S3P_SEQ_MASKED macro to truncate the passed
 * sequence number to the correct number of bits
*/
extern void s3p_init_pkt(s3p_packet_t *pkt, uint8_t *pkt_buf,
        const uint8_t src_id, const uint8_t dst_id,
        const uint8_t flags_seq);

/**
 * @brief Encodes a frame for serial transmission from a #s3p_packet_t
 * packet structure description
 * @param frame_buf Pointer to a buffer that will contain the frame ready to
 * be sent. Must be at least #S3P_MAX_FRAME_SIZE bytes in size.
 * @param pkt_out Pointer to packet structure to be encoded
 * @return Size of the encoded frame, 0 in case of encoding error
*/
extern uint16_t s3p_make_frame(uint8_t *frame_buf, const s3p_packet_t *pkt_out);

/**
 * @brief Helper function to decode error codes to string
 * @param code Error code
 * @return Const char pointer to a null terminated string
*/
extern const char *s3p_err_str(const uint8_t code);

#endif // _S3P_H

