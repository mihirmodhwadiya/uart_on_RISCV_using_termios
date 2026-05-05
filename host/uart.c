/*
 * uart_test.c
 *
 * UART Interface Program for RISC-V ACT Framework Validation
 * -----------------------------------------------------------
 * This program opens a UART/serial port on Linux, configures it
 * using the termios API, transmits a test message, and receives
 * incoming data using select() for non-blocking timeout-based I/O.
 *
 * Usage:  ./uart_test <device>
 * Example: ./uart_test /dev/pts/3       (virtual port via socat)
 *          ./uart_test /dev/ttyUSB0     (real USB-to-Serial adapter)
 *          ./uart_test /dev/ttyS0       (real hardware UART)
 *
 * Build:  gcc -Wall -Wextra -o uart uart.c
 *
 * Author: Mihir Modhwadiya
 * Co-author: Gemini
 * Co-author: Claude
 * Purpose: RISC-V ACT Framework - M-Mode Firmware UART Validation
 */

#include <stdio.h>       /* printf, perror                          */
#include <stdlib.h>      /* exit, EXIT_FAILURE, EXIT_SUCCESS        */
#include <string.h>      /* memset, strlen                          */
#include <unistd.h>      /* open, close, read, write                */
#include <fcntl.h>       /* O_RDWR, O_NOCTTY, O_NDELAY             */
#include <termios.h>     /* termios, tcgetattr, tcsetattr, cfsetspeed */
#include <errno.h>       /* errno                                   */
#include <sys/select.h>  /* select(), fd_set, FD_SET, FD_ZERO       */
#include <sys/types.h>   /* fd_set                                  */

/* ------------------------------------------------------------------ */
/* Configuration Constants                                             */
/* ------------------------------------------------------------------ */

#define BAUD_RATE        B115200   /* 115200 bps — standard for embedded boards */
#define READ_TIMEOUT_SEC 5         /* Wait up to 5 seconds for a response       */
#define READ_BUF_SIZE    256       /* Receive buffer size in bytes               */
#define TEST_MESSAGE     "Hello RISC-V Board! UART ACT Test\r\n"

/* ------------------------------------------------------------------ */
/* Function: open_uart                                                 */
/* Opens the serial port device and returns the file descriptor.      */
/* Returns -1 on failure.                                             */
/* ------------------------------------------------------------------ */
int open_uart(const char *device)
{
    int fd;

    /*
     * O_RDWR   — open for both reading and writing
     * O_NOCTTY — do NOT make this port the controlling terminal
     *            (prevents Ctrl+C and other signals from affecting us)
     * O_NDELAY — open in non-blocking mode initially (don't wait for
     *            DCD signal, which may never come on virtual ports)
     */
    fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);

    if (fd == -1) {
        /*
         * Common reasons for failure:
         *   ENOENT  — device path does not exist
         *   EACCES  — no permission (add user to 'dialout' group)
         *   EBUSY   — port already in use by another process
         */
        fprintf(stderr, "[ERROR] Cannot open device '%s': %s\n",
                device, strerror(errno));
        return -1;
    }

    printf("[INFO]  Opened UART device: %s (fd=%d)\n", device, fd);

    /*
     * Switch the file descriptor back to BLOCKING mode for reads.
     * We use select() later for timeout control, so blocking reads
     * are fine — select() will tell us when data is ready.
     */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        fprintf(stderr, "[ERROR] fcntl F_GETFL failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        fprintf(stderr, "[ERROR] fcntl F_SETFL failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

/* ------------------------------------------------------------------ */
/* Function: configure_uart                                            */
/* Configures baud rate, data bits, parity, stop bits (8N1).         */
/* Returns 0 on success, -1 on failure.                               */
/* ------------------------------------------------------------------ */
int configure_uart(int fd)
{
    struct termios options;

    /*
     * Read current terminal attributes into 'options'.
     * We must start from the current settings and modify only
     * what we need — otherwise we may corrupt the port state.
     */
    if (tcgetattr(fd, &options) == -1) {
        fprintf(stderr, "[ERROR] tcgetattr failed: %s\n", strerror(errno));
        return -1;
    }

    /* ---- Baud Rate ---- */
    /*
     * cfsetispeed — sets input baud rate
     * cfsetospeed — sets output baud rate
     * Both must match for reliable communication.
     * B115200 = 115200 bps (defined in <termios.h>)
     */
    if (cfsetispeed(&options, BAUD_RATE) == -1 ||
        cfsetospeed(&options, BAUD_RATE) == -1) {
        fprintf(stderr, "[ERROR] cfsetspeed failed: %s\n", strerror(errno));
        return -1;
    }

    /* ---- Control Flags (c_cflag) ---- */

    /*
     * CS8 — 8 data bits per character (most common setting)
     * Clear PARENB — no parity bit
     * Clear CSTOPB — 1 stop bit (if set, would be 2 stop bits)
     * CLOCAL — ignore modem control lines (important for USB adapters)
     * CREAD  — enable the receiver
     *
     * Combined: 8N1 = 8 data bits, No parity, 1 stop bit
     */
    options.c_cflag &= ~PARENB;          /* Disable parity               */
    options.c_cflag &= ~CSTOPB;          /* 1 stop bit                   */
    options.c_cflag &= ~CSIZE;           /* Clear data size bits          */
    options.c_cflag |=  CS8;             /* 8 data bits                  */
    options.c_cflag &= ~CRTSCTS;         /* Disable hardware flow control */
    options.c_cflag |=  CLOCAL | CREAD;  /* Ignore modem lines, enable RX */

    /* ---- Local Flags (c_lflag) — RAW mode ---- */
    /*
     * Disable all "cooked" / canonical processing.
     * In raw mode, data is passed through exactly as received —
     * no line buffering, no special character interpretation.
     *
     * ICANON — canonical mode (line-by-line). We DISABLE this.
     * ECHO   — echo input characters. We DISABLE this.
     * ECHOE  — echo erase character. We DISABLE this.
     * ISIG   — generate signals on Ctrl+C etc. We DISABLE this.
     */
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    /* ---- Input Flags (c_iflag) ---- */
    /*
     * Disable software flow control (XON/XOFF) and all
     * special input processing so bytes arrive unmodified.
     */
    options.c_iflag &= ~(IXON | IXOFF | IXANY);   /* No software flow ctrl */
    options.c_iflag &= ~(ICRNL | INLCR | IGNCR);  /* No CR/LF translation  */

    /* ---- Output Flags (c_oflag) ---- */
    /*
     * Disable all output processing — send bytes exactly as-is.
     */
    options.c_oflag &= ~OPOST;   /* Raw output */

    /* ---- Read Timing (c_cc) ---- */
    /*
     * VMIN  = 0 — do not block waiting for a minimum number of bytes
     * VTIME = 0 — no inter-character timer
     * Together with select(), this gives us precise timeout control.
     */
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 0;

    /*
     * Apply the new settings immediately (TCSANOW).
     * Alternatives:
     *   TCSADRAIN — wait for all output to be transmitted first
     *   TCSAFLUSH — flush pending I/O, then apply
     */
    if (tcsetattr(fd, TCSANOW, &options) == -1) {
        fprintf(stderr, "[ERROR] tcsetattr failed: %s\n", strerror(errno));
        return -1;
    }

    /*
     * Flush any stale data in the input/output buffers before we start.
     * This ensures we don't read garbage from a previous session.
     */
    tcflush(fd, TCIOFLUSH);

    printf("[INFO]  UART configured: 115200 baud, 8N1, raw mode\n");
    return 0;
}

/* ------------------------------------------------------------------ */
/* Function: uart_send                                                 */
/* Transmits a string message over the UART port.                     */
/* Returns number of bytes sent, or -1 on error.                      */
/* ------------------------------------------------------------------ */
int uart_send(int fd, const char *message)
{
    ssize_t bytes_written;
    size_t  msg_len = strlen(message);

    printf("[TX]    Sending %zu bytes: \"%.*s\"\n",
           msg_len, (int)(msg_len - 2), message); /* strip \r\n for display */

    bytes_written = write(fd, message, msg_len);

    if (bytes_written == -1) {
        fprintf(stderr, "[ERROR] write() failed: %s\n", strerror(errno));
        return -1;
    }

    if ((size_t)bytes_written != msg_len) {
        fprintf(stderr, "[WARN]  Partial write: sent %zd of %zu bytes\n",
                bytes_written, msg_len);
    }

    /*
     * tcdrain() — blocks until all output bytes have been physically
     * transmitted. Important before switching to read mode.
     */
    if (tcdrain(fd) == -1) {
        fprintf(stderr, "[WARN]  tcdrain() failed: %s\n", strerror(errno));
    }

    printf("[INFO]  Transmitted %zd bytes successfully\n", bytes_written);
    return (int)bytes_written;
}

/* ------------------------------------------------------------------ */
/* Function: uart_receive                                              */
/* Waits for incoming data using select() with a timeout.             */
/* Prints received data to console.                                    */
/* Returns number of bytes received, 0 on timeout, -1 on error.       */
/* ------------------------------------------------------------------ */
int uart_receive(int fd)
{
    fd_set          read_fds;      /* Set of file descriptors to watch      */
    struct timeval  timeout;       /* Timeout duration for select()         */
    char            buf[READ_BUF_SIZE];
    ssize_t         bytes_read;
    int             total_received = 0;
    int             select_result;

    printf("[INFO]  Waiting for response (timeout: %d sec)...\n",
           READ_TIMEOUT_SEC);

    /*
     * Read loop — keep reading until timeout with no new data.
     * This handles cases where the response arrives in multiple chunks.
     */
    while (1) {

        /* ---- Set up select() ---- */
        /*
         * FD_ZERO — clear the file descriptor set
         * FD_SET  — add our UART fd to the watch set
         *
         * select() monitors multiple file descriptors and returns
         * when one or more become ready (data available to read).
         */
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);

        /*
         * Set timeout for this iteration.
         * NOTE: On Linux, select() MODIFIES the timeval struct,
         * so we must reset it every loop iteration.
         */
        timeout.tv_sec  = READ_TIMEOUT_SEC;
        timeout.tv_usec = 0;

        /*
         * select(nfds, readfds, writefds, exceptfds, timeout)
         *   nfds     — highest fd number + 1
         *   readfds  — watch for data to read
         *   writefds — NULL (we don't need write readiness here)
         *   exceptfds— NULL (no exception conditions)
         *   timeout  — max wait time
         *
         * Returns:
         *   > 0  — number of fds ready
         *   = 0  — timeout expired, no data
         *   < 0  — error
         */
        select_result = select(fd + 1, &read_fds, NULL, NULL, &timeout);

        if (select_result == -1) {
            /* select() itself failed */
            fprintf(stderr, "[ERROR] select() failed: %s\n", strerror(errno));
            return -1;
        }

        if (select_result == 0) {
            /* Timeout — no data arrived within READ_TIMEOUT_SEC */
            if (total_received == 0) {
                printf("[INFO]  No response received (timeout after %d sec)\n",
                       READ_TIMEOUT_SEC);
            } else {
                printf("[INFO]  No more data (total received: %d bytes)\n",
                       total_received);
            }
            break;
        }

        /* ---- Data is available — read it ---- */
        if (FD_ISSET(fd, &read_fds)) {

            memset(buf, 0, sizeof(buf));
            bytes_read = read(fd, buf, sizeof(buf) - 1);

            if (bytes_read == -1) {
                fprintf(stderr, "[ERROR] read() failed: %s\n", strerror(errno));
                return -1;
            }

            if (bytes_read == 0) {
                /* EOF — port was closed on the other end */
                printf("[INFO]  Connection closed by remote end\n");
                break;
            }

            /* Null-terminate for safe string printing */
            buf[bytes_read] = '\0';
            total_received += (int)bytes_read;

            printf("[RX]    Received %zd bytes: \"%s\"\n", bytes_read, buf);

            /*
             * Print raw hex dump — useful for debugging binary protocols
             * or checking for hidden control characters.
             */
            printf("[HEX]   ");
            for (ssize_t i = 0; i < bytes_read; i++) {
                printf("%02X ", (unsigned char)buf[i]);
            }
            printf("\n");
        }
    }

    return total_received;
}

/* ------------------------------------------------------------------ */
/* Function: main                                                      */
/* Entry point — orchestrates open, configure, send, receive, close.  */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    int  fd;
    int  result;

    /* ---- Banner ---- */
    printf("============================================\n");
    printf("  RISC-V ACT UART Validation Tool\n");
    printf("  M-Mode Firmware Communication Test\n");
    printf("============================================\n\n");

    /* ---- Argument Check ---- */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <device>\n", argv[0]);
        fprintf(stderr, "  Example: %s /dev/pts/3\n", argv[0]);
        fprintf(stderr, "  Example: %s /dev/ttyUSB0\n", argv[0]);
        fprintf(stderr, "  Example: %s /dev/ttyS0\n\n", argv[0]);
        fprintf(stderr, "  For testing without hardware, use socat:\n");
        fprintf(stderr, "  socat -d -d pty,raw,echo=0 pty,raw,echo=0\n");
        exit(EXIT_FAILURE);
    }

    /* ---- Step 1: Open UART ---- */
    fd = open_uart(argv[1]);
    if (fd == -1) {
        fprintf(stderr, "\n[HINT] If permission denied: sudo usermod -aG dialout $USER\n");
        fprintf(stderr, "[HINT] Then log out and log back in.\n");
        exit(EXIT_FAILURE);
    }

    /* ---- Step 2: Configure UART ---- */
    result = configure_uart(fd);
    if (result == -1) {
        close(fd);
        exit(EXIT_FAILURE);
    }

    /* ---- Step 3: Transmit Test Message ---- */
    printf("\n--- TRANSMIT ---\n");
    result = uart_send(fd, TEST_MESSAGE);
    if (result == -1) {
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Add this to uart_test.c
    const char *test_cmd = "HELLO_RISCV";
    write(fd, test_cmd, strlen(test_cmd));
    printf("Host Sent: %s\n", test_cmd);
    
    /* ---- Step 5: Summary ---- */
    printf("\n============================================\n");
    if (result > 0) {
        printf("  TEST RESULT: PASS — received %d bytes\n", result);
    } else {
        printf("  TEST RESULT: NO RESPONSE (check board/loopback)\n");
    }
    printf("============================================\n");

    /* ---- Step 6: Close UART ---- */
    close(fd);
    printf("[INFO]  UART device closed. Done.\n");

    return EXIT_SUCCESS;
}
