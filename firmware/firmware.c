#define UART_BASE 0x10000000UL
#define UART_THR  (*(volatile unsigned char *)(UART_BASE + 0))
#define UART_LSR  (*(volatile unsigned char *)(UART_BASE + 5))
#define UART_LSR_EMPTY 0x20

// Basic UART Putchar
void uart_putc(char c) {
    while (!(UART_LSR & UART_LSR_EMPTY));
    UART_THR = c;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

// Minimal ACT-style test: Validate Integer Addition
int test_add() {
    int a = 5, b = 10;
    return (a + b == 15);
}

// Read Machine ISA Register (misa) using Inline Assembly
unsigned long read_misa() {
    unsigned long val;
    __asm__ volatile ("csrr %0, misa" : "=r"(val));
    return val;
}

void echo_mode() {
    uart_puts("\n[FIRMWARE] Entering Echo Mode. Send a character from Host...\n");
    while (1) {
        // Wait for LSR bit 0 (Data Ready)
        if (UART_LSR & 0x01) { 
            char c = UART_THR; // Read the character
            uart_puts("Received: ");
            uart_putc(c);      // Send it back
            uart_puts("\n");
        }
    }
}
void main() {
    uart_puts("\n--- RISC-V M-Mode Firmware (QEMU) ---\n");
    
    // Check ISA
    unsigned long misa = read_misa();
    if ((misa >> 8) & 1) { // Check 'I' extension
        uart_puts("PASS: RV Base Integer ISA detected.\n");
    }

    // Run ACT test
    if (test_add()) {
        uart_puts("PASS: Integer Addition Test (ACT-style).\n");
    } else {
        uart_puts("FAIL: Architecture Mismatch.\n");
    }

    uart_puts("Firmware Execution Complete. Halting.\n");
    while(1);
}
