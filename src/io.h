#ifndef IO_KERNEL_H
#define IO_KERNEL_H

#include "lib.h"

// Read a byte from I/O port
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Write a byte to I/O port
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Read a word from I/O port
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Write a word to I/O port
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

// Read 'count' words (2 bytes each) from 'port' into 'addr'
static inline void insw(uint16_t port, void *addr, uint32_t count) {
    __asm__ volatile (
        "cld\n\t"            // clear direction flag
        "rep insw"
        : "+D"(addr), "+c"(count)
        : "d"(port)
        : "memory"
    );
}

// Write 'count' words (2 bytes each) from 'addr' to 'port'
static inline void outsw(uint16_t port, const void *addr, uint32_t count) {
    __asm__ volatile (
        "cld\n\t"
        "rep outsw"
        : "+S"(addr), "+c"(count)
        : "d"(port)
        : "memory"
    );
}

#endif
