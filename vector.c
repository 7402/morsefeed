//
//  vector.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vector.h"

Vector vector_create(size_t capacity, size_t element_size)
{
    Vector vector = { 0, capacity, element_size, NULL };

    if (capacity > 0) {
        vector.p = calloc(capacity, element_size);
        if (vector.p == NULL) vector.capacity = 0;
    }

    return vector;
}

bool vector_push(Vector *vector, const void *element)
{
    bool success = true;

    if (vector->p == NULL) {
        vector->p = malloc(vector->element_size);
        if (vector->p == NULL) {
            success = false;

        } else {
            vector->capacity = 1;
        }

    } else if (vector->size + 1 > vector->capacity) {
        void *old_p = vector->p;
        size_t new_capacity = vector->capacity == 0 ? 1 : 2 * vector->capacity;

        vector->p = realloc(vector->p, new_capacity * vector->element_size);
        if (vector->p == NULL) {
            vector->p = old_p;
            success = false;

        } else {
            vector->capacity = new_capacity;
        }
    }

    if (success) {
        memcpy(vector->p + vector->size * vector->element_size, element, vector->element_size);
        vector->size++;
    }

    return success;
}

bool vector_at(Vector *vector, size_t index, void *element)
{
    bool success = true;

    if (index < vector->size) {
        memcpy(element, vector->p + index * vector->element_size, vector->element_size);

    } else {
        memset(element, 0, vector->element_size);
        success = false;
    }

    return success;
}

bool vector_replace_at(Vector *vector, size_t index, const void *new_element, void *previous_element)
{
    bool success = true;

    if (index < vector->size) {
        if (previous_element != NULL) {
            memcpy(previous_element, vector->p + index * vector->element_size, vector->element_size);
        }
        memcpy(vector->p + index * vector->element_size, new_element, vector->element_size);


    } else {
        if (previous_element != NULL) {
            memset(previous_element, 0, vector->element_size);
        }
        success = false;
    }

    return success;
}

bool vector_delete_at(Vector *vector, size_t index, void *previous_element)
{
    bool success = true;

    if (vector->size == 0 || index >= vector->size) {
        if (previous_element != NULL) {
            memset(previous_element, 0, vector->element_size);
        }
        success = false;

    } else if (index == vector->size - 1) {
        if (previous_element != NULL) {
            memcpy(previous_element, vector->p + index * vector->element_size, vector->element_size);
        }

        vector->size--;

    } else {
        if (previous_element != NULL) {
            memcpy(previous_element, vector->p + index * vector->element_size, vector->element_size);
        }

        memmove(vector->p + index * vector->element_size,
                vector->p + (index + 1) * vector->element_size,
                (vector->size - index - 1) * vector->element_size);

        vector->size--;
    }

    return success;
}

void vector_each(Vector *vector, void (fun)(void *element))
{
    size_t k;
    void *next = vector->p;
    for (k = 0; k < vector->size; k++) {
        fun(next);
        next += vector->element_size;
    }
}

void vector_free(Vector *vector)
{
    if (vector->p != NULL) {
        free(vector->p);
        vector->p = NULL;
    }

    vector->size = 0;
    vector->capacity = 0;
}

void free_pointer(void *p)
{
    free(*(void **)p);
}

StringVector string_vector_create(size_t capacity)
{
    return vector_create(capacity, sizeof(char *));
}

bool string_vector_push(StringVector *vector, const char *element)
{
    bool success = true;
    char *element_copy = NULL;

    if (element != NULL) {
        element_copy = malloc(strlen(element) + 1);
        if (element_copy == NULL) {
            success = false;

        } else {
            strcpy(element_copy, element);
        }
    }

    if (success) {
        success = vector_push(vector, &element_copy);
    }

    return success;
}

const char *string_vector_at(StringVector *vector, size_t index)
{
    const char *elementp = NULL;

    vector_at(vector, index, &elementp);

    return elementp;
}

void string_vector_each(StringVector *vector, void (fun)(const char *element))
{
    size_t k;
    void *next = vector->p;
    for (k = 0; k < vector->size; k++) {
        fun(*(const char **)next);
        next += vector->element_size;
    }
}

void string_vector_free(StringVector *vector)
{
    vector_each(vector, free_pointer);
    vector_free(vector);
}

void free_string_vector(void *p)
{
    string_vector_free((StringVector *)p);
}

const char *empty_str_if_null(const char *str)
{
    return str == NULL ? "" : str;
}

void print_string(const char *p) {
    printf("%s\n", p);
}

#define LINE_BUFFER_SIZE 1024
#define CELL_SIZE 512

StringArray read_string_array(const char *path)
{
    StringArray array = { 0, 0, sizeof(StringVector), NULL };
    bool newline = true;;
    FILE *file = fopen(path, "r");
    char line[LINE_BUFFER_SIZE];
    char cell[CELL_SIZE];
    StringVector *row = NULL;
    bool mem_error = false;
    bool cell_truncated = false;

    memset(cell, 0, CELL_SIZE);

    if (file != NULL) {
        while (fgets(line, LINE_BUFFER_SIZE, file) != NULL) {
            size_t k;
            size_t cell_len = strlen(cell);

            if (newline) {
                StringVector next_vector = string_vector_create(2);
                if (!vector_push(&array, &next_vector)) {
                    row = NULL;
                    mem_error = true;

                } else {
                    row = (StringVector *)(array.p + (array.size - 1) * sizeof(StringVector));
                }
            }

            newline = line[strlen(line) - 1] == '\n';

            for (k = 0; k < strlen(line); k++) {
                char c = line[k];
                bool escape = c == '\\';

                if (escape) {
                    c = line[++k];
                    switch (c) {
                        case 'n':   c = '\n'; break;
                        case 't':   c = '\t'; break;
                        case 'r':   c = '\r'; break;
                        case '\\':  c = '\\'; break;
                        case '\0':  c = '\\'; break;
                    }
                }

                if ((c == '\t' && !escape) || (c == '\n' && !escape) ||
                    (k >= strlen(line) - 1 && feof(file))) {
                    if (row != NULL) {
                        if (!string_vector_push(row, cell)) {
                            mem_error = true;
                        }
                    }

                    memset(cell, 0, CELL_SIZE);
                    cell_len = 0;

                } else if (cell_len < CELL_SIZE - 1) {
                    cell[cell_len++] = c;

                } else {
                    cell_truncated = true;
                }
            }
        }
    }

    if (mem_error) {
        fprintf(stderr, "Memory error while reading %s\n", path);
    }

    if (cell_truncated) {
        fprintf(stderr, "Cell string of size > %d truncated while reading %s\n", CELL_SIZE - 1, path);
    }

    return array;
}

void write_string_array(const char *path, const StringArray *array)
{
    size_t row;
    size_t col;
    size_t k;
    FILE *file = fopen(path, "w");

    if (file == NULL) {
        fprintf(stderr, "Unable to open %s for writing.\n", path);

    } else {
        for (row = 0; row < array->size; row++) {
            StringVector vector;
            vector_at((Vector *)array, row, &vector);

            for (col = 0; col < vector.size; col++) {
                const char *str = string_vector_at(&vector, col);
                for (k = 0; k < strlen(str); k++) {
                    switch (str[k]) {
                        case '\t':  fprintf(file, "\\t");   break;
                        case '\n':  fprintf(file, "\\n");   break;
                        case '\r':  fprintf(file, "\\r");   break;
                        case '\\':  fprintf(file, "\\\\");  break;
                        default:
                            fprintf(file, "%c", str[k]);
                            break;
                    }
                }

                if (col < vector.size - 1) fprintf(file, "\t");
            }

            fprintf(file, "\n");
        }

        fclose(file);
        file = NULL;
    }
}

void string_array_free(StringArray *array)
{
    vector_each(array, free_string_vector);
    vector_free(array);
}

CString cstring_create(size_t capacity)
{
    CString cstring = vector_create(capacity == 0 ? 1 : capacity, sizeof(char));
    if (cstring.capacity > 0) {
        char c = '\0';
        vector_push(&cstring, &c);
    }

    return cstring;
}

const char *cstring_p(CString *cstring)
{
    if (cstring->size == 0) {
        return NULL;

    } else {
        return cstring->p;
    }
}

bool cstring_append(CString *cstring, const char *str)
{
    bool success = true;

    if (str != NULL) {
        size_t len = strlen(str);

        if (cstring->p == NULL) {
            cstring->p = malloc(len + 1);
            if (cstring->p == NULL) {
                success = false;

            } else {
                cstring->capacity = len + 1;
            }

        } else if (cstring->size == 0) {
            // should never get here, unless CString created incorrectly or bug
            success = false;

        } else if (cstring->size + len > cstring->capacity) {
            void *old_p = cstring->p;
            size_t new_capacity = cstring->capacity == 0 ? 1 : 2 * cstring->capacity;

            while (cstring->size + len > new_capacity) new_capacity *= 2;

            cstring->p = realloc(cstring->p, new_capacity);
            if (cstring->p == NULL) {
                cstring->p = old_p;
                success = false;

            } else {
                cstring->capacity = new_capacity;
            }
        }

        if (success) {
            strcat(cstring->p, str);
            cstring->size += len;
        }
    }

    return success;
}

bool cstring_append_char(CString *cstring, const char c)
{
    char cc[2] = { c, '\0' };
    return cstring_append(cstring, cc);
}

void cstring_clear(CString *cstring)
{
    if (cstring->size >= 1) {
        cstring->size = 1;
        memset(cstring->p, 0, 1);
    }
}

void cstring_free(CString *cstring)
{
    vector_free(cstring);
}

#if DEBUG

bool cmp_ints(Vector *v, int *array, size_t count);
bool cmp_ints(Vector *v, int *array, size_t count)
{
    bool match = true;

    for (int k = 0; k < count && match; k++) {
        int next;

        if (vector_at(v, k, &next)) {
            match = array[k] == next;

        } else {
            match = false;
        }
    }

    return match;
}

bool print_if_fail(bool result, const char *message)
{
    if (!result) printf("%s\n", message);

    return result;
}

void print_int(void *p);
void print_int(void *p)
{
    int *ip = (int *)p;
    printf("%d ", *ip);
}

void vector_tests(void)
{
    bool ok = true;
    printf("vector_tests()\n");

    // vector_create
    Vector vector = vector_create(0, sizeof(int));
    int i132[] = { 1, 3, 2 };

    // vector_push
    vector_push(&vector, &i132[0]);
    vector_push(&vector, &i132[1]);
    vector_push(&vector, &i132[2]);

    ok &= print_if_fail(cmp_ints(&vector, i132, 3), "FAIL: vector_push");

    int i465[] = { 4, 6, 5 };
    Vector copy = vector_create(2, sizeof(int));
    int i = 1;

    // vector_replace_at
    vector_replace_at(&vector, 0, &i465[0], NULL);
    vector_push(&copy, &i);

    vector_replace_at(&vector, 1, &i465[1], &i);
    vector_push(&copy, &i);

    vector_replace_at(&vector, 2, &i465[2], &i);
    vector_push(&copy, &i);

    ok &= print_if_fail(cmp_ints(&vector, i465, 3), "FAIL: vector_replace_at (1)");
    ok &= print_if_fail(cmp_ints(&copy, i132, 3), "FAIL: vector_replace_at (2)");

    // vector_each
    printf("{ 4 6 5 } = { ");
    vector_each(&vector, print_int);
    printf("}\n");

    // vector_free
    vector_free(&copy);
    // copy is still usable, just empty

    i = 4;
    vector_push(&copy, &i);

    // vector_delete_at
    vector_delete_at(&vector, 1, &i);
    vector_push(&copy, &i);

    int i45[] = { 4, 5 };
    ok &= print_if_fail(cmp_ints(&vector, i45, 2), "FAIL: vector_delete_at (1)");

    vector_delete_at(&vector, 0, NULL);

    int i5 = 5;
    ok &= print_if_fail(cmp_ints(&vector, &i5, 1), "FAIL: vector_delete_at (2)");

    vector_delete_at(&vector, 0, &i);
    vector_push(&copy, &i);

    ok &= print_if_fail(vector.size == 0, "FAIL: vector_delete_at (3)");
    ok &= print_if_fail(cmp_ints(&copy, i465, 3), "FAIL: vector_delete_at (4)");

    vector_free(&vector);
    vector_free(&copy);

    // vector init
    vector = vector_create(8, 2);
    ok &= print_if_fail(vector.size == 0, "FAIL: vector init (1)");
    ok &= print_if_fail(vector.capacity == 8, "FAIL: vector init (2)");
    ok &= print_if_fail(vector.element_size == 2, "FAIL: vector init (3)");
    ok &= print_if_fail(vector.p != NULL, "FAIL: vector init (4)");
    vector_free(&vector);
    ok &= print_if_fail(vector.size == 0, "FAIL: vector init (5)");
    ok &= print_if_fail(vector.capacity == 0, "FAIL: vector init (6)");
    ok &= print_if_fail(vector.p == NULL, "FAIL: vector init (7)");

    // string_vector_create
    StringVector sv = string_vector_create(0);

    // string_vector_push
    string_vector_push(&sv, "alpha");
    string_vector_push(&sv, "bravo");
    string_vector_push(&sv, "charlie");

    // string_vector_at
    ok &= print_if_fail(strcmp("alpha", string_vector_at(&sv, 0)) == 0, "FAIL: string_vector_at (1)");
    ok &= print_if_fail(strcmp("bravo", string_vector_at(&sv, 1)) == 0, "FAIL: string_vector_at (2)");
    ok &= print_if_fail(strcmp("charlie", string_vector_at(&sv, 2)) == 0, "FAIL: string_vector_at (3)");

    // string_vector_each
    string_vector_each(&sv, print_string);

    // string_vector_free
    string_vector_free(&sv);

    // empty_str_if_null
    ok &= print_if_fail(strcmp("", empty_str_if_null(NULL)) == 0, "FAIL: empty_str_if_null (1)");
    ok &= print_if_fail(strcmp("foo", empty_str_if_null("foo")) == 0, "FAIL: empty_str_if_null (2)");

    StringArray array = vector_create(0, sizeof(StringVector));

    sv = string_vector_create(0);
    string_vector_push(&sv, "word");
    string_vector_push(&sv, "tab");
    string_vector_push(&sv, "backslash");
    vector_push(&array, &sv);

    sv = string_vector_create(0);
    string_vector_push(&sv, "up");
    string_vector_push(&sv, "[\t]");
    string_vector_push(&sv, "[\\]");
    vector_push(&array, &sv);

    // write_string_array
    write_string_array("array.tmp", &array);

    // string_array_free
    string_array_free(&array);

    FILE *foo = fopen("array.tmp", "r");
    char txt[512];
    size_t fs = fread(txt, 1, 511, foo);
    fclose(foo);
    txt[fs] = '\0';
    printf("array.tmp:\n%s", txt);

    // read_string_array
    StringArray array2 = read_string_array("array.tmp");
    remove("array.tmp");

    vector_at(&array2, 0, &sv);
    ok &= print_if_fail(strcmp("word", string_vector_at(&sv, 0)) == 0, "FAIL: read_string_array (1)");
    ok &= print_if_fail(strcmp("tab", string_vector_at(&sv, 1)) == 0, "FAIL: read_string_array (2)");
    ok &= print_if_fail(strcmp("backslash", string_vector_at(&sv, 2)) == 0, "FAIL: read_string_array (3)");

    vector_at(&array2, 1, &sv);
    ok &= print_if_fail(strcmp("up", string_vector_at(&sv, 0)) == 0, "FAIL: read_string_array (4)");
    ok &= print_if_fail(strcmp("[\t]", string_vector_at(&sv, 1)) == 0, "FAIL: read_string_array (5)");
    ok &= print_if_fail(strcmp("[\\]", string_vector_at(&sv, 2)) == 0, "FAIL: read_string_array (6)");

    string_array_free(&array2);

    // cstring
    const char *result = "cstring-test";
    CString cstring = cstring_create(0);
    ok &= print_if_fail(cstring.capacity > 0, "FAIL: cstring (1)");
    ok &= print_if_fail(strlen(cstring_p(&cstring)) == 0, "FAIL: cstring (2)");
    cstring_append(&cstring, "cstring");
    cstring_append_char(&cstring, '-');
    cstring_append(&cstring, "test");
    ok &= print_if_fail(strcmp(cstring_p(&cstring), result) == 0, "FAIL: cstring (3)");
    ok &= print_if_fail(cstring.size == strlen(result) + 1, "FAIL: cstring (4)");

    cstring_clear(&cstring);
    ok &= print_if_fail(strlen(cstring_p(&cstring)) == 0, "FAIL: cstring (5)");

    cstring_free(&cstring);

    printf(ok ? "Others OK\n\n" : "Other FAILURE\n\n");
}
#endif

