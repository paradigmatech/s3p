#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <termios.h>
#include <stdlib.h>
#include <signal.h>
#include "ser.h"
#include "s3p.h"
#include "value.h"
#include "s3psh_utils.h"
#include "s3p_dbg.h"
#include "colors.h"

// TODO:
// - check for ser_write() errors
// - optimization: in rlist <regid> show info from local table if present
// - optimization: in get <regid> show reg name and type from local table

#define VER             "1.01"
#define M_MIN(_x,_y)    ( ( (_x) > (_y) ) ? (_y) : (_x) )
#define BYTE_DELAY      10000UL // 10ms
#define DEF_MANAGER_ID  0x6A
#define DEF_NODE_ID     0x2A
#define RESP_TO_MS      10000
#define PROMPT          C_GRN "\ns3psh> " C_NRM

// Flags regs
#define F_NONE          0x0000
#define F_MUTABLE       0x0001
#define F_PERSIST       0x0002
// Flags VMEM
#define VF_NONE         0x00
#define VF_READ         0x01
#define VF_WRITE        0x02
#define VF_MIRROR       0x04
// End markers
#define REGS_END        0xFFFF
#define VMEM_END        0xFFFFFFFF

#define IS_EQUAL(_cmd, _c)      (!strcmp(_cmd, _c))
#define USE_READLINE

#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#define HISTORY_FILE    ".s3psh_history"
#define HISTORY_MAX_LEN 1000
#endif

typedef enum {
    RG_NONE = 0,
    RG_DBG,
    RG_RADIO,
    RG_ADV_RADIO,
    RG_TELEM1,
    RG_TELEM2,
    RG_LIMITS,
    RG_CALIB,
    RG_NET,
} reg_group_t;

typedef enum {
    MT_NONE = 0,
    MT_SNOR,
    MT_FRAM,
    MT_MRAM,
    MT_UNOR1,
    MT_UNOR2,
} mem_type_t;

typedef struct {
    //value_t value;
    uint16_t id;
    uint16_t flags;
    uint8_t group_id;
    value_type_t vt;
    char name[S3P_MAX_NAME_SIZE];
} reg_t;

typedef struct {
    uint32_t vstart;
    uint32_t size;
    mem_type_t type;
    mem_type_t type2;
    uint8_t flags;
    char name[S3P_MAX_NAME_SIZE];
} vmem_t;

static reg_t *regs_table = NULL;
static vmem_t *vmem_table = NULL;

struct ser_struct ser = { 0 };
// Frame buffer
static uint8_t frame_buf[S3P_MAX_FRAME_SIZE];
// Decoded packet in buffer
static uint8_t pkt_in_buf[S3P_MAX_PKT_SIZE];
// Unencoded packet out buffer
static uint8_t pkt_out_buf[S3P_MAX_PKT_SIZE];
static uint8_t seq_num;
static uint8_t node_id = DEF_NODE_ID;
static uint8_t manager_id = DEF_MANAGER_ID;
static bool en_adv_cmds;

static int ctrlc = 0;

static void dump_ts(void)
{
    time_t ltime;
    ltime = time(NULL);
    struct tm *tm;
    tm = localtime(&ltime);

    DBG(0, "%04d/%02d/%02d %02d:%02d:%02d", tm->tm_year+1900, tm->tm_mon,
            tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static void show_usage(char **argv)
{
    DBG(0, "\n");
    DBG(0, "Usage: %s [-a] [-d[d]] [-i id] [-m id] <ser_dev>\n", argv[0]);
    DBG(0, "\n");
    DBG(0, "Where:\n");
    DBG(0, "  -a          enable advanced/debug commands\n");
    DBG(0, "  -d[d]       enable debug. More verbose with -dd\n");
    DBG(0, "  -i id       id/serial address of the remote node\n");
    DBG(0, "  -m id       id/serial address of the manager (this app)\n");
    DBG(0, "  <ser_dev>   serial device (e.g. /dev/ttyUSB0\n");
    DBG(0, "\n\n");
}

static void show_help(void)
{
    DBG(0, "\n");
    DBG(0, "Arguments type:\n");
    DBG(0, "  (d) decimal, (h) hex, (s) string\n");
    DBG(0, "\n");
    DBG(0, "Value types:\n");
    DBG(0, "  u8/i8, u16/i16, u32/i32, flt(float), str(string)\n");
    DBG(0, "\n");
    DBG(0, "\n");
    DBG(0, "Commands:\n");
    DBG(0, "  node [node_id(d)]                         - show or select remote node id\n");
    DBG(0, "  info                                      - get node S3P version info\n");
    DBG(0, "  ping                                      - ping remote node\n");
    DBG(0, "  reboot                                    - reboot remote node\n");
    DBG(0, "  get <1st_reg(d)>[+nregs(d)]               - read nregs starting from first_reg\n");
    DBG(0, "  get <reg(d)> [reg(d)] .. [reg(d)]         - read a space separated list of regs\n");
    DBG(0, "  set <reg(d)> <vt(s)> <value>              - write reg value with of type vt\n");
    DBG(0, "  sget <reg(d)>                             - read string register\n");
    DBG(0, "  sset <reg(d)> <string(s)>                 - write string register\n");
    DBG(0, "  rlist [refresh | <reg(d)>]                - show regs table, donwloading from remote\n");
    DBG(0, "                                              if needed, [refresh] forces download,\n");
    DBG(0, "                                              <reg(d)] get single register info\n");
    DBG(0, "  vmem [refresh]                            - show vmem, donwloading from remote\n");
    DBG(0, "                                              if needed, [refresh] forces download\n");
    DBG(0, "  down[load] <addr(h)> <size(d)> <file(s)>  - download to a file from vmem\n");
    DBG(0, "  up[load]   <addr(h)> <file(s)>            - upload a file to vmem\n");
    DBG(0, "\n");
    if (en_adv_cmds) {
        DBG(0, "Advanced Commands:\n");
        DBG(0, "  cmd <cmd_id(h)> <value(u32)>              - exec custom command <cmd_id> with u32 arg\n");
        DBG(0, "\n");
    }
    DBG(0, "Command prefixes\n");
    DBG(0, "  r<delay_ms>:cmd  - repeats cmd every delay_ms. E.g. r1000:get 500+10\n");
    DBG(0, "  R<delay_ms>:cmd  - same as 'r' but clears the screen\n");
    DBG(0, "\n");
    DBG(0, "Local commands:\n");
    DBG(0, "  ? or h(elp)  - show help\n");
    DBG(0, "  q(uit)       - quit application\n");
}

static void catch_signal(int sig)
{
    //signal(SIGTERM, catch_signal);
    //signal(SIGINT, catch_signal);

    DBG(0, "Got Signal %d\n",sig);
    ctrlc = 1;
}

const char *mem_type_str(mem_type_t type)
{
    switch (type) {
        case MT_NONE:   return "NONE";
        case MT_SNOR:   return "SNOR";
        case MT_FRAM:   return "FRAM";
        case MT_MRAM:   return "MRAM";
        case MT_UNOR1:  return "UNOR1";
        case MT_UNOR2:  return "UNOR2";
        default: break;
    }
    return "UNK";
}

static const char *group_name(reg_group_t group_id)
{
    switch (group_id) {
    case RG_NONE     : return "";
    case RG_DBG      : return "dbg";
    case RG_RADIO    : return "radio";
    case RG_ADV_RADIO: return "adv";
    case RG_TELEM1   : return "telem1";
    case RG_TELEM2   : return "telem2";
    case RG_LIMITS   : return "limits";
    case RG_CALIB    : return "cal";
    case RG_NET      : return "net";
    default: break;
    }
    return "UNK";
}

static uint8_t seq_inc(void)
{
    seq_num++;
    seq_num = S3P_SEQ_MASKED(seq_num);
    return seq_num;
}

static bool check_seq(const s3p_packet_t *pkt_in)
{
    const uint8_t seq_in = S3P_SEQ_MASKED(pkt_in->flags_seq);
    if (seq_in != seq_num) {
        DBG(1, "RESP: wrong seq, exp=0x%02X, recv=0x%02X\n", seq_num, seq_in);
        return false;
    }
    return true;
}

static bool wait_response(s3p_packet_t *pkt_in)
{
    uint32_t last_ms;
    bool timeout;
    int nbytes;
    uint8_t byt;
    uint16_t rx_len = 0;

    last_ms = client_utils_get_ms();
    while (! (timeout=client_utils_is_elapsed_ms(last_ms, RESP_TO_MS)) ) {
        nbytes = ser_read(&ser, &byt, 1);
        if (!nbytes) {
            usleep(BYTE_DELAY);
            continue;
        }
        if (rx_len < S3P_MAX_FRAME_SIZE)
            frame_buf[rx_len++] = byt;

        if (byt == S3P_COBS_DELIM) {
            s3p_init_pkt(pkt_in, pkt_in_buf, S3P_ID_NONE, S3P_ID_NONE,
                    S3P_SEQ_NONE);
            bool res = s3p_parse_frame(pkt_in, manager_id, frame_buf,
                    rx_len-1);
            return res;
        }
    }
    if (timeout)
        DBG(0, "Response timeout\n");
    return false;
}

static void exec_cmd(const uint8_t cmd_id, const uint32_t arg)
{
    s3p_packet_t pkt_out;
    s3p_packet_t pkt_in;
    int data_len = 0;
    uint16_t size;
    uint8_t code;

    s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
    // Cmd id
    pkt_out.data[data_len++] = cmd_id;
    // Arg
    pkt_out.data[data_len++] = (uint8_t)(arg >> 24);
    pkt_out.data[data_len++] = (uint8_t)(arg >> 16);
    pkt_out.data[data_len++] = (uint8_t)(arg >> 8);
    pkt_out.data[data_len++] = (uint8_t)arg;
    // Header
    pkt_out.data_len = data_len;
    pkt_out.type = PT_EXEC_CMD;
    size = s3p_make_frame(frame_buf, &pkt_out);
    if (!size)
        return;

    ser_write(&ser, frame_buf, size);
    if (!wait_response(&pkt_in))
        return;
    if (!check_seq(&pkt_in))
        return;

    code = pkt_in.data[0];
    DBG(1, "EXEC CMD RESP: data_len=%u, res_code=%u\n", pkt_in.data_len,
            code);
    if (code != S3P_ERR_NONE) {
        DBG(0, "Read error: %s (%u)\n", s3p_err_str(code), code);
        return;
    }
}

static void exec_ping(void)
{
    s3p_packet_t pkt_out;
    s3p_packet_t pkt_in;
    int data_len = 0;
    uint16_t size;
    uint8_t code;
    uint32_t start_ms;

    s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
    // Cmd id
    pkt_out.data[data_len++] = CT_PING;
    // Arg
    data_len += 4;
    // Header
    pkt_out.data_len = data_len;
    pkt_out.type = PT_EXEC_CMD;
    size = s3p_make_frame(frame_buf, &pkt_out);
    if (!size)
        return;

    ser_write(&ser, frame_buf, size);
    start_ms = client_utils_get_ms();
    if (!wait_response(&pkt_in))
        return;
    if (!check_seq(&pkt_in))
        return;

    code = pkt_in.data[0];
    DBG(1, "EXEC PING: data_len=%u, res_code=%u\n", pkt_in.data_len,
            code);
    if (code != S3P_ERR_NONE) {
        DBG(0, "Read error: %s (%u)\n", s3p_err_str(code), code);
        return;
    }
    DBG(0, "PING ok, latency: %u ms\n", client_utils_elapsed_ms(start_ms));
}

static void exec_rregs(const uint16_t reg_id, const uint16_t regs_cnt)
{
    s3p_packet_t pkt_out;
    s3p_packet_t pkt_in;
    char value_str[VALUE_SCALAR_MAX_SIZE];
    int data_len = 0;
    uint16_t cnt;
    uint16_t size;
    uint16_t id;
    uint8_t code;
    value_t value;
    const char *name;

    s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
    // Start reg id
    pkt_out.data[data_len++] = (uint8_t)(reg_id >> 8);
    pkt_out.data[data_len++] = (uint8_t)reg_id;
    // Regs count
    pkt_out.data[data_len++] = (uint8_t)(regs_cnt >> 8);
    pkt_out.data[data_len++] = (uint8_t)regs_cnt;
    // Header
    pkt_out.data_len = data_len;
    pkt_out.type = PT_READ_REGS;
    size = s3p_make_frame(frame_buf, &pkt_out);
    if (!size)
        return;

    ser_write(&ser, frame_buf, size);
    if (!wait_response(&pkt_in))
        return;
    if (!check_seq(&pkt_in))
        return;

    size = 0;
    cnt = 0;
    code = pkt_in.data[size++];
    DBG(1, "READ REGS RESP: data_len=%u, res_code=%u\n",
            pkt_in.data_len, code);
    if (code != S3P_ERR_NONE) {
        DBG(0, "Read error: %s (%u)\n", s3p_err_str(code), code);
        return;
    }
    while (size+S3P_SER_ITEM_SIZE <= pkt_in.data_len) {
        // This is safe becaus size of union value.val is > 4
        // Note that buf is not guaranteed to be 4 bytes aligned
        // ID
        id = ((uint16_t)pkt_in.data[size++]) << 8;
        id |= (uint16_t)pkt_in.data[size++];
        // Type
        value.vt = ((uint8_t)pkt_in.data[size++]);
        // Value
        value.val.u32 = 0;
        value.val.u32 |= ((uint32_t)pkt_in.data[size++]) << 24;
        value.val.u32 |= ((uint32_t)pkt_in.data[size++]) << 16;
        value.val.u32 |= ((uint32_t)pkt_in.data[size++]) << 8;
        value.val.u32 |= (uint32_t)pkt_in.data[size++];
        cnt++;
        name = "";
        if (VALUE_TYPE_IS_SCALAR(value.vt)) {
            value_dump(value_str, &value, VALUE_SCALAR_MAX_SIZE);
            DBG(0, "[%3u] %3u %-20s %3s   %s\n", cnt, id, name,
                    value_type_str(value.vt), value_str);
        }
        else {
            DBG(0, "[%3u] %3u %-20s %3s   NOT SCALAR\n", cnt, id, name,
                    value_type_str(value.vt));
        }
    }
    DBG(0, "Response ok\n");
}

static void exec_wreg(const uint16_t reg_id, const value_t *value)
{
    s3p_packet_t pkt_out;
    s3p_packet_t pkt_in;
    int data_len = 0;
    uint16_t size;
    uint8_t code;

    s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
    // Reg id
    pkt_out.data[data_len++] = (uint8_t)(reg_id >> 8);
    pkt_out.data[data_len++] = (uint8_t)reg_id;
    // Value type
    pkt_out.data[data_len++] = value->vt;
    // Value u8
    pkt_out.data[data_len++] = (uint8_t)(value->val.u32 >> 24);
    pkt_out.data[data_len++] = (uint8_t)(value->val.u32 >> 16);
    pkt_out.data[data_len++] = (uint8_t)(value->val.u32 >> 8);
    pkt_out.data[data_len++] = (uint8_t)value->val.u32;
    // Header
    pkt_out.data_len = data_len;
    pkt_out.type = PT_WRITE_REG;
    size = s3p_make_frame(frame_buf, &pkt_out);
    if (!size)
        return;

    ser_write(&ser, frame_buf, size);
    if (!wait_response(&pkt_in))
        return;
    if (!check_seq(&pkt_in))
        return;

    size = 0;
    code = pkt_in.data[size++];
    DBG(1, "WRITE REGS RESP: data_len=%u, res_code=%u\n",
            pkt_in.data_len, code);
    if (code != S3P_ERR_NONE) {
        DBG(0, "Write error: %s (%u)\n", s3p_err_str(code), code);
    }
}

static void exec_rstr(const uint16_t reg_id)
{
    s3p_packet_t pkt_out;
    s3p_packet_t pkt_in;
    int data_len = 0;
    uint16_t size;
    uint8_t code;
    uint16_t id;
    char *str;
    value_type_t vt;

    s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
    // Reg id
    pkt_out.data[data_len++] = (uint8_t)(reg_id >> 8);
    pkt_out.data[data_len++] = (uint8_t)reg_id;
    // Header
    pkt_out.data_len = data_len;
    pkt_out.type = PT_READ_STR_REG;
    size = s3p_make_frame(frame_buf, &pkt_out);
    if (!size)
        return;

    ser_write(&ser, frame_buf, size);
    if (!wait_response(&pkt_in))
        return;
    if (!check_seq(&pkt_in))
        return;

    size = 0;
    code = pkt_in.data[size++];
    DBG(1, "READ STR REG RESP: data_len=%u, res_code=%u\n",
            pkt_in.data_len, code);
    if (code != S3P_ERR_NONE) {
        DBG(0, "Read error: %s (%u)\n", s3p_err_str(code), code);
        return;
    }
    // Id
    id = ((uint16_t)pkt_in.data[size++]) << 8;
    id |= (uint16_t)pkt_in.data[size++];
    // Type
    vt = ((uint8_t)pkt_in.data[size++]);
    // String
    str = (char *)&pkt_in.data[size];
    const char *name = "";
    DBG(0, "%3u %-20s %3s   '%s'\n", id, name, value_type_str(vt), str);
}

static void exec_info(void)
{
    s3p_packet_t pkt_out;
    s3p_packet_t pkt_in;
    int data_len = 0;
    uint16_t size;
    uint8_t code;
    uint16_t ver = 0;
    uint16_t reg_min_id = 0;
    uint16_t reg_max_id = 0;
    uint16_t regs_cnt = 0;
    uint8_t vmem_rows;

    s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
    // Header
    pkt_out.data_len = data_len;
    pkt_out.type = PT_S3P_INFO;
    size = s3p_make_frame(frame_buf, &pkt_out);
    if (!size)
        return;

    ser_write(&ser, frame_buf, size);
    if (!wait_response(&pkt_in))
        return;
    if (!check_seq(&pkt_in))
        return;

    size = 0;
    code = pkt_in.data[size++];
    DBG(1, "TABLE INFO RESP: data_len=%u, res_code=%u\n",
            pkt_in.data_len, code);
    if (code != S3P_ERR_NONE) {
        DBG(0, "Table info error: %s (%u)\n", s3p_err_str(code), code);
        return;
    }
    // Version
    ver = ((uint16_t)pkt_in.data[size++]) << 8;
    ver |= (uint16_t)pkt_in.data[size++];
    // Reg min
    reg_min_id = ((uint16_t)pkt_in.data[size++]) << 8;
    reg_min_id |= (uint16_t)pkt_in.data[size++];
    // Reg max
    reg_max_id = ((uint16_t)pkt_in.data[size++]) << 8;
    reg_max_id |= (uint16_t)pkt_in.data[size++];
    // Regs cnt
    regs_cnt = ((uint16_t)pkt_in.data[size++]) << 8;
    regs_cnt |= (uint16_t)pkt_in.data[size++];
    // VMEM rows
    vmem_rows = (uint16_t)pkt_in.data[size++];
    // Info
    DBG(0, "Remote node S3P info:\n");
    DBG(0, "  S3P ver  : %2u.%02u (local: %2u.%02u)\n", ver>>8,
            (uint8_t)ver, S3P_VERSION>>8, (uint8_t)(S3P_VERSION));
    DBG(0, "  reg min  : %3u\n", reg_min_id);
    DBG(0, "  reg max  : %3u\n", reg_max_id);
    DBG(0, "  regs cnt : %3u\n", regs_cnt);
    DBG(0, "  vmem maps: %3u %s\n", vmem_rows, vmem_rows?"":"(NOT SUPPORTED)");
    DBG(0, "Response ok\n");
}

static void exec_rinfo(const uint16_t reg_id)
{
    s3p_packet_t pkt_out;
    s3p_packet_t pkt_in;
    int data_len = 0;
    uint16_t size;
    uint16_t id;
    uint16_t next_id;
    uint8_t code;
    value_type_t vt;
    uint8_t group_id;
    uint16_t flags;
    const char *name = "";

    s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
    // Reg id
    pkt_out.data[data_len++] = (uint8_t)(reg_id >> 8);
    pkt_out.data[data_len++] = (uint8_t)reg_id;
    // Header
    pkt_out.data_len = data_len;
    pkt_out.type = PT_REG_INFO;
    size = s3p_make_frame(frame_buf, &pkt_out);
    if (!size)
        return;

    ser_write(&ser, frame_buf, size);
    if (!wait_response(&pkt_in))
        return;
    if (!check_seq(&pkt_in))
        return;

    size = 0;
    code = pkt_in.data[size++];
    DBG(1, "REG INFO RESP: data_len=%u, res_code=%u\n",
            pkt_in.data_len, code);
    if (code != S3P_ERR_NONE) {
        DBG(0, "Reg info error: %s (%u)\n", s3p_err_str(code), code);
        return;
    }
    // Id
    id = ((uint16_t)pkt_in.data[size++]) << 8;
    id |= (uint16_t)pkt_in.data[size++];
    // Next id
    next_id = ((uint16_t)pkt_in.data[size++]) << 8;
    next_id |= (uint16_t)pkt_in.data[size++];
    // Type
    vt = ((uint8_t)pkt_in.data[size++]);
    // Group
    group_id = ((uint8_t)pkt_in.data[size++]);
    // Flags
    flags = ((uint16_t)pkt_in.data[size++]) << 8;
    flags |= (uint16_t)pkt_in.data[size++];
    // Info
    name = (char *)&pkt_in.data[size];
    DBG(0, " group    |  id | name                 | type | flags\n");
    DBG(0, "-----------+-----+----------------------+------+--------\n");
    DBG(0, " %-9s| %3u | %-20s | %4s | %c%c\n", group_name(group_id),
            id, name, value_type_str(vt),
            flags&F_MUTABLE?'M':' ', flags&F_PERSIST?'P':' ');
    DBG(0, "Response ok\n");
}

static void exec_rlist(void)
{
    s3p_packet_t pkt_out;
    s3p_packet_t pkt_in;
    int data_len = 0;
    uint16_t cnt = 0;
    uint16_t reg_id;
    uint16_t id;
    uint16_t next_id;
    uint8_t code;
    value_type_t vt;
    uint8_t group_id;
    uint16_t flags;
    const char *name = "";
    uint16_t ver = 0;
    uint16_t reg_min_id = 0;
    uint16_t reg_max_id = 0;
    uint16_t regs_cnt = 0;

    DBG(0, "Downloading regs table...\n");

    s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
    // Header
    pkt_out.data_len = data_len;
    pkt_out.type = PT_S3P_INFO;
    uint16_t size = s3p_make_frame(frame_buf, &pkt_out);
    if (!size)
        return;

    ser_write(&ser, frame_buf, size);
    if (!wait_response(&pkt_in))
        return;
    if (!check_seq(&pkt_in))
        return;

    size = 0;
    code = pkt_in.data[size++];
    DBG(1, "S3P INFO RESP: data_len=%u, res_code=%u\n",
            pkt_in.data_len, code);
    if (code != S3P_ERR_NONE) {
        DBG(0, "Table info error: %s (%u)\n", s3p_err_str(code), code);
        return;
    }
    // Version
    ver = ((uint16_t)pkt_in.data[size++]) << 8;
    ver |= (uint16_t)pkt_in.data[size++];
    // Reg min
    reg_min_id = ((uint16_t)pkt_in.data[size++]) << 8;
    reg_min_id |= (uint16_t)pkt_in.data[size++];
    // Reg max
    reg_max_id = ((uint16_t)pkt_in.data[size++]) << 8;
    reg_max_id |= (uint16_t)pkt_in.data[size++];
    // Regs cnt
    regs_cnt = ((uint16_t)pkt_in.data[size++]) << 8;
    regs_cnt |= (uint16_t)pkt_in.data[size++];
    // VMEM rows
    //vmem_rows = (uint16_t)pkt_in.data[size++];
    size++;

    if (!reg_min_id || !regs_cnt) {
        DBG(0, "Invalid table info\n");
        return;
    }

    // Allocate regs_table
    if (regs_table != NULL)
        free(regs_table);
    // Add one more reg for end marker
    regs_table = calloc(regs_cnt+1, sizeof(reg_t));
    if (regs_table == NULL) {
        DBG(0, "Failed to allocate regs table\n");
        return;
    }
    reg_t *reg = regs_table;

    ctrlc = 0;
    reg_id = reg_min_id;
    while (!ctrlc && cnt<regs_cnt && reg_id<=reg_max_id) {
        data_len = 0;
        s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
        // Reg id
        pkt_out.data[data_len++] = (uint8_t)(reg_id >> 8);
        pkt_out.data[data_len++] = (uint8_t)reg_id;
        // Header
        pkt_out.data_len = data_len;
        pkt_out.type = PT_REG_INFO;
        uint16_t size = s3p_make_frame(frame_buf, &pkt_out);
        if (!size)
            break;
        // Send msg
        if (ser_write(&ser, frame_buf, size) < 0) {
            DBG(1, "Ser write error!\n");
            break;
        }

        memset(&pkt_in, 0x00, sizeof(s3p_packet_t));
        //ser_discard(&ser);
        if (!wait_response(&pkt_in))
            break;
        if (!check_seq(&pkt_in))
            break;

        size = 0;
        code = pkt_in.data[size++];
        if (code != S3P_ERR_NONE) {
            DBG(0, "Table list error: %s (%u)\n", s3p_err_str(code), code);
            break;
        }
        // Id
        id = ((uint16_t)pkt_in.data[size++]) << 8;
        id |= (uint16_t)pkt_in.data[size++];
        // Next id
        next_id = ((uint16_t)pkt_in.data[size++]) << 8;
        next_id |= (uint16_t)pkt_in.data[size++];
        // Type
        vt = ((uint8_t)pkt_in.data[size++]);
        // Group
        group_id = ((uint8_t)pkt_in.data[size++]);
        // Flags
        flags = ((uint16_t)pkt_in.data[size++]) << 8;
        flags |= (uint16_t)pkt_in.data[size++];
        // Name
        name = (char *)&pkt_in.data[size];
        snprintf(reg->name, S3P_MAX_NAME_SIZE, "%s", name);
        // Add to table
        reg->id = id;
        reg->group_id = group_id;
        reg->flags = flags;
        //reg->value = value; // TODO
        reg->vt = vt;

        reg++;
        cnt++;

        // Progress
        DBG(0, "Getting %3u of %u (%3u%%)\r", cnt, regs_cnt, (cnt*100)/regs_cnt);

        if (!next_id)
            break;
        reg_id = next_id;
    }
    // Add regs table end marker
    reg->id = REGS_END;
    // Summary
    DBG(0, "\n");
    DBG(0, "Got %u reg of %u, %s\n", cnt, regs_cnt,
            cnt == regs_cnt ? "OK" : "ERROR");
}

static void exec_rshow(void)
{
    if (regs_table == NULL) {
        DBG(0, "Local table not found, forcing download...\n");
        exec_rlist();
    }

    reg_t *reg = regs_table;

    DBG(0, " group    |  id | name                 | type | flags\n");
    DBG(0, "----------+-----+----------------------+------+--------\n");
    while (reg->id != REGS_END) {
        DBG(0, " %-9s| %3u | %-20s | %4s | %c%c\n", group_name(reg->group_id),
                reg->id, reg->name, value_type_str(reg->vt),
                reg->flags&F_MUTABLE?'M':' ', reg->flags&F_PERSIST?'P':' ');
        reg++;
    }
}

static void exec_vlist(void)
{
    s3p_packet_t pkt_out;
    s3p_packet_t pkt_in;
    int data_len = 0;
    uint8_t code;
    uint8_t flags;
    const char *name = "";
    uint8_t idx, next_idx, row_idx;
    uint8_t cnt = 0;
    mem_type_t type, type2;
    uint32_t vaddr, vsize;
    uint8_t vmem_rows;

    DBG(0, "Downloading VMEM table...\n");

    s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
    // Header
    pkt_out.data_len = data_len;
    pkt_out.type = PT_S3P_INFO;
    uint16_t size = s3p_make_frame(frame_buf, &pkt_out);
    if (!size)
        return;

    ser_write(&ser, frame_buf, size);
    if (!wait_response(&pkt_in))
        return;
    if (!check_seq(&pkt_in))
        return;

    size = 0;
    code = pkt_in.data[size++];
    DBG(1, "VMEM INFO RESP: data_len=%u, res_code=%u\n",
            pkt_in.data_len, code);
    if (code != S3P_ERR_NONE) {
        DBG(0, "VMEM info error: %s (%u)\n", s3p_err_str(code), code);
        return;
    }
    // Version
    size++; // ver = ((uint16_t)pkt_in.data[size++]) << 8;
    size++; // ver |= (uint16_t)pkt_in.data[size++];
    // Reg min
    size++; // reg_min_id = ((uint16_t)pkt_in.data[size++]) << 8;
    size++; // reg_min_id |= (uint16_t)pkt_in.data[size++];
    // Reg max
    size++; // reg_max_id = ((uint16_t)pkt_in.data[size++]) << 8;
    size++; // reg_max_id |= (uint16_t)pkt_in.data[size++];
    // Regs cnt
    size++; // regs_cnt = ((uint16_t)pkt_in.data[size++]) << 8;
    size++; // regs_cnt |= (uint16_t)pkt_in.data[size++];
    // VMEM rows
    vmem_rows = pkt_in.data[size++];

    if (!vmem_rows) {
        DBG(0, "Invalid VMEM info\n");
        return;
    }

    // Allocate vmem_table
    if (vmem_table != NULL)
        free(vmem_table);
    // Add one more vmem for end marker
    vmem_table = calloc(vmem_rows+1, sizeof(vmem_t));
    if (vmem_table == NULL) {
        DBG(0, "Failed to allocate regs table\n");
        return;
    }
    vmem_t *vmem = vmem_table;

    ctrlc = 0;
    row_idx = 0;
    while (!ctrlc && cnt<vmem_rows && row_idx<vmem_rows) {
        data_len = 0;
        s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
        // Row idx
        pkt_out.data[data_len++] = row_idx;
        // Header
        pkt_out.data_len = data_len;
        pkt_out.type = PT_VMEM_INFO;
        size = s3p_make_frame(frame_buf, &pkt_out);
        if (!size)
            break;
        // Send msg
        if (ser_write(&ser, frame_buf, size) < 0) {
            DBG(1, "Ser write error!\n");
            break;
        }

        memset(&pkt_in, 0x00, sizeof(s3p_packet_t));
        //ser_discard(&ser);
        if (!wait_response(&pkt_in))
            break;
        if (!check_seq(&pkt_in))
            break;

        size = 0;
        code = pkt_in.data[size++];
        if (code != S3P_ERR_NONE) {
            DBG(0, "VMEM list error: %s (%u)\n", s3p_err_str(code), code);
            break;
        }
        // Id
        idx = pkt_in.data[size++];
        idx = idx; // Avoid warning
        // Next idx
        next_idx = pkt_in.data[size++];
        // Type
        type = ((mem_type_t)pkt_in.data[size++]);
        // vaddr
        vaddr = ((uint32_t)pkt_in.data[size++]) << 24;
        vaddr |= ((uint32_t)pkt_in.data[size++]) << 16;
        vaddr |= ((uint32_t)pkt_in.data[size++]) << 8;
        vaddr |= (uint32_t)pkt_in.data[size++];
        // vsize
        vsize = ((uint32_t)pkt_in.data[size++]) << 24;
        vsize |= ((uint32_t)pkt_in.data[size++]) << 16;
        vsize |= ((uint32_t)pkt_in.data[size++]) << 8;
        vsize |= (uint32_t)pkt_in.data[size++];
        // Flags
        flags = pkt_in.data[size++];
        // Mirror type
        type2 = ((mem_type_t)pkt_in.data[size++]);
        // Name
        name = (char *)&pkt_in.data[size];
        snprintf(vmem->name, S3P_MAX_NAME_SIZE, "%s", name);
        // Add to table
        vmem->vstart = vaddr;
        vmem->size = vsize;
        vmem->type = type;
        vmem->type2 = type2;
        vmem->flags = flags;

        vmem++;
        cnt++;

        // Progress
        DBG(0, "Getting %3u of %u (%3u%%)\r", cnt, vmem_rows, (cnt*100)/vmem_rows);

        if (!next_idx)
            break;
        row_idx = next_idx;
    }
    // Add regs table end marker
    vmem->vstart = VMEM_END;
    // Summary
    DBG(0, "\n");
    DBG(0, "Got %u vmem items of %u, %s\n", cnt, vmem_rows,
            cnt == vmem_rows ? "OK" : "ERROR");
}

static void exec_vshow(void)
{
    if (vmem_table == NULL) {
        DBG(0, "Local table not found, forcing download...\n");
        exec_vlist();
    }

    vmem_t *vmem = vmem_table;

    DBG(0, " name     |   address  |  type |   size   | flags | mirror\n");
    DBG(0, "----------+------------+-------+----------+-------+-------\n");
    while (vmem->vstart != VMEM_END) {
        DBG(0, " %-8s | 0x%08X | %-5s | %8u |  %c%c%c  | %-5s \n",
                vmem->name,
                vmem->vstart,
                mem_type_str(vmem->type),
                vmem->size,
                vmem->flags&VF_READ?'R':' ',
                vmem->flags&VF_WRITE?'W':' ',
                vmem->flags&VF_MIRROR?'M':' ',
                vmem->type2!=MT_NONE?mem_type_str(vmem->type2):"");
        vmem++;
    }
}

static void exec_wstr(const uint16_t reg_id, const char *str)
{
    s3p_packet_t pkt_out;
    s3p_packet_t pkt_in;
    int data_len = 0;
    uint16_t size;
    uint8_t code;

    //DBG(0, "exec_wstr: reg_id=%u, str='%s'\n", reg_id, str);
    s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
    // Reg id
    pkt_out.data[data_len++] = (uint8_t)(reg_id >> 8);
    pkt_out.data[data_len++] = (uint8_t)reg_id;
    // Data
    strcpy((char *)(pkt_out.data+data_len), str);
    data_len += strlen((char *)(pkt_out.data+data_len));
    data_len +=1; // Add null terminator
    //DBG(0, "exec_wstr: data_len=%u, data='%s'\n", data_len, pkt_out.data);
    // Header
    pkt_out.data_len = data_len;
    pkt_out.type = PT_WRITE_STR_REG;
    size = s3p_make_frame(frame_buf, &pkt_out);
    if (!size)
        return;

    ser_write(&ser, frame_buf, size);
    if (!wait_response(&pkt_in))
        return;
    if (!check_seq(&pkt_in))
        return;

    size = 0;
    code = pkt_in.data[size++];
    DBG(1, "WRITE STR RESP: data_len=%u, res_code=%u\n",
            pkt_in.data_len, code);
    if (code != S3P_ERR_NONE) {
        DBG(0, "Write error: %s (%u)\n", s3p_err_str(code), code);
        return;
    }
}

static void exec_down(uint32_t addr, const uint32_t tot_size, const char *file)
{
    s3p_packet_t pkt_out;
    s3p_packet_t pkt_in = { 0 };
    uint16_t size;
    size_t nbytes;
    uint32_t rsize = 0;
    uint32_t rbytes = 0;
    uint8_t vmem_buf[S3P_MAX_CHUNK_SIZE];
    uint8_t code;
    uint32_t wsize;

    DBG(0, "Download %u bytes to file '%s' from address 0x%08X\n",
            tot_size, file, addr);

    FILE *fp = fopen(file, "w");
    if (fp == NULL) {
        DBG(0, "Can't open file '%s' for writing\n", file);
        return;
    }

    ctrlc = 0;
    while (!ctrlc && rbytes<tot_size) {
        int data_len = 0;
        s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
        // Address
        pkt_out.data[data_len++] = (uint8_t)(addr >> 24);
        pkt_out.data[data_len++] = (uint8_t)(addr >> 16);
        pkt_out.data[data_len++] = (uint8_t)(addr >> 8);
        pkt_out.data[data_len++] = (uint8_t)addr;
        // Read size
        rsize = M_MIN(S3P_MAX_CHUNK_SIZE, tot_size-rbytes);
        pkt_out.data[data_len++] = (uint8_t)(rsize >> 8);
        pkt_out.data[data_len++] = (uint8_t)rsize;
        // Header
        pkt_out.data_len = data_len;
        pkt_out.type = PT_READ_VMEM;

        DBG(1, "\nCHUNK REQ: rsize=%u, rbytes=%u\n", rsize, rbytes);
        size = s3p_make_frame(frame_buf, &pkt_out);
        if (!size)
            return;

        memset(&pkt_in, 0x00, sizeof(s3p_packet_t));
        ser_write(&ser, frame_buf, size);
        //ser_discard(&ser);
        if (!wait_response(&pkt_in))
            return;
        if (!check_seq(&pkt_in))
            return;

        // Code
        code = pkt_in.data[0];
        wsize = M_MIN(pkt_in.data_len-1, rsize);
        DBG(1, "CHUNK: code=0x%02X, data_len=%u, wsize=%u\n",
                code, pkt_in.data_len, wsize);
        if (code == 0) {
            memcpy(vmem_buf, pkt_in.data+1, wsize);
            nbytes = fwrite(vmem_buf, 1, wsize, fp);
            rbytes += nbytes;
            DBG(2, "[0x%08X] +%4lu (%4u)\n", addr, nbytes, rbytes);
            addr += nbytes;
            DBG(0, "Received %6u of %6u (%3u%%)\r", rbytes, tot_size, (rbytes*100)/tot_size);
            fflush(stdout);
        }
        else {
            DBG(0, "\nError: %s (%u)\n", s3p_err_str(code), code);
            break;
        }
    }

    DBG(0, "\nDownload complete: got %u bytes of %u\n", rbytes, tot_size);
    if (rbytes == tot_size)
        DBG(0, "File download ok\n");
    else
        DBG(0, "File download error\n");

    fclose(fp);
}

static void exec_up(uint32_t addr, const char *file)
{
    s3p_packet_t pkt_out;
    s3p_packet_t pkt_in = { 0 };
    int data_len;
    uint16_t size;
    uint8_t code;
    size_t nbytes;
    uint32_t wbytes = 0;
    uint8_t vmem_buf[S3P_MAX_CHUNK_SIZE];

    DBG(0, "Upload file '%s' to address 0x%08X\n", file, addr);

    FILE *fp = fopen(file, "r");
    if (fp == NULL) {
        DBG(0, "Can't open file '%s' for reading\n", file);
        return;
    }
    fseek(fp, 0L, SEEK_END);
    uint32_t tot_size = (uint32_t)ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    ctrlc = 0;
    while (!ctrlc && (nbytes = fread(vmem_buf, 1, S3P_MAX_CHUNK_SIZE, fp)) > 0) {
        DBG(1, "\n[0x%08X] +%4lu\n", addr, nbytes);

        data_len = 0;
        s3p_init_pkt(&pkt_out, pkt_out_buf, manager_id, node_id, seq_inc());
        // Address
        pkt_out.data[data_len++] = (uint8_t)(addr >> 24);
        pkt_out.data[data_len++] = (uint8_t)(addr >> 16);
        pkt_out.data[data_len++] = (uint8_t)(addr >> 8);
        pkt_out.data[data_len++] = (uint8_t)addr;
        // Data
        memcpy(pkt_out.data+data_len, vmem_buf, nbytes);
        data_len += nbytes;
        // Header
        pkt_out.data_len = data_len;
        pkt_out.type = PT_WRITE_VMEM;

        size = s3p_make_frame(frame_buf, &pkt_out);
        if (!size)
            return;

        memset(&pkt_in, 0x00, sizeof(s3p_packet_t));
        ser_write(&ser, frame_buf, size);
        //ser_discard(&ser);
        if (!wait_response(&pkt_in))
            return;
        if (!check_seq(&pkt_in))
            return;

        // Code
        code = pkt_in.data[0];
        if (code == S3P_ERR_NONE) {
            addr += nbytes;
            wbytes += nbytes;
            DBG(0, "Sent %6u of %6u (%3u%%)\r", wbytes, tot_size, (wbytes*100)/tot_size);
            fflush(stdout);
        }
        else {
            DBG(0, "\nError: %s (%u)\n", s3p_err_str(code), code);
            break;
        }
    }

    DBG(0, "\nUpload complete: sent %u bytes of %u\n", wbytes, tot_size);
    if (feof(fp))
        DBG(0, "File upload ok\n");
    else
        DBG(0, "File upload error\n");

    fclose(fp);
}

static void manage_cmd(const char *cmd, const char *args)
{
    //DBG(0, "MANAGE: cmd='%s'  args='%s'\n", cmd, args);

    if (IS_EQUAL(cmd, "node")) {
        uint8_t id;
        int args_cnt = sscanf(args, "%hhu", &id);
        if (args_cnt == 1) {
            node_id = id;
        }
        DBG(0, "Node id=0x%02X %u\n", node_id, node_id);
    }
    else if (IS_EQUAL(cmd, "cmd")) {
        uint8_t cmd_id;
        uint32_t arg;
        int args_cnt = sscanf(args, "%hhx %u", &cmd_id, &arg);
        if (args_cnt != 2) {
            DBG(0, "Arg(s) missing or wrong\n");
            return;
        }
        exec_cmd(cmd_id, arg);
    }
    else if (IS_EQUAL(cmd, "ping")) {
        exec_ping();
    }
    else if (IS_EQUAL(cmd, "reboot")) {
        exec_cmd(CT_REBOOT, 0);
    }
    else if (IS_EQUAL(cmd, "get")) {
        uint16_t reg_id, regs_cnt;
        int args_cnt = sscanf(args, "%hu+%hu", &reg_id, &regs_cnt);
        if (args_cnt == 2) {
            exec_rregs(reg_id, regs_cnt);
            return;
        }
        else {
            char *dup = strdup(args);
            char *ptr = strtok(dup, " ");
            int cnt = 0;
            if (ptr != NULL) {
                cnt++;
                //reg_id = atoi(ptr);
                reg_id = strtol(ptr, NULL, 0);
                exec_rregs(reg_id, 1);
                while ( (ptr=strtok(NULL, " ")) != NULL) {
                    cnt++;
                    reg_id = atoi(ptr);
                    exec_rregs(reg_id, 1);
                }
                free(dup);
                return;
            }
            free(dup);
        }
        DBG(0, "Arg(s) missing or wrong\n");
    }
    else if (IS_EQUAL(cmd, "set")) {
        uint16_t reg_id;
        char value_str[16];
        char vt_str[16];
        int args_cnt = sscanf(args, "%hu %s %s", &reg_id, vt_str, value_str);
        if (args_cnt != 3) {
            DBG(0, "Arg(s) missing or wrong\n");
            return;
        }
        value_t value;
        value.vt = value_type_from_str(vt_str);
        if (value.vt == VT_EMPTY) {
            DBG(0, "Unknown value type: %s\n", vt_str);
            return;
        }
        DBG(1, "WREG: type='%s', reg_id=%u, value_str='%s'\n",
                value_type_str(value.vt), reg_id, value_str);
        if (value.vt == VT_U8)  { args_cnt = sscanf(value_str, "%hhu", &(value.val.u8)) ; }
        if (value.vt == VT_I8)  { args_cnt = sscanf(value_str, "%hhi", &(value.val.i8)) ; }
        if (value.vt == VT_X8)  { args_cnt = sscanf(value_str, "%hhx", &(value.val.u8)) ; }
        if (value.vt == VT_U16) { args_cnt = sscanf(value_str, "%hu",  &(value.val.u16)); }
        if (value.vt == VT_I16) { args_cnt = sscanf(value_str, "%hi",  &(value.val.i16)); }
        if (value.vt == VT_X16) { args_cnt = sscanf(value_str, "%hx",  &(value.val.u16)); }
        if (value.vt == VT_U32) { args_cnt = sscanf(value_str, "%u",   &(value.val.u32)); }
        if (value.vt == VT_I32) { args_cnt = sscanf(value_str, "%i",   &(value.val.i32)); }
        if (value.vt == VT_X32) { args_cnt = sscanf(value_str, "%x",   &(value.val.u32)); }
        if (value.vt == VT_FLT) { args_cnt = sscanf(value_str, "%f",   &(value.val.flt)); }

        if (args_cnt != 1) {
            DBG(0, "Arg(s) missing or wrong\n");
            return;
        }
        exec_wreg(reg_id, &value);
    }
    else if (IS_EQUAL(cmd, "sget")) {
        uint16_t reg_id;
        int args_cnt = sscanf(args, "%hu", &reg_id);
        if (args_cnt != 1) {
            DBG(0, "Arg(s) missing or wrong\n");
            return;
        }
        exec_rstr(reg_id);
    }
    else if (IS_EQUAL(cmd, "sset")) {
        uint16_t reg_id;
        char str[32];
        int args_cnt = sscanf(args, "%hu %s", &reg_id, str);
        if (args_cnt != 2) {
            DBG(0, "Arg(s) missing or wrong\n");
            return;
        }
        exec_wstr(reg_id, str);
    }
    else if (IS_EQUAL(cmd, "info")) {
        exec_info();
    }
    else if (IS_EQUAL(cmd, "rlist")) {
        char str[32];
        int args_cnt = sscanf(args, "%s", str);
        if (args_cnt == 1) {
            if (IS_EQUAL(str, "refresh")) {
                exec_rlist();
                return;
            }
        }
        uint16_t reg_id;
        args_cnt = sscanf(args, "%hu", &reg_id);
        if (args_cnt == 1) {
            exec_rinfo(reg_id);
            return;
        }
        exec_rshow();
    }
    else if (IS_EQUAL(cmd, "vmem")) {
        char str[32];
        int args_cnt = sscanf(args, "%s", str);
        if (args_cnt == 1) {
            if (IS_EQUAL(str, "refresh")) {
                exec_vlist();
                return;
            }
            else {
                DBG(0, "Arg %s not supported\n", str);
            }
        }
        exec_vshow();
    }
    else if (IS_EQUAL(cmd, "down") || IS_EQUAL(cmd, "download")) {
        uint32_t addr;
        uint32_t tot_size = 0;
        char file[256];
        int args_cnt = sscanf(args, "%x %u %s", &addr, &tot_size, file);
        if (args_cnt != 3) {
            DBG(0, "Arg(s) missing or wrong\n");
            return;
        }
        exec_down(addr, tot_size, file);
    }
    else if (IS_EQUAL(cmd, "up") || IS_EQUAL(cmd, "upload")) {
        uint32_t addr;
        char file[256];
        int args_cnt = sscanf(args, "%x %s", &addr, file);
        if (args_cnt != 2) {
            DBG(0, "Arg(s) missing or wrong\n");
            return;
        }
        exec_up(addr, file);
    }
    else if (IS_EQUAL(cmd, "h") || IS_EQUAL(cmd, "help") || IS_EQUAL(cmd, "?")) {
        show_help();
    }
    else {
        DBG(0, "Unknown command '%s'\n", cmd);
    }
}

int main(int argc, char **argv)
{
#ifndef USE_READLINE
    char cmd_line[256];
#endif
    char cmd[16];
    uint32_t repeat;
    uint32_t parg;
    int args_off;
    char prefix;
    bool clean = false;

    DBG(0, "\nS3P Client Shell\n");
    DBG(0, "==================\n\n");
    DBG(0, "Version %s - build %s %s\n\n", VER, __DATE__, __TIME__);

    // Manage options
    while (argc > 2) {
        if (argc>1 && !strcmp(argv[1], "-a")) {
            en_adv_cmds = true;
            argv = &argv[1];
            argc--;
        }
        if (argc>1 && !strcmp(argv[1], "-dd")) {
            _dbg_lvl = 2;
            argv = &argv[1];
            argc--;
        }
        if (argc>1 && !strcmp(argv[1], "-d")) {
            _dbg_lvl = 1;
            argv = &argv[1];
            argc--;
        }
        if (argc>2 && !strcmp(argv[1], "-i")) {
            node_id = (uint8_t)atoi(argv[2]);
            argv = &argv[2];
            argc--;
            argc--;
        }
        if (argc>2 && !strcmp(argv[1], "-m")) {
            manager_id = (uint8_t)atoi(argv[2]);
            argv = &argv[2];
            argc--;
            argc--;
        }
    }

    if (argc < 2) {
        show_usage(argv);
        return -1;
    }

    DBG(0, "Advanced cmds   : %s\n", en_adv_cmds?"ENABLED":"DISABLED");
    DBG(0, "Debug level     : %u\n", _dbg_lvl);
    DBG(0, "Node id (remote): 0x%02X %3u (%s)\n", node_id, node_id,
            node_id==DEF_NODE_ID?"DEFAULT":"CUSTOM");
    DBG(0, "Manager id (us) : 0x%02X %3u (%s)\n", manager_id, manager_id,
            manager_id==DEF_MANAGER_ID?"DEFAULT":"CUSTOM");

    // Set debug level
    s3p_set_debug_level(_dbg_lvl);

    // Open port
    DBG(0, "Opening serial port '%s'\n", argv[1]);
    int res = ser_open(&ser, argv[1], 230400, 'N', 8, 1);
    if (res) {
        DBG(0, "Error opening serial port, res=%d\n", res);
        return -1;
    }

    ser_discard(&ser);

    //DBG("\nInteractive console. Press CTRL-C to exit\n");
    //signal(SIGTERM, catch_signal);
    signal(SIGINT, catch_signal);

    stifle_history(HISTORY_MAX_LEN);
    read_history(HISTORY_FILE);

    show_help();

    while (1) {
#ifndef USE_READLINE
        memset(cmd_line, 0x00, sizeof(cmd_line));
        DBG(0, PROMPT);
#endif

#ifdef USE_READLINE
        char *cmd_line = readline(PROMPT);
        if (cmd_line == NULL)
            continue;
        if (strlen(cmd_line) > 0)
            add_history(cmd_line);
#else
        char *dummy = fgets(cmd_line, sizeof(cmd_line), stdin);
#endif
        if (cmd_line[0] == 'q') {
            DBG(0, "Quit\n");
            goto exit;
        }

        //int args_cnt = sscanf(cmd_line, "r%d:%s ", &parg, cmd);
        int args_cnt = sscanf(cmd_line, "%c%d:%s ", &prefix, &parg, cmd);
        if (args_cnt == 3) {
            if (prefix == 'r') {
                DBG(0, "Repeat cmd every %u ms. Ctrl-c to stop\n", repeat);
                clean = false;
                repeat = parg;
            }
            else if (prefix == 'R') {
                clean = true;
                repeat = parg;
            }
            else {
                DBG(0, "Unknown prefix '%c'\n", prefix);
                goto exit_loop;
            }
            char *sep = strchr(cmd_line, ':');
            if (sep == NULL) {
                DBG(0, "Wrong syntax\n");
                goto exit_loop;
            }
            args_off = sep - cmd_line + strlen(cmd) + 1;
        }
        else {
            repeat = 0;
            args_cnt = sscanf(cmd_line, "%s ", cmd);
            if (args_cnt < 1)
                goto exit_loop;
            args_off = strlen(cmd);
        }

        ctrlc = 0;
        while (!ctrlc) {
            ser_discard(&ser);
            if (repeat) {
                if (clean) {
                    printf("\e[1;1H\e[2J");
                    DBG(0, "Repeat cmd every %u ms. Ctrl-c to stop\n", repeat);
                }
                dump_ts();
                DBG(0, " ================\n");
            }
            manage_cmd(cmd, cmd_line+args_off);
            if (repeat)
                usleep(repeat*1000U);
            else
                break;
        }

exit_loop:
#ifdef USE_READLINE
        free(cmd_line);
#endif
    }

exit:
    write_history(HISTORY_FILE);

    return 0;
}

