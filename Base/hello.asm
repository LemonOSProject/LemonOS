_start:
    mov rax, 4 ; Write
    
    mov rbx, 0 ; Stdout
    mov rcx, string ; Data
    mov rdx, string_end - string ; Length
    int 0x69 ; Write
    
    mov rax, 1 ; Exit
    
    mov rbx, 0
    int 0x69 ; Exit
string:
 db "Hello, Lemon OS World!"
string_end:
