#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include "ser.h"

#define termios asmtermios
#define termio asmtermio
#define winsize asmwinsize
#include <asm/termios.h>
#undef  termios
#undef termio
#undef winsize
#include <termios.h>

#define SER_DBG(...)     printf(__VA_ARGS__)
//#include "s3p_dbg.h"
//#define SER_DBG(...)     DBG(0, __VA_ARGS__)

int ser_open(struct ser_struct * const ser,
        const char * const device, const int baud, const char parity,
        const uint8_t data_bit, const uint8_t stop_bit)
{
    struct termios tios;
    speed_t speed;
    int cust_br = 0;

    // TODO
    // check parameters for errors
    // ...

    //strlcpy(ser->device, device, MAX_DEVICE_SIZE);
    strncpy(ser->device, device, MAX_DEVICE_SIZE);
    ser->baud = baud;
    ser->data_bit = data_bit;
    ser->stop_bit = stop_bit;
    ser->parity = parity;
    ser->busy = 0;

    SER_DBG("Opening %s at %d bauds (%c, %d, %d)\n",
            ser->device, ser->baud, ser->parity,
            ser->data_bit, ser->stop_bit);

    ser->fd = open(ser->device, O_RDWR | O_NOCTTY | O_NDELAY | O_EXCL);
    if (ser->fd == -1) {
        SER_DBG("ERROR Can't open the device %s (%s)\n",
                ser->device, strerror(errno));
        return -1;
    }

    // Save
    tcgetattr(ser->fd, &(ser->old_tios));

    memset(&tios, 0, sizeof(struct termios));

    switch (ser->baud) {
        case 110: speed = B110; break;
        case 300: speed = B300; break;
        case 600: speed = B600; break;
        case 1200: speed = B1200; break;
        case 2400: speed = B2400; break;
        case 4800: speed = B4800; break;
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        case 460800: speed = B460800; break;
        default:
#if 0
            speed = B9600;
            SER_DBG("WARNING Unknown baud rate %d for %s (B9600 used)\n",
                    ser->baud, ser->device);
#else
            SER_DBG("WARNING Custom baud rate %d for %s\n", ser->baud, ser->device);
            cust_br = 1;
#endif
    }

    // Set serial parameters
    if (cust_br) {
        struct termios2 tio;
        ioctl(ser->fd, TCGETS2, &tio);
        tio.c_cflag &= ~CBAUD;
        tio.c_cflag |= BOTHER;
        tio.c_ispeed = ser->baud;
        tio.c_ospeed = ser->baud;
        ioctl(ser->fd, TCSETS2, &tio);
    }
    else {
        if ((cfsetispeed(&tios, speed) < 0) ||
                (cfsetospeed(&tios, speed) < 0)) {
            close(ser->fd);
            ser->fd = -1;
            return -1;
        }
    }


    tios.c_cflag |= (CREAD | CLOCAL);
    tios.c_cflag &= ~CSIZE;
    switch (ser->data_bit) {
        case 5: tios.c_cflag |= CS5; break;
        case 6: tios.c_cflag |= CS6; break;
        case 7: tios.c_cflag |= CS7; break;
        case 8: default: tios.c_cflag |= CS8; break;
    }
    if (ser->stop_bit == 1)
        tios.c_cflag &=~ CSTOPB;
    else /* 2 */
        tios.c_cflag |= CSTOPB;

    if (ser->parity == 'N')
        tios.c_cflag &=~ PARENB;
    else if (ser->parity == 'E') {
        tios.c_cflag |= PARENB;
        tios.c_cflag &=~ PARODD;
    }
    else {
        tios.c_cflag |= PARENB;
        tios.c_cflag |= PARODD;
    }
    tios.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);
    if (ser->parity == 'N')
        tios.c_iflag &= ~INPCK;
    else
        tios.c_iflag |= INPCK;
    tios.c_iflag &= ~(IXON | IXOFF | IXANY);
    tios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK );
    tios.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL);
    tios.c_oflag &= ~(OPOST | ONLCR);
    // VMIN = 0, VTIME = 0: No blocking, return immediately with what is
    // available
    // VMIN > 0, VTIME = 0: This will make read() always wait for bytes
    // (exactly how many is determined by VMIN), so read() could block
    // indefinitely.
    // VMIN = 0, VTIME > 0: This is a blocking read of any number of
    // chars with a maximum timeout (given by VTIME). read() will block
    // until either any amount of data is available, or the timeout
    // occurs. This happens to be my favourite mode (and the one I use
    // the most).
    // VMIN > 0, VTIME > 0: Block until either VMIN characters have been
    // received, or VTIME after first character has elapsed. Note that
    // the timeout for VTIME does not begin until the first character is
    // received.
    // VMIN and VTIME are both defined as the type cc_t, which I have
    // always seen be an alias for unsigned char (1 byte). This puts an
    // upper limit on the number of VMIN characters to be 255 and the
    // maximum timeout of 25.5 seconds (255 deciseconds).
    tios.c_cc[VMIN] = 0;
    tios.c_cc[VTIME] = 0;

    if (tcsetattr(ser->fd, TCSANOW, &tios) < 0) {
        close(ser->fd);
        ser->fd = -1;
        return -1;
    }

    // Flush any stale character
    tcflush(ser->fd, TCIOFLUSH);

    ser->busy = 1;
    return 0;
}

int ser_write(struct ser_struct * const ser, const uint8_t *buf,
        int size)
{
    return write(ser->fd, buf, size);
}

int ser_read(struct ser_struct * const ser, uint8_t *buf, int size)
{
    return read(ser->fd, buf, size);
}

int ser_discard(struct ser_struct * const ser)
{
    return tcflush(ser->fd, TCIOFLUSH);
}

void ser_close(struct ser_struct * const ser)
{
    // Flush
    ser_discard(ser);
    // Restore previous settings
    tcsetattr(ser->fd, TCSANOW, &(ser->old_tios));
    close(ser->fd);
    ser->busy = 0;
}

// TODO: rivedere codici di errore e di ritorno
static int ser_select(struct ser_struct * const ser, fd_set *rfds,
        struct timeval *tv, int length_to_read)
{
    int s_rc;
    while ((s_rc = select(ser->fd+1, rfds, NULL, NULL, tv)) == -1) {
        if (errno == EINTR) {
            SER_DBG("A non blocked signal was caught\n");
            // Necessary after an error
            FD_ZERO(rfds);
            FD_SET(ser->fd, rfds);
        }
        else
            return -1;
    }

    if (s_rc == 0) {
        errno = ETIMEDOUT;
        return -1;
    }

    return s_rc;
}

int ser_wait_msg(struct ser_struct * const ser, uint8_t *buf,
        int size, const int32_t timeout_ms)
{
    int rc;
    fd_set rfds;
    struct timeval tv;
    struct timeval *p_tv;
    int rx_len = 0;

    // Add a file descriptor to the set
    FD_ZERO(&rfds);
    FD_SET(ser->fd, &rfds);

    if (timeout_ms) {
        tv.tv_sec = timeout_ms/1000;
        tv.tv_usec = (timeout_ms%1000)*1000;
        p_tv = &tv;
    }
    else
        p_tv = NULL;

    while (size != 0) {
        rc = ser_select(ser, &rfds, p_tv, size);
        if (rc == -1) {
            //SER_DBG("Error select\n");
            return -1;
        }

        rc = ser_read(ser, buf, size);
        if (rc == 0) {
            errno = ECONNRESET;
            rc = -1;
        }

        if (rc == -1) {
            //SER_DBG("Error read\n");
            return -1;
        }

        // Display the hex code of each character received
        //{
        //    int i;
        //    for (i=0; i < rc; i++)
        //        SER_DBG("<%02X>\n", buf[rx_len + i]);
        //}

        rx_len += rc;
        buf += rc;
        size -= rc;

        if (size == 0) {
            // TODO
            // Messaggio ricevuto
            // ...
        }
    }

    return rx_len;
}

int ser_is_busy(const struct ser_struct * const ser)
{
    return ser->busy;
}

#if 0
#define SLEEP_DELAY         50000UL // 50ms
int main(void)
{
    struct ser_struct ser = { 0 };
    uint8_t rx_buf[1024];
    int nbytes;

    int res = ser_open(&ser, "/dev/ttyUSB0", 115200, 'N', 8, 1);
    if (res) {
        SER_DBG("Error opening serial port, res=%d\n", res);
        return -1;
    }

    SER_DBG("Listening...\n");
    while (1) {
        while ( (nbytes=ser_read(&ser, rx_buf, sizeof(rx_buf)-1)) > 0) {
            rx_buf[nbytes] = 0;
            //printf("RX (%d)[%s]\r\n", nbytes, buf);
            printf("%s", rx_buf);
            //fwrite(buf, 1, 1, fp);
        }

        usleep(SLEEP_DELAY);
    }

    return 0;
}
#endif


