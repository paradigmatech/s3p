#include <string.h>
#include "crc16.h"
#include "cobs.h"
#include "s3p.h"
#include "s3p_dbg.h"

static uint8_t pkt_in_buf[S3P_MAX_PKT_SIZE];
//static uint8_t pkt_out_buf[S3P_MAX_PKT_SIZE];

void s3p_set_debug_level(const int level)
{
    _dbg_lvl = level;
}

bool s3p_parse_frame(s3p_packet_t *pkt, const uint8_t dst_id,
        const uint8_t *frame_buf, uint16_t len)
{
    DBG(1, "New msg rx: len=%u\n", len);

    cobs_decode_result res = cobs_decode(pkt_in_buf, S3P_MAX_PKT_SIZE,
            frame_buf, len);
    if (res.status != COBS_DECODE_OK) {
        DBG(1, "Decode error, res=0x%02X\n", res.status);
        return false;
    }

    DBG(1, "Decode ok: in_len=%u, out_len=%lu\n", len, res.out_len);

    pkt->src_id = pkt_in_buf[0];
    pkt->dst_id = pkt_in_buf[1];
    pkt->flags_seq = pkt_in_buf[2];
    pkt->type = pkt_in_buf[3];
    pkt->data_len = (pkt_in_buf[4]<<8) | pkt_in_buf[5];
    pkt->data = &pkt_in_buf[6];

    const uint16_t exp = (pkt_in_buf[res.out_len-2]<<8) | pkt_in_buf[res.out_len-1];
    const uint16_t calc = crc16_ccitt(pkt_in_buf, res.out_len-2, CRC_START_CCITT_1D0F);
    DBG(2, "Msg in: src=0x%02X, dst=0x%02X, flags_seq=0x%02X, type=0x%02X\n",
            pkt->src_id, pkt->dst_id, pkt->flags_seq, pkt->type);
    DBG(2, "        data_len=%u, crc=0x%04X\n",
            pkt->data_len, exp);

    // Check CRC
    if (exp != calc) {
        DBG(1, " CRC err: exp=0x%04X  calc=0x%04X\n", exp, calc);
        return false;
    }

    //Check dst_id
    if (pkt->dst_id != dst_id) {
        DBG(1, "Discarding pkt, dst_id=0x%02X != 0x%02X\n",
                pkt->src_id, dst_id);
        return false;
    }

    return true;
}

void s3p_init_pkt_out(s3p_packet_t *pkt_out, uint8_t *pkt_buf,
        const uint8_t src_id, const uint8_t dst_id,
        const uint8_t flags_seq)
{
    pkt_out->buf = pkt_buf;
    memset(pkt_buf, 0x00, S3P_MAX_PKT_SIZE);
    pkt_out->src_id = src_id;
    pkt_out->dst_id = dst_id;
    pkt_out->flags_seq = flags_seq;
    pkt_out->type = PT_NONE;
    pkt_out->data_len = 0;
    pkt_out->data = &pkt_out->buf[6];
}

uint16_t s3p_make_frame(uint8_t *buf, const s3p_packet_t *pkt_out)
{
    uint16_t pkt_size = 0;

    // Set source and destination
    pkt_out->buf[pkt_size++] = pkt_out->src_id;    // Source
    pkt_out->buf[pkt_size++] = pkt_out->dst_id;    // Dst is request src
    pkt_out->buf[pkt_size++] = pkt_out->flags_seq; // Flags/seq
    // Type
    pkt_out->buf[pkt_size++] = pkt_out->type;
    // Set data_len
    pkt_out->buf[pkt_size++] = (uint8_t)(pkt_out->data_len >> 8);
    pkt_out->buf[pkt_size++] = (uint8_t)pkt_out->data_len;
    pkt_size += pkt_out->data_len;
    // CRC
    const uint16_t crc = crc16_ccitt(pkt_out->buf, pkt_size, CRC_START_CCITT_1D0F);
    pkt_out->buf[pkt_size++] = (uint8_t)(crc >> 8);
    pkt_out->buf[pkt_size++] = (uint8_t)crc;

    DBG(2, "Msg out: src=0x%02X, dst=0x%02X, flags_seq=0x%02X, type=0x%02X\n",
            pkt_out->src_id, pkt_out->dst_id, pkt_out->flags_seq, pkt_out->type);
    DBG(2, "         data_len=%u, crc=0x%04X\n",
            pkt_out->data_len, crc);

    cobs_encode_result res = cobs_encode(buf, S3P_MAX_FRAME_SIZE,
            pkt_out->buf, pkt_size);

    if (res.status == COBS_ENCODE_OK) {
        DBG(2, "Encode ok: in_len=%u, out_len=%lu\n",
                pkt_size, res.out_len);
        // Add terminator
        buf[res.out_len] = 0x00;
        return res.out_len + 1;
    }

    DBG(1, "Encode error, res=0x%02X\n", res.status);

    return 0;
}

const char *s3p_err_str(const uint8_t code)
{
    switch (code) {
        case S3P_ERR_NONE      : return "NONE";
        case S3P_ERR_VMEM_XLATE: return "VMEM_XLATE";
        case S3P_ERR_NO_REG    : return "NO_REG";
        case S3P_ERR_NO_LOCK   : return "NO_LOCK";
        case S3P_ERR_TYPE      : return "TYPE";
        case S3P_ERR_SIZE      : return "SIZE";
        case S3P_ERR_NO_WRITE  : return "NO_WRITE";
        case S3P_ERR_NO_VMEM   : return "NO_VMEM";
        case S3P_ERR_NO_CMD    : return "NO_CMD";
        default: break;
    }
    return "UNKNOWN";
}

