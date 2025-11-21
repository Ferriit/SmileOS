org 0x7C00
bits 16


%define ENDL 0x0d, 0x0a

start:
    jmp main


; prints a string to the screen
; Params:
; ds:si points to string

puts:
    ; save registers we will modify
    push si
    push ax

.loop:
    lodsb           ; loads next character in al
    or al, al       ; verifyt if next character is null or not
    jz .done
    
    mov ah, 0x0e    ; call bios interrupt
    mov bh, 0       ; go to page idx 0
    int 0x10        ; trigger interrupt

    jmp .loop       ; loop back

.done:
    pop ax
    pop si
    ret

main:
    ; setup data segments
    mov ax, 0
    mov ds, ax
    mov es, ax

    ; setup stack
    mov ss, ax
    mov sp, 0x7C00

    mov si, msg
    call puts

    hlt

.halt:
    jmp .halt

msg: db 'Hello World!', ENDL, 0

times 510-($-$$) db 0
dw 0AA55h
