#ifndef CONVERSION_H
#define CONVERSION_H

//--- STRING TO VALUE -------------------------------------------------------------------

int string_to_int(const char* str);
float string_to_float(const char* str);
double string_to_double(const char* str);
unsigned long long string_to_unsigned(const char* str, char** endptr, int base = 0);

//--- VALUE TO STRING -------------------------------------------------------------------

void bool_to_string(bool a, char* str);
void int_to_string(long long n, char* str, unsigned base = 10);
void float_to_string(double n, char* str, unsigned precision = 6);
void float_to_hex_string(float n, char* str);
void double_to_hex_string(double n, char* str);

#endif
