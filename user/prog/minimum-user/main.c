#include <stdint.h>
#include "minemu/user_abi.h"

#define INPUT_SIZE 26

static char input_buffer[INPUT_SIZE];
static uint32_t input_length = 0;
static int input_overflow = 0;

static void uart_putc(char c) {
    while (ioctl(MINEMU_FD_UART_OUT,
                 MINEMU_IOCTL_UART_WRITE,
                 &c, 1U) != 1) {
    }
}

static void uart_puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s);
        s++;
    }
}

static int starts_with_echo(const char *line) {
    return line[0] == 'e' &&
           line[1] == 'c' &&
           line[2] == 'h' &&
           line[3] == 'o' &&
           (line[4] == ' ' || line[4] == '\0');
}

static void process_command(char *line) {
    uint32_t start = 0;

    // Skip leading spaces.
    while (line[start] == ' ') {
        start++;
    }

    // Empty command.
    if (line[start] == '\0') {
        return;
    }

    if (starts_with_echo(&line[start])) {
        uint32_t i = start + 4;
        int pending_space = 0;
        int printed_anything = 0;

        // Skip spaces after "echo".
        while (line[i] == ' ') {
            i++;
        }

        while (line[i] != '\0') {
            if (line[i] == ' ') {
                pending_space = 1;
            } else {
                if (pending_space && printed_anything) {
                    uart_putc(' ');
                }

                uart_putc(line[i]);
                printed_anything = 1;
                pending_space = 0;
            }

            i++;
        }

        uart_putc('\n');
        return;
    }

    uart_puts("command not found: ");

    uint32_t i = start;

    while (line[i] != '\0' && line[i] != ' ') {
        uart_putc(line[i]);
        i++;
    }

    uart_putc('\n');
}

void minemu_user_main(void) {
    uart_puts("msh> ");

    for (;;) {
        int32_t status = ioctl(
            MINEMU_FD_UART_IN,
            MINEMU_IOCTL_UART_STATUS
        );

        if ((status & MINEMU_IOCTL_UART_RX_READY) == 0) {
            continue;
        }

        char c = '\0';

        if (ioctl(MINEMU_FD_UART_IN,
                  MINEMU_IOCTL_UART_READ,
                  &c, 1U) != 1) {
            continue;
        }

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            if (!input_overflow) {
                input_buffer[input_length] = '\0';
                process_command(input_buffer);
            }

            input_length = 0;
            input_overflow = 0;

            uart_puts("msh> ");
        } else if (c == '\b' || c == 127) {
            if (!input_overflow && input_length > 0) {
                input_length--;
            }
        } else if (!input_overflow) {
            if (input_length < INPUT_SIZE - 1) {
                input_buffer[input_length++] = c;
            } else {
                input_overflow = 1;
            }
        }
    }
}