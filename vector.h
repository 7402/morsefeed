//
//  vector.h
//  morsefeed
//
// Copyright (C) 2018 Michael Budiansky. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this list of conditions
// and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice, this list of conditions
// and the following disclaimer in the documentation and/or other materials provided with the
// distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
// WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef vector_h
#define vector_h

#include <stdbool.h>
#include <stddef.h>

struct Vector {
    size_t size;
    size_t capacity;
    size_t element_size;
    void *p;
};
typedef struct Vector Vector;
typedef struct Vector StringVector;
typedef struct Vector StringArray;
typedef struct Vector CString;

Vector vector_create(size_t capacity, size_t element_size);
bool vector_push(Vector *vector, const void *element);
bool vector_at(Vector *vector, size_t index, void *element);
bool vector_replace_at(Vector *vector, size_t index, const void *new_element, void *previous_element);
bool vector_delete_at(Vector *vector, size_t index, void *previous_element);
void vector_each(Vector *vector, void (fun)(void *element));
void vector_free(Vector *vector);

void free_pointer(void *p);

StringVector string_vector_create(size_t capacity);
bool string_vector_push(StringVector *vector, const char *element);
const char *string_vector_at(StringVector *vector, size_t index);
void string_vector_each(StringVector *vector, void (fun)(const char *element));
void string_vector_free(StringVector *vector);

const char *empty_str_if_null(const char *str);

void free_string_vector(void *p);

void print_string(const char *p);

StringArray read_string_array(const char *path);
void write_string_array(const char *path, const StringArray *array);
void string_array_free(StringArray *array);

CString cstring_create(size_t capacity);
const char *cstring_p(CString *cstring);
bool cstring_append(CString *cstring, const char *str);
bool cstring_append_char(CString *cstring, const char c);
void cstring_clear(CString *cstring);
void cstring_free(CString *cstring);

#if DEBUG
bool print_if_fail(bool result, const char *message);
void vector_tests(void);
#endif

#endif /* vector_h */
