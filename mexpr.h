#ifndef mexpr_h
#define mexpr_h
/* Normalize M80 source literals and rest-of-expression NUL to shared syntax. */
const char *mexpr_text(char [256], unsigned);
/* Caller supplies radix 2..16; formatting uses the low 16 bits. */
void mexpr_number(char [32], unsigned long, unsigned);
#endif
