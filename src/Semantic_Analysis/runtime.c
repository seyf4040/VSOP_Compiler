#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Print functions
void print_string(const char* s) {
    printf("%s", s);
}

void print_int32(int value) {
    printf("%d", value);
}

void print_bool(int value) {
    printf("%s", value ? "true" : "false");
}

int input_int32() {
    int value;
    scanf("%d", &value);
    return value;
}

char* input_string() {
    size_t buffer_size = 128;
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer) return NULL;
    
    size_t i = 0;
    int c;
    
    // Skips leading whitespace
    while ((c = getchar()) != EOF && (c == ' ' || c == '\t' || c == '\n'));
    
    // Reads characters until newline or EOF
    if (c != EOF) {
        buffer[i++] = c;
        
        while ((c = getchar()) != EOF && c != '\n') {
            if (i >= buffer_size - 1) {
                // Resize buffer
                buffer_size *= 2;
                char* new_buffer = (char*)realloc(buffer, buffer_size);
                if (!new_buffer) {
                    free(buffer);
                    return NULL;
                }
                buffer = new_buffer;
            }
            
            buffer[i++] = c;
        }
    }
    
    // Null-terminates the string
    buffer[i] = '\0';
    
    return buffer;
}