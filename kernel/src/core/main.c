#include <stddef.h>
#include <stdint.h>

#include "minemu/boot.h"
#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minemu/trap.h"
#include "minemu/trace.h"

#define INPUT_SIZE 21

static volatile char input_buffer[INPUT_SIZE];
static volatile size_t input_length = 0;
static volatile int line_ready = 0;

static void uart_putc(char c) {
    MINEMU_UART0->tx_data = (uint8_t)c;
}

static void uart_puts(const char *string) {
    while (*string != '\0') {
        uart_putc(*string);
        ++string;
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
    size_t start = 0;

    while (line[start] == ' ') {
        ++start;
    }

    if (line[start] == '\0') {
        return;
    }

    if (starts_with_echo(&line[start])) {
        size_t index = start + 4;

        while (line[index] == ' ') {
            ++index;
        }

        uart_puts(&line[index]);
        uart_putc('\n');
        return;
    }

    uart_puts("command not found: ");
    uart_puts(&line[start]);
    uart_putc('\n');
}

struct minemu_trap_frame *minemu_irq_dispatch(struct minemu_trap_frame *frame) {
    uint32_t source = (uint32_t)frame->exception_id;

    if (source == MINEMU_IRQ_UART0) {
        while ((MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) != 0) {
            uint8_t c = (uint8_t)MINEMU_UART0->rx_data;

            if (!line_ready) {
                if (c == '\n' || c == '\r') {
                    input_buffer[input_length] = '\0';
                    line_ready = 1;
                } else if (c == 0x08 || c == 0x7f) {
                    if (input_length > 0) {
                        --input_length;
                    }
                } else if (input_length < INPUT_SIZE - 1) {
                    input_buffer[input_length] = (char)c;
                    ++input_length;
                }
            }
        }
    }

    MINEMU_INTERRUPT->eoi = source;
    return frame;
}

void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    if ((uintptr_t)boot_info != MINEMU_BOOT_INFO_VADDR ||
        boot_info->magic != MINEMU_BOOT_INFO_MAGIC ||
        boot_info->version != MINEMU_ABI_VERSION ||
        boot_info->size != sizeof(*boot_info) ||
        boot_info->system_rom_base != UINT32_C(0x08000000) ||
        boot_info->direct_map_vaddr != UINT32_C(0xc0000000) ||
        boot_info->direct_map_paddr != UINT32_C(0x40000000) ||
        boot_info->direct_map_size != UINT32_C(0x04000000)) {
        minemu_trace_event(UINT32_C(0xb007bad0));
        minemu_fail_stop();
    }

    MINEMU_INTERRUPT->enable = UINT32_C(1) << MINEMU_IRQ_UART0;
    MINEMU_UART0->control = MINEMU_UART_CONTROL_RX_IRQ_ENABLE;

    minemu_irq_enable();

    uart_puts("hello world\n");
    uart_puts("msh> ");

    for (;;) {
        if (line_ready) {
            minemu_irq_disable();

            char line[INPUT_SIZE];
            size_t length = input_length;

            for (size_t index = 0; index <= length; ++index) {
                line[index] = input_buffer[index];
            }

            input_length = 0;
            line_ready = 0;

            minemu_irq_enable();

            process_command(line);
            uart_puts("msh> ");
        }
    }
}