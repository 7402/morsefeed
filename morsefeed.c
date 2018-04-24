//
//  morsefeed.c
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

#ifndef _POSIX_C_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include <curl/curl.h>

#include "morsefeed.h"

#define FIRST_BUFFER_SIZE 65536

MorseFeedError process_and_send(MorseFeedParams mfp)
{
    MorseFeedError error = MF_NO_ERROR;
    BufferStruct text_buffer = { NULL, 0, 0 };

    FILE *pipe_to_mbeep = NULL;
    FILE *pipe_from_mbeep = NULL;
    pid_t pid = -1;
    bool use_key_control = false;
    int word_number = 0;
    char line[LINE_SIZE];
    char token[LINE_SIZE];
    size_t token_length = 0;
    size_t buffer_index = 0;
    size_t line_offset = 0;
    size_t token_offset = 0;
    bool more_buffers;
    size_t link_index;
    StringVector linked_urls = string_vector_create(0);
    StringVector linked_titles = string_vector_create(0);
    bool filter_html = false;
    bool excluding_tag = false;
    char entity[ENTITY_SIZE];
    char tag[TAG_SIZE];

    if (mfp.fork_mbeep) init_fork_mbeep(use_key_control);

    if (mfp.url != NULL) {
        error = url_to_buffer(mfp.url, &text_buffer);
        filter_html = true;
    }

    if (error == MF_NO_ERROR) {
        if (mfp.in_file == NULL && mfp.url == NULL) mfp.in_file = stdin;
        use_key_control = mfp.fork_mbeep && mfp.in_file != stdin;

        if (mfp.fork_mbeep) {
            if (mfp.words_per_row == DEFAULT) mfp.words_per_row = 1;
            begin_fork_mbeep(&pipe_to_mbeep, &pipe_from_mbeep, &pid, mfp.freq, mfp.paris_wpm, mfp.codex_wpm,
                             mfp.farnsworth_wpm, mfp.print_fcc_wpm, use_key_control);

        } else {
            if (mfp.out_file == NULL) mfp.out_file = stdout;
            if (mfp.words_per_row == DEFAULT) mfp.words_per_row = 5;
        }
    }

    if (error == MF_NO_ERROR &&
        (mfp.save_and_use_position || mfp.text_after != NULL || mfp.text_before != NULL) &&
        text_buffer.p == NULL && mfp.in_file != NULL) {
        long file_size = 0;

        if (fseek(mfp.in_file, 0L, SEEK_END) < 0) {
            error = MF_FILE_READ_ERROR;

        } else {
            file_size = ftell(mfp.in_file);
            rewind(mfp.in_file);
        }

        if (error == MF_NO_ERROR && file_size > 0) {
            text_buffer.p = malloc(file_size + 1);
            error = text_buffer.p == NULL ? MF_OUT_OF_MEMORY : MF_NO_ERROR;
        }

        if (text_buffer.p != NULL) {
            text_buffer.p[file_size] = '\0';
            text_buffer.used = file_size + 1;
            error = fread(text_buffer.p, 1, file_size, mfp.in_file) != file_size ?
            MF_FILE_READ_ERROR : MF_NO_ERROR;
        }
    }

    buffer_index = 0;

    if (error == MF_NO_ERROR && mfp.text_after != NULL) {
        size_t position = find_string(mfp.text_after, text_buffer.p, text_buffer.used - 1, 0);
        if (position < text_buffer.used - 1) {
            position += strlen(mfp.text_after);
            buffer_index = position;
            line_offset = position;
            token_offset = position;
        }
    }

    if (error == MF_NO_ERROR && mfp.save_and_use_position) {
        size_t position = 0;
        error = read_saved_position(mfp.state_path, mfp.url != NULL ? mfp.url : mfp.in_file_name, &position);
        if (position > buffer_index) {
            buffer_index = position;
            line_offset = position;
            token_offset = position;
        }
    }

    if (error == MF_NO_ERROR && mfp.text_before != NULL) {
        size_t position = find_string(mfp.text_before, text_buffer.p, text_buffer.used - 1, buffer_index);
        if (position < text_buffer.used - 1) {
            text_buffer.used = position + 1;
        }
    }

    if (error == MF_NO_ERROR && mfp.follow_links && text_buffer.p != NULL) {
        extract_urls(mfp.url, text_buffer.p, buffer_index, text_buffer.used - 1,
                     &linked_urls, &linked_titles);
#if DEBUG
//            string_vector_each(&linked_urls, print_string);
//            string_vector_each(&linked_titles, print_string);
#endif
    }

    more_buffers = true;
    link_index = 0;

    while (error == MF_NO_ERROR && more_buffers) {
        if (text_buffer.p == NULL) {
            // reading from file; only one pass
            more_buffers = false;

        } else if (linked_urls.size == 0) {
            // only one text buffer; only one pass
            more_buffers = false;

        } else {
            // read next linked URL
            const char *next_url = string_vector_at(&linked_urls, link_index);
            const char *next_title = string_vector_at(&linked_titles, link_index);

            if (mfp.fork_mbeep) fprintf(stderr, "%ld) %s\n", (long)link_index, next_title);

            token_length = 0;
            buffer_index = 0;
            line_offset = 0;
            token_offset = 0;
            excluding_tag = false;
            entity[0] = '\0';
            tag[0] = '\0';

            error = url_to_buffer(next_url, &text_buffer);

            if (error == MF_NO_ERROR && mfp.linked_text_after != NULL) {
                size_t position = find_string(mfp.linked_text_after, text_buffer.p, text_buffer.used - 1, 0);
                if (position < text_buffer.used - 1) {
                    position += strlen(mfp.linked_text_after);
                    buffer_index = position;
                    line_offset = position;
                    token_offset = position;
                }
            }

            if (error == MF_NO_ERROR && mfp.linked_text_before != NULL) {
                size_t position = find_string(mfp.linked_text_before, text_buffer.p, text_buffer.used - 1, buffer_index);
                if (position < text_buffer.used - 1) {
                    text_buffer.used = position + 1;
                }
            }

            if (++link_index >= linked_urls.size) {
                // this is the last one
                more_buffers = false;

            } else if (link_index > 1) {
                write_token("=", mfp.out_file, pipe_to_mbeep, pipe_from_mbeep,
                            mfp.words_per_row, &word_number, mfp.word_count,
                            use_key_control, filter_html, &excluding_tag, entity, tag);
            }
        }

        while (error == MF_NO_ERROR && NULL !=
               fbgets(line, LINE_SIZE, mfp.in_file, text_buffer.p, text_buffer.used - 1, &buffer_index)) {
#ifdef DEBUG
            fprintf(stderr, "fgets(%s)\n", line);
#endif
            size_t k;
            for (k = 0; k < strlen(line) && error == MF_NO_ERROR; k++) {
                char c = line[k];
                if (isspace(c)) {
                    if (token_length > 0) {
                        token[token_length] = '\0';
                        error = write_token(token, mfp.out_file, pipe_to_mbeep, pipe_from_mbeep,
                                            mfp.words_per_row, &word_number, mfp.word_count,
                                            use_key_control, filter_html, &excluding_tag, entity, tag);

                        if (error == MF_NO_ERROR) {
                            token_offset = line_offset + k + 1;
#ifdef DEBUG
                            fprintf(stderr, "token_offset = %d\n", (int)token_offset);
#endif
                        }

                        token_length = 0;
                    }

                } else if (token_length == LINE_SIZE - 1) {
                    token[token_length] = '\0';
                    error = write_token(token, mfp.out_file, pipe_to_mbeep, pipe_from_mbeep,
                                        mfp.words_per_row, &word_number, mfp.word_count,
                                        use_key_control, filter_html, &excluding_tag, entity, tag);

                    if (error == MF_NO_ERROR) {
                        token_offset = line_offset + token_length;
#ifdef DEBUG
                        fprintf(stderr, "token_offset = %d\n", (int)token_offset);
#endif
                    }
                    token[0] = c;
                    token_length = 1;

                } else {
                    token[token_length++] = c;
                }
            }

            line_offset += strlen(line);
        }

        // see if last token to finish
        if (token_length > 0) {
            token[token_length] = '\0';
            error = write_token(token, mfp.out_file, pipe_to_mbeep, pipe_from_mbeep,
                                mfp.words_per_row, &word_number, mfp.word_count,
                                use_key_control, filter_html, &excluding_tag, entity, tag);
            token_length = 0;
        }

        if (error == MF_NEXT) error = MF_NO_ERROR;
    }

    string_vector_free(&linked_urls);
    string_vector_free(&linked_titles);

    if (error == MF_NO_ERROR && text_buffer.p == NULL && !feof(mfp.in_file)) {
        error = MF_FILE_READ_ERROR;
    }

    if (error == MF_NO_ERROR && token_length > 0) {
        token[token_length] = '\0';
        error = write_token(token, mfp.out_file, pipe_to_mbeep, pipe_from_mbeep,
                            mfp.words_per_row, &word_number, mfp.word_count, use_key_control,
                            filter_html, &excluding_tag, entity, tag);

        if (error == MF_NO_ERROR) {
            token_offset = line_offset;
#ifdef DEBUG
            fprintf(stderr, "final token_offset = %d\n", (int)token_offset);
#endif
        }
    }

    if ((error == MF_NO_ERROR || error == MF_EXIT) && pipe_to_mbeep == NULL &&
            fprintf(mfp.out_file, "\n") < 0) {
        error = MF_FILE_WRITE_ERROR;
    }

    if (mfp.fork_mbeep) end_fork_mbeep(pipe_to_mbeep, pipe_from_mbeep, pid);

    if ((error == MF_NO_ERROR || error == MF_EXIT) && mfp.save_and_use_position) {
        if (token_offset >= text_buffer.used - 1) token_offset = 0;

        error = write_saved_position(mfp.state_path, mfp.url != NULL ? mfp.url : mfp.in_file_name, token_offset);
    }

    free_buffer(&text_buffer);

    return error;
}

// see https://ec.haxx.se/libcurlexamples.html
size_t curl_write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    BufferStruct *text_bufferp = (BufferStruct *)userp;
    size_t bytes_to_copy = size * nmemb;
    char *copy_to;

    size_t new_used = text_bufferp->used + bytes_to_copy;
    if (text_bufferp->used == 0) {
        // also need room for terminating nul
        new_used++;
    }

    if (text_bufferp->p == NULL) {
        init_buffer(text_bufferp, new_used > FIRST_BUFFER_SIZE ? new_used : FIRST_BUFFER_SIZE);

    } else if (new_used > text_bufferp->capacity) {
        size_t new_capacity = 2 * text_bufferp->capacity;
        while (new_capacity < new_used) new_capacity *= 2;

        char *new_buffer = realloc(text_bufferp->p, new_capacity);
        if (new_buffer != NULL) {
            text_bufferp->p = new_buffer;
            text_bufferp->capacity = new_capacity;
        }
    }

    if (text_bufferp->used == 0) {
        copy_to = text_bufferp->p;

    } else {
        copy_to = text_bufferp->p + text_bufferp->used - 1;
    }

    if (new_used <= text_bufferp->capacity) {
        if (bytes_to_copy > 0) strncpy(copy_to, buffer, bytes_to_copy);
        text_bufferp->used = new_used;
        text_bufferp->p[new_used - 1] = '\0';

    } else {
        bytes_to_copy = 0;
    }

    return bytes_to_copy;
}

MorseFeedError url_to_buffer(const char *url, BufferStruct *buffer)
{
    MorseFeedError error = MF_NO_ERROR;
    CURLcode curl_code = CURLE_OK;
    CURL *handle = NULL;
    long response_code = 0;

    buffer->used = 0;

    if (url != NULL) {
        curl_global_init(CURL_GLOBAL_ALL);
        handle = curl_easy_init();
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_write_data);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)buffer);
        curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 5);

        curl_code = curl_easy_perform(handle);
        if (curl_code != CURLE_OK) error = MF_URL_READ_ERROR;

        if (error == MF_NO_ERROR) {
            curl_code = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
            if (curl_code != CURLE_OK) error = MF_URL_READ_ERROR;
        }

        curl_easy_cleanup(handle);
        curl_global_cleanup();
    }

    if (curl_code != CURLE_OK) {
        const char *error_str = NULL;
        switch (curl_code) {
            case CURLE_UNSUPPORTED_PROTOCOL:    error_str = "CURLE_UNSUPPORTED_PROTOCOL";   break;
            case CURLE_URL_MALFORMAT:           error_str = "CURLE_URL_MALFORMAT";          break;
            case CURLE_COULDNT_RESOLVE_PROXY:   error_str = "CURLE_COULDNT_RESOLVE_PROXY";  break;
            case CURLE_COULDNT_RESOLVE_HOST:    error_str = "CURLE_COULDNT_RESOLVE_HOST";   break;
            case CURLE_COULDNT_CONNECT:         error_str = "CURLE_COULDNT_CONNECT";        break;
            case CURLE_WRITE_ERROR:             error_str = "CURLE_WRITE_ERROR";            break;
            default:
                break;
        }

        if (error_str == NULL) {
            fprintf(stderr, "response code %ld CURL error %d URL: %s\n",
                    response_code, curl_code, url);

        } else {
            fprintf(stderr, "response code %ld CURL error %s URL: %s\n",
                    response_code, error_str, url);
        }
    }

    if (buffer->used == 0) {
        error = MF_URL_READ_ERROR;
    }

    return error;
}

void init_buffer(BufferStruct *buffer, size_t capacity)
{
    buffer->p = capacity == 0 ? NULL : malloc(capacity);
    buffer->capacity = buffer->p == NULL ? 0 : capacity;
    buffer->used = 0;
}

void free_buffer(BufferStruct *buffer)
{
    if (buffer->p != NULL) {
        free(buffer->p);
        buffer->p = NULL;
    }

    buffer->used = 0;
    buffer->capacity = 0;
}

char *fbgets(char *line, int line_size, FILE *file, char *buffer, size_t buffer_size, size_t *next_index)
{
    char *result = NULL;
    if (buffer == NULL) {
        result = fgets(line, line_size, file);

    } else if (*next_index == buffer_size) {
        result = NULL;
        
    } else {
        size_t line_index = 0;
        while (line_index < line_size - 1 && *next_index < buffer_size &&
               (line_index == 0 || line[line_index - 1] != '\n')) {
            line[line_index++] = buffer[(*next_index)++];
        }
        
        line[line_index] = '\0';
        result = line;
    }
    
    return result;
}

MorseFeedError read_saved_position(const char *state_path, const char *label, size_t *position)
{
    MorseFeedError error = MF_NO_ERROR;
    StringArray array = read_string_array(state_path);
    size_t k;
    bool found = false;
    
    *position = 0;
    for (k = 0; k < array.size && !found; k++) {
        StringVector next;
        if (vector_at(&array, k, &next) && next.size == 3 &&
            strcmp(string_vector_at(&next, 0), "position") == 0 &&
            strcmp(string_vector_at(&next, 1), label) == 0) {
            
            found = true;
            *position = atoi(string_vector_at(&next, 2));
        }
    }
    
    string_array_free(&array);
    
    return error;
}

MorseFeedError write_saved_position(const char *state_path, const char *label, size_t position)
{
    MorseFeedError error = MF_NO_ERROR;
    StringArray array = read_string_array(state_path);
    StringVector new_entry = string_vector_create(3);
    size_t found_index = 0;
    bool found = false;
    bool changed = false;
    char position_str[32];
    
    for (size_t index = 0; index < array.size && !found; index++) {
        StringVector next;
        if (vector_at(&array, index, &next) && next.size == 3 &&
            strcmp(string_vector_at(&next, 0), "position") == 0 &&
            strcmp(string_vector_at(&next, 1), label) == 0) {
            
            found = true;
            found_index = index;
        }
    }
    
    if (position == 0) {
        if (found) {
            StringVector previous;
            vector_delete_at(&array, found_index, &previous);
            string_vector_free(&previous);
            changed = true;
        }
        
    } else {
        sprintf(position_str, "%ld", (long)position);
        if (new_entry.p == NULL || !string_vector_push(&new_entry, "position") ||
            !string_vector_push(&new_entry, label) ||
            !string_vector_push(&new_entry, position_str)) {
            
            string_vector_free(&new_entry);
            error = MF_OUT_OF_MEMORY;
        }
        
        if (error == MF_NO_ERROR) {
            if (found) {
                StringVector previous;
                vector_replace_at(&array, found_index, &new_entry, &previous);
                string_vector_free(&previous);
                changed = true;
                
            } else {
                if (!vector_push(&array, &new_entry)) {
                    string_vector_free(&new_entry);
                    error = MF_OUT_OF_MEMORY;
                }
                changed = true;
            }
        }
    }
    
    if (changed && error == MF_NO_ERROR) {
        write_string_array(state_path, &array);
    }
    
    string_array_free(&array);
    
    return error;
}

MorseFeedError write_token(char *token, FILE *out_file, FILE *pipe_to_mbeep, FILE *pipe_from_mbeep,
                           int words_per_row, int *word_number, int word_count,
                           bool use_key_control, bool filter_html, bool *excluding_tag,
                           char entity[ENTITY_SIZE], char tag[TAG_SIZE])
{
    MorseFeedError error = MF_NO_ERROR;
    char word[LINE_SIZE];
    size_t word_length = 0;
    size_t index = 0;
    bool latin1 = false;

#ifdef DEBUG
    fprintf(stderr, "strlen(%s) = %d\n", token, (int)strlen(token));
#endif

    while (index < strlen(token) && word_length < LINE_SIZE - 1) {
        char *name = "";
        unsigned char c = token[index];

        if (filter_html && c == '<') {
            size_t len = strlen(tag);
            *excluding_tag = true;
            if (len < TAG_SIZE - 1) {
                tag[len++] = c;
                tag[len] = '\0';
            }

        } else if (*excluding_tag && c == '>') {
            size_t len = strlen(tag);
            *excluding_tag = false;
            if (len < TAG_SIZE - 1) {
                tag[len++] = c;
                tag[len] = '\0';
            }

            if (strcmp(tag, "</li>") == 0) {
                name = "|";

            } else {
                name = " ";
            }

            tag[0] = '\0';

        } else if (*excluding_tag) {
            size_t len = strlen(tag);
            if (len < TAG_SIZE - 1) {
                tag[len++] = c;
                tag[len] = '\0';
            }

        } else if (filter_html && c == '&') {
            strcpy(entity, "&");
            
        } else if (filter_html && entity[0] != '\0') {
            size_t len = strlen(entity);
            if (len < ENTITY_SIZE - 1) {
                entity[len++] = c;
                entity[len] = '\0';
            }
            
            if (c == ';') {
                if (strcmp(entity, "&amp;") == 0) {
                    name = "andsign";
                    
                } else if (strcmp(entity, "&#x27;") == 0) {
                    // skip - ambiguous whether quote or apostrophe
                    //name = "apostrophe";
                    
                } else if (strcmp(entity, "&quot;") == 0) {
                    name = word_length == 0 ? "quote" : "unquote";
                    
                } else if (strcmp(entity, "&middot;") == 0) {
                    name = "dot";
                    
                } else if (strcmp(entity, "&gt;") == 0) {
                    name = "greaterthan";
                    
                } else if (strcmp(entity, "&lt;") == 0) {
                    name = "lessthan";
                    
                } else if (strcmp(entity, "&copy;") == 0) {
                    name = "copyright";
                
                } else {
#ifdef DEBUG
                    fprintf(stderr, "entity: %s\n", entity);
#endif
                }
                
                entity[0] = '\0';
            }

        } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            c == '.' || c == ',' || c == '?' || c == '/') {
            word[word_length++] = toupper(c);
            
        } else if (c == '\'') {
            // skip - ambiguous whether quote or apostrophe
            // word[word_length++] = '/';
            
        } else if (c <= 0x7F) {
            switch (c) {
                case '!':   name = "exclamation";                           break;
                case '"':   name = word_length == 0 ? "quote" : "unquote";  break;
                case '#':   name = "hashmark";                              break;
                case '$':   name = "dollarsign";                            break;
                case '%':   name = "percent";                               break;
                case '&':   name = "andsign";                               break;
                case '(':   name = "openparen";                             break;
                case ')':   name = "closeparen";                            break;
                case '*':   name = "asterisk";                              break;
                case '+':   name = "plus";                                  break;
                case '-':   name = "dash";                                  break;
                case ':':   name = "colon";                                 break;
                case ';':   name = "semicolon";                             break;
                case '<':   name = "lessthan";                              break;
                case '>':   name = "greaterthan";                           break;
                case '=':   name = "=";                                     break;
                case '@':   name = "atsign";                                break;
                case '[':   name = "leftbracket";                           break;
                case '\\':  name = "backslash";                             break;
                case ']':   name = "rightbracket";                          break;
                case '^':   name = "caret";                                 break;
                case '_':   name = "underscore";                            break;
                case '`':   name = "backtick";                              break;
                case '{':   name = "leftcurly";                             break;
                case '|':   name = "verticalbar";                           break;
                case '}':   name = "rightcurly";                            break;
                case '~':   name = "tilde";                                 break;
            }
            
        } else if ((c & 0b11100000) == 0b11000000) {
            // 2 byte UTF-8
#ifdef DEBUG
            fprintf(stderr, "%X %X\n", c, (unsigned char)token[index + 1]);
#endif
            
            if ((index + 1 < strlen(token)) && ((token[index + 1] & 0b11000000) == 0b10000000)) {
                // valid
#ifdef DEBUG
                fprintf(stderr, "(uint32_t)c & 0b00011111 %X\n", (uint32_t)c & 0b00011111);
                fprintf(stderr, "((uint32_t)c & 0b00011111) << 6) %X\n", ((uint32_t)c & 0b00011111) << 6);
                fprintf(stderr, "(token[index + 1] & 0b00111111) %X\n", (token[index + 1] & 0b00111111));
#endif
                
                uint32_t u = (((uint32_t)c & 0b00011111) << 6) | (token[index + 1] & 0b00111111);
                if (u <= 0x00FF) {
                    c = (unsigned char)u;
                    latin1 = true;
                    
                } else {
                    switch (u) {
                        case /* “ */ 0x201C:    name = "quote";     break;
                        case /* ” */ 0x201D:    name = "unquote";   break;
                            
                        case /* © */ 0xC2A9:    name = "copyright"; break;
                            
                        // skip - ambiguous whether quote or apostrophe
                        //case /* ʼ */ 0x02BC:    name = "/";         break;
                        //case /* ‘ */ 0x2018:    name = "/";         break;
                        //case /* ’ */ 0x2019:    name = "/";         break;
                            
                    }
                }
                
                index++;
                
            } else {
                latin1 = true;
            }
            
        } else if ((c & 0b11110000) == 0b11100000) {
            // 3 byte UTF-8
            if ((index + 2 < strlen(token)) && ((token[index + 1] & 0b11000000) == 0b10000000) &&
                ((token[index + 2] & 0b11000000) == 0b10000000)) {
                // valid
                index += 2;
                
            } else {
                latin1 = true;
            }
            
        } else if ((c & 0b11111000) == 0b11110000) {
            // 4 byte UTF-8
            if ((index + 3 < strlen(token)) && ((token[index + 1] & 0b11000000) == 0b10000000) &&
                ((token[index + 2] & 0b11000000) == 0b10000000) &&
                ((token[index + 3] & 0b11000000) == 0b10000000)) {
                // valid
                index += 3;
                
            } else {
                latin1 = true;
            }
            
        } else {
            latin1 = true;
        }
        
        if (latin1) {
            switch (c) {
                case /* ¡ */ 0xA1:  name = "exclamation";       break;
                case /* ¢ */ 0xA2:  name = "cents";             break;
                case /* £ */ 0xA3:  name = "pounds";            break;
                case /* ¤ */ 0xA4:  name = "currency";          break;
                case /* ¥ */ 0xA5:  name = "yen";               break;
                case /* ¦ */ 0xA6:  name = "brokenbar";         break;
                case /* § */ 0xA7:  name = "section";           break;

                case /* © */ 0xA9:  name = "copyright";         break;
                case /* « */ 0xAB:  name = "anglequote";        break;
                case /* ¬ */ 0xAC:  name = "notsign";           break;

                case /* ® */ 0xAE:  name = "registered";        break;

                case /* ° */ 0xB0:  name = "degrees";           break;
                case /* ± */ 0xB1:  name = "plusorminus";       break;
                case /* ´ */ 0xB4:  name = "accent";            break;
                case /* µ */ 0xB5:  name = "mu";                break;
                case /* ¶ */ 0xB6:  name = "paragraph";         break;
                case /* · */ 0xB7:  name = "cdot";              break;

                case /* » */ 0xBB:  name = "angleunquote";      break;
                case /* ÷ */ 0xF7:  name = "dividedby";         break;
            }

            if (strlen(name) == 0) {
                char *cstr = NULL;

                switch (c) {
                    case /* ª */ 0xAA:  cstr = "a";                 break;

                    case /* ² */ 0xB2:  cstr = "2";                 break;
                    case /* ³ */ 0xB3:  cstr = "3";                 break;

                    case /* ¹ */ 0xB9:  cstr = "1";                 break;
                    case /* º */ 0xBA:  cstr = "o";                 break;
                    case /* ¼ */ 0xBC:  cstr = "1/4";               break;
                    case /* ½ */ 0xBD:  cstr = "1/2";               break;
                    case /* ¾ */ 0xBE:  cstr = "3/4";               break;
                    case /* ¿ */ 0xBF:  cstr = "?";                 break;
                    case /* À */ 0xC0:  cstr = "A";                 break;
                    case /* Á */ 0xC1:  cstr = "A";                 break;
                    case /* Â */ 0xC2:  cstr = "A";                 break;
                    case /* Ã */ 0xC3:  cstr = "A";                 break;
                    case /* Ä */ 0xC4:  cstr = "A";                 break;
                    case /* Å */ 0xC5:  cstr = "A";                 break;
                    case /* Æ */ 0xC6:  cstr = "AE";                break;
                    case /* Ç */ 0xC7:  cstr = "C";                 break;
                    case /* È */ 0xC8:  cstr = "E";                 break;
                    case /* É */ 0xC9:  cstr = "E";                 break;
                    case /* Ê */ 0xCA:  cstr = "E";                 break;
                    case /* Ë */ 0xCB:  cstr = "E";                 break;
                    case /* Ì */ 0xCC:  cstr = "I";                 break;
                    case /* Í */ 0xCD:  cstr = "I";                 break;
                    case /* Î */ 0xCE:  cstr = "I";                 break;
                    case /* Ï */ 0xCF:  cstr = "I";                 break;
                    case /* Ð */ 0xD0:  cstr = "D";                 break;
                    case /* Ñ */ 0xD1:  cstr = "N";                 break;
                    case /* Ò */ 0xD2:  cstr = "O";                 break;
                    case /* Ó */ 0xD3:  cstr = "O";                 break;
                    case /* Ô */ 0xD4:  cstr = "O";                 break;
                    case /* Õ */ 0xD5:  cstr = "O";                 break;
                    case /* Ö */ 0xD6:  cstr = "O";                 break;
                    case /* × */ 0xD7:  cstr = "x";                 break;
                    case /* Ø */ 0xD8:  cstr = "O";                 break;
                    case /* Ù */ 0xD9:  cstr = "U";                 break;
                    case /* Ú */ 0xDA:  cstr = "U";                 break;
                    case /* Û */ 0xDB:  cstr = "U";                 break;
                    case /* Ü */ 0xDC:  cstr = "U";                 break;
                    case /* Ý */ 0xDD:  cstr = "Y";                 break;
                    case /* Þ */ 0xDE:  cstr = "TH";                break;
                    case /* ß */ 0xDF:  cstr = "ss";                break;
                    case /* à */ 0xE0:  cstr = "a";                 break;
                    case /* á */ 0xE1:  cstr = "a";                 break;
                    case /* â */ 0xE2:  cstr = "a";                 break;
                    case /* ã */ 0xE3:  cstr = "a";                 break;
                    case /* ä */ 0xE4:  cstr = "a";                 break;
                    case /* å */ 0xE5:  cstr = "a";                 break;
                    case /* æ */ 0xE6:  cstr = "ae";                break;
                    case /* ç */ 0xE7:  cstr = "c";                 break;
                    case /* è */ 0xE8:  cstr = "e";                 break;
                    case /* é */ 0xE9:  cstr = "e";                 break;
                    case /* ê */ 0xEA:  cstr = "e";                 break;
                    case /* ë */ 0xEB:  cstr = "e";                 break;
                    case /* ì */ 0xEC:  cstr = "i";                 break;
                    case /* í */ 0xED:  cstr = "i";                 break;
                    case /* î */ 0xEE:  cstr = "i";                 break;
                    case /* ï */ 0xEF:  cstr = "i";                 break;
                    case /* ð */ 0xF0:  cstr = "th";                break;
                    case /* ñ */ 0xF1:  cstr = "n";                 break;
                    case /* ò */ 0xF2:  cstr = "o";                 break;
                    case /* ó */ 0xF3:  cstr = "o";                 break;
                    case /* ô */ 0xF4:  cstr = "o";                 break;
                    case /* õ */ 0xF5:  cstr = "o";                 break;
                    case /* ö */ 0xF6:  cstr = "o";                 break;
                    case /* ø */ 0xF8:  cstr = "o";                 break;
                    case /* ù */ 0xF9:  cstr = "u";                 break;
                    case /* ú */ 0xFA:  cstr = "u";                 break;
                    case /* û */ 0xFB:  cstr = "u";                 break;
                    case /* ü */ 0xFC:  cstr = "u";                 break;
                    case /* ý */ 0xFD:  cstr = "y";                 break;
                    case /* þ */ 0xFE:  cstr = "th";                break;
                    case /* ÿ */ 0xFF:  cstr = "y";                 break;
                }

                if (cstr != NULL) {
                    size_t cstr_len = strlen(cstr);
                    if (word_length + cstr_len < LINE_SIZE) {
                        memcpy(&word[word_length], cstr, cstr_len);
                        word_length += cstr_len;

                    } else {
                        // no room, treat as name
                        name = cstr;
                    }
                }
            }
        }
        
        if (strlen(name) > 0) {
            if (word_length > 0) {
                word[word_length] = '\0';
                error = write_word(word, out_file, pipe_to_mbeep, pipe_from_mbeep, words_per_row,
                                   word_number, word_count, use_key_control);
                word_length = 0;
            }

            if (error == MF_NO_ERROR) {
                error = write_word(name, out_file, pipe_to_mbeep, pipe_from_mbeep, words_per_row,
                                   word_number, word_count, use_key_control);
            }
        }
        
        index++;
    }
    
    if (index < strlen(token)) error = MF_PROGRAM_ERR;
    
    if (error == MF_NO_ERROR && word_length > 0) {
        word[word_length] = '\0';
        error = write_word(word, out_file, pipe_to_mbeep, pipe_from_mbeep, words_per_row,
                           word_number, word_count, use_key_control);
    }
    
    return error;
}

MorseFeedError write_word(char *word, FILE *out_file, FILE *pipe_to_mbeep, FILE *pipe_from_mbeep,
                          int words_per_row, int *word_number, int word_count, bool use_key_control)
{
    MorseFeedError error = MF_NO_ERROR;
    FILE *output = pipe_to_mbeep != NULL ? pipe_to_mbeep : out_file;
    bool paused = false;

    if (use_key_control) {
        do {
            char c;
            int count = (int)fread(&c, 1, 1, stdin);
            if (count != 0) {
                if (c == ' ') {
                    paused = !paused;
                    //if (paused) usleep(500000L);
                    if (paused) {
                        struct timespec ts = { 0, 500000000L };
                        nanosleep(&ts, NULL);
                    }

                } else if (c == 'q' || c == 'Q') {
                    error = MF_EXIT;

                } else if (c == 'n' || c == 'N') {
                    error = MF_NEXT;
                }
            }
            
        } while (paused && error == MF_NO_ERROR);
    }
    
    if (error == MF_NO_ERROR) {
#ifdef DEBUG
        fprintf(stderr, "write word(%s)\n", word);
#endif
        
        if (*word_number % words_per_row == 0) {
            // no preceeding space
            
        } else {
            if (fprintf(output, " ") < 0) error = MF_FILE_WRITE_ERROR;
        }
        
        if (fprintf(output, "%s", word) < 0) error = MF_FILE_WRITE_ERROR;
        
        if (*word_number % words_per_row == words_per_row - 1) {
            if (fprintf(output, "\n") < 0) error = MF_FILE_WRITE_ERROR;
            
            if (pipe_to_mbeep != NULL) {
                fflush(pipe_to_mbeep);
            }
            
            if (pipe_from_mbeep != NULL) {
                char echo_str[LINE_SIZE];
                
                char *got = fgets(echo_str, LINE_SIZE, pipe_from_mbeep);
#ifdef DEBUG
                if (got != NULL) {
                    fprintf(stderr, "echoed '%s'", got);
                }
#endif
                if (got == NULL) error = MF_PIPE_ERROR;
            }
        }
        
        if (strlen(word ) != 0 && strcmp(word, " ") != 0) (*word_number)++;

        if (word_count != DEFAULT && *word_number >= word_count) error = MF_EXIT;
    }
    
    return error;
}

struct termios previous_termios;
int flags;

void signal_handler(int signum)
{
    char *message = "signal_handler(<unknown>)\n";
    switch (signum) {
        case SIGINT:    message = " signal_handler(SIGINT)\n";    break;
        case SIGHUP:    message = " signal_handler(SIGHUP)\n";    break;
        case SIGQUIT:   message = " signal_handler(SIGQUIT)\n";   break;
        case SIGTERM:   message = " signal_handler(SIGTERM)\n";   break;
        case SIGSEGV:   message = " signal_handler(SIGSEGV)\n";   break;
        case SIGPIPE:   message = " signal_handler(SIGPIPE)\n";   break;
        case SIGCHLD:   message = " signal_handler(SIGCHLD)\n";   break;
        case SIGTSTP:   message = " signal_handler(SIGTSTP)\n";   break;
    }
    
    write(STDERR_FILENO, message, strlen(message));
    tcsetattr(STDIN_FILENO, TCSANOW, &previous_termios);
    fcntl(STDIN_FILENO, F_SETFL, flags);

    if (signum == SIGSEGV) {
        // see 'man backtrace'
        void* callstack[128];
        int i, frames = backtrace(callstack, 128);
        char** strs = backtrace_symbols(callstack, frames);

        write(STDERR_FILENO, "Stack:\n", strlen("Stack:\n"));
        for (i = 0; i < frames; ++i) {
            write(STDERR_FILENO, strs[i], strlen(strs[i]));
            write(STDERR_FILENO, "\n", 1);
        }
        free(strs);
    }
    
    if (signum != SIGCHLD) _exit(EXIT_FAILURE);
}

// #define TWO_WAY_POPEN

void init_fork_mbeep(bool use_key_control)
{
    struct termios raw;

    tcgetattr(STDIN_FILENO, &previous_termios);
    flags = fcntl(STDIN_FILENO, F_GETFL, 0);

    {
        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGHUP, &sa, NULL);
        sigaction(SIGQUIT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGPIPE, &sa, NULL);
        sigaction(SIGCHLD, &sa, NULL);
        sigaction(SIGTSTP, &sa, NULL);
    }

    if (use_key_control) {
        tcgetattr(STDIN_FILENO, &raw);
        raw.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);

        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
}

MorseFeedError begin_fork_mbeep(FILE **pipe_to_mbeep, FILE **pipe_from_mbeep, pid_t *pid,
                                double freq, double paris_wpm, double codex_wpm, double farnsworth_wpm,
                                bool print_fcc_wpm, bool use_key_control)
{
    MorseFeedError error = MF_NO_ERROR;
    struct termios raw;

    tcgetattr(STDIN_FILENO, &previous_termios);
    flags = fcntl(STDIN_FILENO, F_GETFL, 0);

    {
        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGHUP, &sa, NULL);
        sigaction(SIGQUIT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGPIPE, &sa, NULL);
        sigaction(SIGCHLD, &sa, NULL);
        sigaction(SIGTSTP, &sa, NULL);
    }

    if (use_key_control) {
        tcgetattr(STDIN_FILENO, &raw);
        raw.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);

        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

#define MAX_COMMAND 256
#define MAX_PART 64

#ifdef TWO_WAY_POPEN
    FILE *pipe = NULL;
    if (error == MF_NO_ERROR) {
        char command[MAX_COMMAND];
        char part[MAX_PART];
        strcpy(KEY_COMMAND, "mbeep -e -I");

        if (freq != DEFAULT) {
            if (snprintf(part, MAX_PART, " -f %.3f", freq) >= MAX_PART) error = MF_PROGRAM_ERR;
            if (strlcat(command, part, MAX_COMMAND) >= MAX_COMMAND) error = MF_PROGRAM_ERR;
        }

        if (paris_wpm != DEFAULT) {
            if (snprintf(part, MAX_PART, " -w %.3f", paris_wpm) >= MAX_PART) error = MF_PROGRAM_ERR;
            if (strlcat(command, part, MAX_COMMAND) >= MAX_COMMAND) error = MF_PROGRAM_ERR;

        } else if (codex_wpm != DEFAULT) {
            if (snprintf(part, MAX_PART, " --codex-wpm %.3f", codex_wpm) >= MAX_PART) error = MF_PROGRAM_ERR;
            if (strlcat(command, part, MAX_COMMAND) >= MAX_COMMAND) error = MF_PROGRAM_ERR;
        }

        if (farnsworth_wpm != DEFAULT) {
            if (snprintf(part, MAX_PART, " -x %.3f", farnsworth_wpm) >= MAX_PART) error = MF_PROGRAM_ERR;
            if (strlcat(command, part, MAX_COMMAND) >= MAX_COMMAND) error = MF_PROGRAM_ERR;
        }

        if (print_fcc_wpm) {
            if (strlcat(command, " --fcc", MAX_COMMAND) >= MAX_COMMAND) error = MF_PROGRAM_ERR;
        }

        if (strlcat(command, " -c", MAX_COMMAND) >= MAX_COMMAND) error = MF_PROGRAM_ERR;

        if (error == MF_NO_ERROR) {
            pipe = popen(command, "r+");
            *pipe_to_mbeep = pipe;
            *pipe_from_mbeep = pipe;
            if (pipe == NULL) error = MF_PIPE_ERROR;
        }
    }

#else
    // read from [0], write to [1]
    int from_parent_pipes[2];
    int to_parent_pipes[2];

    if (error == MF_NO_ERROR && pipe(from_parent_pipes)) error = MF_PIPE_ERROR;
    if (error == MF_NO_ERROR && pipe(to_parent_pipes)) error = MF_PIPE_ERROR;

    *pid = -1;
    if (error == MF_NO_ERROR) {
        *pid = fork();
        if (*pid == 0) {
            // child
#define MAX_ARGS 11     // mbeep -e -I -f NNN -w NNN -x NNN -c NULL
            char *mbeep_args[MAX_ARGS];
            size_t arg_count = 0;
            char freq_part[MAX_PART];
            char wpm_part[MAX_PART];
            char farnsworth_part[MAX_PART];

            mbeep_args[arg_count++] = "mbeep";
            mbeep_args[arg_count++] = "-e";
            mbeep_args[arg_count++] = "-I";

            if (freq != DEFAULT) {
                mbeep_args[arg_count++] = "-f";
                if (snprintf(freq_part, MAX_PART, "%.3f", freq) >= MAX_PART) error = MF_PROGRAM_ERR;
                mbeep_args[arg_count++] = freq_part;
            }

            if (paris_wpm != DEFAULT) {
                mbeep_args[arg_count++] = "-w";
                if (snprintf(wpm_part, MAX_PART, "%.3f", paris_wpm) >= MAX_PART) {
                    error = MF_PROGRAM_ERR;
                }
                mbeep_args[arg_count++] = wpm_part;

            } else if (codex_wpm != DEFAULT) {
                mbeep_args[arg_count++] = "--codex-wpm";
                if (snprintf(wpm_part, MAX_PART, "%.3f", codex_wpm) >= MAX_PART) {
                    error = MF_PROGRAM_ERR;
                }
                mbeep_args[arg_count++] = wpm_part;
            }

            if (farnsworth_wpm != DEFAULT) {
                mbeep_args[arg_count++] = "-x";
                if (snprintf(farnsworth_part, MAX_PART, "%.3f", farnsworth_wpm) >= MAX_PART) {
                    error = MF_PROGRAM_ERR;
                }
                mbeep_args[arg_count++] = farnsworth_part;
            }

            if (print_fcc_wpm) {
                mbeep_args[arg_count++] = "--fcc";
            }

            mbeep_args[arg_count++] = "-c";
            mbeep_args[arg_count++] = NULL;

            if (error == MF_NO_ERROR) {
                dup2(from_parent_pipes[0], STDIN_FILENO);
                dup2(to_parent_pipes[1], STDOUT_FILENO);
                close(from_parent_pipes[1]);
                close(to_parent_pipes[0]);

                execvp("mbeep", mbeep_args);
            }

            // error if get here
            exit(EXIT_FAILURE);

        } else if (*pid < 0) {
            // error
            error = MF_FORK_ERROR;
        }
    }

    // parent continues
    if (error == MF_NO_ERROR) {
        close(from_parent_pipes[0]);
        close(to_parent_pipes[1]);

        *pipe_from_mbeep = fdopen(to_parent_pipes[0], "r");
        *pipe_to_mbeep = fdopen(from_parent_pipes[1], "w");

        if (*pipe_to_mbeep == NULL || *pipe_from_mbeep == NULL) error = MF_PIPE_ERROR;
    }
#endif

    return error;
}

MorseFeedError end_fork_mbeep(FILE *pipe_to_mbeep, FILE *pipe_from_mbeep, pid_t pid)
{
    MorseFeedError error = MF_NO_ERROR;

    tcsetattr(STDIN_FILENO, TCSANOW, &previous_termios);
    fcntl(STDIN_FILENO, F_SETFL, flags);

#ifdef TWO_WAY_POPEN
    if (pipe_to_mbeep != NULL) pclose(pipe_to_mbeep);
#else
    if (pipe_to_mbeep != NULL) fclose(pipe_to_mbeep);
    if (pipe_from_mbeep != NULL) fclose(pipe_from_mbeep);
#endif

    if (pid > 0) wait(NULL);    //kill(pid, SIGTERM);

    return error;
}

size_t find_string(const char *string, const char *buffer, size_t buffer_length,
                           size_t starting_at)
{
    size_t found_at = buffer_length;

    void *p = memmem(&buffer[starting_at], buffer_length - starting_at, string, strlen(string));

    if (p != NULL) {
        found_at = starting_at + (p - (void *)&buffer[starting_at]);
    }

    return found_at;
}

void extract_urls(const char *base_url, const char *str, size_t start_index, size_t end_index,
                  StringVector *urls, StringVector *titles)
{
#define BEGIN_URL "<a href=\""
#define END_URL "\""
#define BEGIN_TITLE ">"
#define END_TITLE "</a>"
#define URL_SIZE 1024
#define TITLE_SIZE 128
#define HTTP_PREFIX "http://"
#define HTTPS_PREFIX "https://"

    char url[URL_SIZE];
    char title[TITLE_SIZE];
    size_t found_at = start_index;

    do {
        size_t offset;
        bool got_url = false;

        // URL
        found_at = find_string(BEGIN_URL, str, end_index, found_at);
        offset = found_at + strlen(BEGIN_URL);
        if (offset < end_index) {
            found_at = find_string(END_URL, str, end_index, offset);
        }

        if (offset < found_at) {
            size_t url_length = found_at - offset;
            if (url_length < URL_SIZE && found_at < end_index) {
                strncpy(url, &str[offset], url_length);
                url[url_length] = '\0';

                if (strncmp(url, HTTP_PREFIX, strlen(HTTP_PREFIX)) == 0) {
                    string_vector_push(urls, url);
                    got_url = true;

                } else if (strncmp(url, HTTPS_PREFIX, strlen(HTTPS_PREFIX)) == 0) {
                    string_vector_push(urls, url);
                    got_url = true;

                } else if (strlen(base_url) + strlen("/") + strlen(url) > URL_SIZE - 1) {
                    // too big
                    // (allowing for extra '/', which might be needed)

                } else if (url[0] == '/') {
                    char full_url[URL_SIZE];
                    size_t slash_at;

                    strcpy(full_url, base_url);
                    slash_at = find_string("//", full_url, strlen(full_url), 0);
                    if (slash_at + 2 < strlen(full_url)) {
                        slash_at = find_string("/", full_url, strlen(full_url), slash_at + 2);
                    }

                    if (slash_at < strlen(full_url)) {
                        full_url[slash_at] = '\0';
                    }

                    strcat(full_url, url);
                    string_vector_push(urls, full_url);
                    got_url = true;

                } else {
                    char full_url[URL_SIZE];

                    strcpy(full_url, base_url);
                    if (full_url[strlen(full_url) - 1] != '/') {
                        strcat(full_url, "/");
                    }

                    strcat(full_url, url);
                    string_vector_push(urls, full_url);
                    got_url = true;
                }
            }
        }

        // title
        if (got_url) {
            bool got_title = false;

            found_at = find_string(BEGIN_TITLE, str, end_index, found_at);
            offset = found_at + strlen(BEGIN_TITLE);
            if (offset < end_index) {
                found_at = find_string(END_TITLE, str, end_index, offset);
            }

            if (offset < found_at && found_at < end_index) {
                size_t title_length = found_at - offset;
                if (title_length > TITLE_SIZE - 1) title_length = TITLE_SIZE - 1;

                strncpy(title, &str[offset], title_length);
                title[title_length] = '\0';
                string_vector_push(titles, title);
                got_title = true;
            }

            if (!got_title) string_vector_push(titles, "");
        }

    } while (found_at < end_index);
}

#define STATE_VECTOR_SIZE 17

bool replace_with_copy_or_null(const char **str, StringVector *string_storage);
bool replace_with_copy_or_null(const char **str, StringVector *string_storage)
{
    bool mem_error = false;
    
    if (*str == NULL) {
        // leave as is
        
    } else if (strlen(*str) == 0) {
        // replace with NULL
        *str = NULL;
        
    } else {
        // make copy
        mem_error = !string_vector_push(string_storage, *str);
        if (!mem_error) {
            *str = string_vector_at(string_storage, string_storage->size - 1);
        }
    }
    
    return mem_error;
}

MorseFeedError read_state(const char *label, MorseFeedParams *mfp, StringVector *string_storage)
{
    MorseFeedError error = MF_NO_ERROR;
    StringArray array = read_string_array(mfp->state_path);
    bool found = false;
    
    for (size_t k = 0; k < array.size && !found; k++) {
        StringVector next;
        if (vector_at(&array, k, &next) && next.size == STATE_VECTOR_SIZE &&
            strcmp(string_vector_at(&next, 0), "state") == 0 &&
            strcmp(string_vector_at(&next, 1), label) == 0) {
            
            bool mem_error = false;
            
            found = true;
            
            mfp->in_file_name = string_vector_at(&next, 2);
            mem_error |= replace_with_copy_or_null(&mfp->in_file_name, string_storage);
            
            mfp->url = string_vector_at(&next, 3);
            mem_error |= replace_with_copy_or_null(&mfp->url, string_storage);
            
            mfp->words_per_row = atoi(string_vector_at(&next, 4));
            mfp->word_count = atoi(string_vector_at(&next, 5));
            
            mfp->fork_mbeep = atoi(string_vector_at(&next, 6)) != 0;
            mfp->save_and_use_position = atoi(string_vector_at(&next, 7)) != 0;
            mfp->follow_links = atoi(string_vector_at(&next, 8)) != 0;
            
            mfp->text_after = string_vector_at(&next, 9);
            mem_error |= replace_with_copy_or_null(&mfp->text_after, string_storage);
            
            mfp->text_before = string_vector_at(&next, 10);
            mem_error |= replace_with_copy_or_null(&mfp->text_before, string_storage);
            
            mfp->linked_text_after = string_vector_at(&next, 11);
            mem_error |= replace_with_copy_or_null(&mfp->linked_text_after, string_storage);
            
            mfp->linked_text_before = string_vector_at(&next, 12);
            mem_error |= replace_with_copy_or_null(&mfp->linked_text_before, string_storage);
            
            mfp->freq = atof(string_vector_at(&next, 13));
            mfp->paris_wpm = atof(string_vector_at(&next, 14));
            mfp->codex_wpm = atof(string_vector_at(&next, 15));
            mfp->farnsworth_wpm = atof(string_vector_at(&next, 16));
            
            if (mem_error) error = MF_OUT_OF_MEMORY;
        }
    }

    string_array_free(&array);

    if (!found) error = MF_UNKNOWN_SAVED_STATE;

    return error;
}

MorseFeedError save_state(const char *label, const MorseFeedParams *mfp)
{
    MorseFeedError error = MF_NO_ERROR;
    StringArray array = read_string_array(mfp->state_path);
    StringVector new_entry = string_vector_create(STATE_VECTOR_SIZE);
    bool found = false;
    size_t found_index = 0;

    if (new_entry.p == NULL) {
        error = MF_OUT_OF_MEMORY;
        
    } else {
        char str[32];
        bool push_error = !string_vector_push(&new_entry, "state") ||
            !string_vector_push(&new_entry, label);
        
        push_error |= !string_vector_push(&new_entry, mfp->in_file_name == NULL ? "" : mfp->in_file_name);
        push_error |= !string_vector_push(&new_entry, mfp->url == NULL ? "" : mfp->url);
        
        sprintf(str, "%d", mfp->words_per_row);
        push_error |= !string_vector_push(&new_entry, str);
        
        sprintf(str, "%d", mfp->word_count);
        push_error |= !string_vector_push(&new_entry, str);
        
        push_error |= !string_vector_push(&new_entry, mfp->fork_mbeep ? "1" : "0");
        push_error |= !string_vector_push(&new_entry, mfp->save_and_use_position ? "1" : "0");
        push_error |= !string_vector_push(&new_entry, mfp->follow_links ? "1" : "0");
        
        push_error |= !string_vector_push(&new_entry, mfp->text_after == NULL ? "" : mfp->text_after);
        push_error |= !string_vector_push(&new_entry, mfp->text_before == NULL ? "" : mfp->text_before);
        push_error |= !string_vector_push(&new_entry, mfp->linked_text_after == NULL ? "" : mfp->linked_text_after);
        push_error |= !string_vector_push(&new_entry, mfp->linked_text_before == NULL ? "" : mfp->linked_text_before);
        
        sprintf(str, "%12.3f", mfp->freq);
        push_error |= !string_vector_push(&new_entry, str);
        
        sprintf(str, "%12.3f", mfp->paris_wpm);
        push_error |= !string_vector_push(&new_entry, str);
        
        sprintf(str, "%12.3f", mfp->codex_wpm);
        push_error |= !string_vector_push(&new_entry, str);
        
        sprintf(str, "%12.3f", mfp->farnsworth_wpm);
        push_error |= !string_vector_push(&new_entry, str);
        
        if (push_error) {
            string_vector_free(&new_entry);
            error = MF_OUT_OF_MEMORY;
        }
    }
    
    if (error == MF_NO_ERROR) {
        for (size_t index = 0; index < array.size && !found; index++) {
            StringVector next;
            if (vector_at(&array, index, &next) && next.size == 17 &&
                strcmp(string_vector_at(&next, 0), "state") == 0 &&
                strcmp(string_vector_at(&next, 1), label) == 0) {
                
                found = true;
                found_index = index;
            }
        }
        
        if (found) {
            StringVector previous;
            vector_replace_at(&array, found_index, &new_entry, &previous);
            string_vector_free(&previous);
            
        } else {
            if (!vector_push(&array, &new_entry)) {
                string_vector_free(&new_entry);
                error = MF_OUT_OF_MEMORY;
            }
        }
    }

    if (error == MF_NO_ERROR) {
        write_string_array(mfp->state_path, &array);
    }
    
    string_array_free(&array);

    return error;
}

#if DEBUG
bool same_or_nulls(const char *s1, const char *s2);
bool same_or_nulls(const char *s1, const char *s2)
{
    if (s1 == NULL && s2 == NULL) {
        return true;

    } else if (s1 == NULL || s2 == NULL) {
        return false;

    } else {
        return strcmp(s1, s2) == 0;
    }
}

bool same_state(MorseFeedParams *a, MorseFeedParams *b);
bool same_state(MorseFeedParams *a, MorseFeedParams *b)
{
    bool same = same_or_nulls(a->in_file_name, b->in_file_name);
    same = same && same_or_nulls(a->url, b->url);
    same = same && a->words_per_row == b->words_per_row;
    same = same && a->word_count == b->word_count;
    same = same && a->fork_mbeep == b->fork_mbeep;
    same = same && a->save_and_use_position == b->save_and_use_position;
    same = same && a->follow_links == b->follow_links;
    same = same && same_or_nulls(a->text_after, b->text_after);
    same = same && same_or_nulls(a->text_before, b->text_before);
    same = same && same_or_nulls(a->linked_text_after, b->linked_text_after);
    same = same && same_or_nulls(a->linked_text_before, b->linked_text_before);

    // test cases use integer values, so this is O:
    same = same && a->freq == b->freq;
    same = same && a->paris_wpm == b->paris_wpm;
    same = same && a->codex_wpm == b->codex_wpm;
    same = same && a->farnsworth_wpm == b->farnsworth_wpm;

    return same;
}


void morsefeed_tests(void)
{
    bool ok = true;
    printf("morsefeed_tests()\n");

    // find_string
    char *sample = "This is a small buffer of text.";
    ok &= print_if_fail(find_string("This", sample, strlen(sample), 0) == 0, "FAIL: find_string (1)");
    ok &= print_if_fail(find_string("is", sample, strlen(sample), 3) == 5, "FAIL: find_string (2)");
    ok &= print_if_fail(find_string(".", sample, strlen(sample), 0) == strlen(sample) - 1, "FAIL: find_string (3)");
    ok &= print_if_fail(find_string("foo", sample, strlen(sample), 0) == strlen(sample), "FAIL: find_string (4)");

    // read_saved_position write_saved_position
    size_t position = 999;
    ok &= print_if_fail(read_saved_position("state.tmp", "file1", &position) == MF_NO_ERROR, "FAIL: read_saved_position (1)");
    ok &= print_if_fail(position == 0, "FAIL: read_saved_position (2)");

    ok &= print_if_fail(write_saved_position("state.tmp", "file1", 11) == MF_NO_ERROR, "FAIL: write_saved_position (1)");
    ok &= print_if_fail(write_saved_position("state.tmp", "file2", 22) == MF_NO_ERROR, "FAIL: write_saved_position (2)");

    ok &= print_if_fail(read_saved_position("state.tmp", "file1", &position) == MF_NO_ERROR, "FAIL: read_saved_position (3)");
    ok &= print_if_fail(position == 11, "FAIL: read_saved_position (4)");
    ok &= print_if_fail(read_saved_position("state.tmp", "file2", &position) == MF_NO_ERROR, "FAIL: read_saved_position (5)");
    ok &= print_if_fail(position == 22, "FAIL: read_saved_position (6)");

    write_saved_position("state.tmp", "file1", 0);
    write_saved_position("state.tmp", "file2", 200);

    ok &= print_if_fail(read_saved_position("state.tmp", "file1", &position) == MF_NO_ERROR, "FAIL: read_saved_position (7)");
    ok &= print_if_fail(position == 0, "FAIL: read_saved_position (8)");
    ok &= print_if_fail(read_saved_position("state.tmp", "file2", &position) == MF_NO_ERROR, "FAIL: read_saved_position (9)");
    ok &= print_if_fail(position == 200, "FAIL: read_saved_position (10)");

    FILE *foo = fopen("state.tmp", "r");
    char txt[512];
    size_t fs = fread(txt, 1, 511, foo);
    fclose(foo);
    txt[fs] = '\0';
    printf("state.tmp:\n%s", txt);

    // save_state & read_state
    MorseFeedParams mfp1 = { "foo.txt", NULL, NULL, "https:://example.com", "state.tmp",
        1, 2, false, true, false, "one", "two", "three", "four", 12.0, 13.0, 14.0, 15.0 };

    MorseFeedParams mfp2 = { "bar.txt", NULL, NULL, "https:://foo.com", "state.tmp",
        3, 4, true, false, true, "alpha", "bravo", "charlie", "delta", 16.0, 17.0, 18.0, 19.0 };

    ok &= print_if_fail(save_state("one", &mfp1) == MF_NO_ERROR, "FAIL: save_state (1)");
    ok &= print_if_fail(save_state("two", &mfp2) == MF_NO_ERROR, "FAIL: save_state (2)");

    MorseFeedParams mfp;
    memset(&mfp, 1, sizeof(mfp));
    mfp.state_path = "state.tmp";

    StringVector storage = string_vector_create(0);
    ok &= print_if_fail(read_state("one", &mfp, &storage) == MF_NO_ERROR, "FAIL: read_state (1)");
    ok &= print_if_fail(same_state(&mfp1, &mfp), "FAIL: read_state (2)");
    ok &= print_if_fail(read_state("two", &mfp, &storage) == MF_NO_ERROR, "FAIL: read_state (3)");
    ok &= print_if_fail(same_state(&mfp2, &mfp), "FAIL: read_state (4)");

    ok &= print_if_fail(save_state("two", &mfp1) == MF_NO_ERROR, "FAIL: save_state (3)");
    ok &= print_if_fail(save_state("one", &mfp2) == MF_NO_ERROR, "FAIL: save_state (4)");

    ok &= print_if_fail(read_state("two", &mfp, &storage) == MF_NO_ERROR, "FAIL: read_state (5)");
    ok &= print_if_fail(same_state(&mfp1, &mfp), "FAIL: read_state (6)");
    ok &= print_if_fail(read_state("one", &mfp, &storage) == MF_NO_ERROR, "FAIL: read_state (7)");
    ok &= print_if_fail(same_state(&mfp2, &mfp), "FAIL: read_state (8)");

    remove("state.tmp");

    printf(ok ? "Others OK\n\n" : "Other FAILURE\n\n");

}
#endif

