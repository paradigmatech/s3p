#include <stdbool.h>
#include <termios.h>

#define MAX_DEVICE_SIZE     256

struct ser_struct {
    char device[MAX_DEVICE_SIZE];
    int baud;
    uint8_t data_bit;
    char parity;    // 'N' 'O' 'E'
    uint8_t stop_bit;
    int fd;         // File descriptor
    struct termios old_tios;
    bool busy;
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int ser_open(struct ser_struct * const ser,
        const char * const device, const int baud,
        const char parity, const uint8_t data_bit, const uint8_t stop_bit);
extern int ser_is_busy(const struct ser_struct * const ser);
extern int ser_read(struct ser_struct * const ser, uint8_t *buf,
        int size);
extern int ser_write(struct ser_struct * const ser,
        const uint8_t *buf, int size);
extern int ser_discard(struct ser_struct * const ser);
extern void ser_close(struct ser_struct * const ser);
extern int ser_flush(struct ser_struct * const ser);
extern int ser_wait_msg(struct ser_struct * const ser,
        uint8_t *buf, int size, const int timeout_ms);

#ifdef __cplusplus
}
#endif /* __cplusplus */

