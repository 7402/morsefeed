//
//  morsefeed.h
//  morsefeed
//
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

#ifndef morsefeed_h
#define morsefeed_h

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#include "vector.h"

#define DEFAULT -1
#define LINE_SIZE 1024
#define ENTITY_SIZE 16
#define TAG_SIZE 8

struct MorseFeedParams {
    // Input/Output
    const char *in_file_name;
    FILE *in_file;
    FILE *out_file;
    const char *url;
    char *state_path;

    // Options
    int words_per_row;
    int word_count;
    bool fork_mbeep;
    bool save_and_use_position;
    bool follow_links;
    const char *text_after;
    const char *text_before;
    const char *linked_text_after;
    const char *linked_text_before;

    // Passed to mbeep
    double freq;
    double paris_wpm;
    double codex_wpm;
    double farnsworth_wpm;
    bool print_fcc_wpm;
};
typedef struct MorseFeedParams MorseFeedParams;

struct BufferStruct {
    char *p;
    size_t capacity;    // current allocation
    size_t used;        // including terminating null
};
typedef struct BufferStruct BufferStruct;

typedef enum MorseFeedError {
    MF_NO_ERROR = 0,
    MF_PIPE_ERROR,
    MF_FORK_ERROR,
    MF_EXIT,
    MF_NEXT,
    MF_INVALID_VALUE,
    MF_OUT_OF_MEMORY,
    MF_INVALID_FREQUENCY,
    MF_INVALID_WPM,
    MF_INVALID_OPTION,
    MF_FILE_READ_ERROR,
    MF_URL_READ_ERROR,
    MF_INPUT_FILE_OPEN_ERROR,
    MF_OUTPUT_FILE_OPEN_ERROR,
    MF_POSITION_FILE_OPEN_ERROR,
    MF_FILE_ALREADY_OPEN_ERROR,
    MF_FILE_WRITE_ERROR,
    MF_PROGRAM_ERR,
    MF_NO_STATE_PATH,
    MF_UNKNOWN_SAVED_STATE,
    MF_UNKNOWN
} MorseFeedError;

MorseFeedError write_token(char *token, FILE *out_file, FILE *pipe_to_mbeep, FILE *pipe_from_mbeep,
                           int words_per_row, int *word_number, int word_count,
                           bool use_key_control, bool exclude_tags, bool *excluding,
                           char entity[ENTITY_SIZE], char tag[TAG_SIZE]);

MorseFeedError write_word(char *word, FILE *out_file, FILE *pipe_to_mbeep, FILE *pipe_from_mbeep,
                          int words_per_row, int *word_number, int word_count, bool use_key_control);

void init_fork_mbeep(bool use_key_control);

MorseFeedError begin_fork_mbeep(FILE **pipe_to_mbeep, FILE **pipe_from_mbeep, pid_t *pid,
                                double freq, double paris_wpm, double codex_wpm, double farnsworth_wpm,
                                bool print_fcc_wpm, bool use_key_control);

MorseFeedError end_fork_mbeep(FILE *pipe_to_mbeep, FILE *pipe_from_mbeep, pid_t pid);

size_t find_string(const char *string, const char *buffer, size_t buffer_length,
                   size_t starting_at);

MorseFeedError read_saved_position(const char *state_path, const char *label, size_t *position);
MorseFeedError write_saved_position(const char *state_path, const char *label, size_t position);

void init_buffer(BufferStruct *buffer, size_t capacity);
void free_buffer(BufferStruct *buffer);

size_t curl_write_data(void *buffer, size_t size, size_t nmemb, void *userp);
MorseFeedError url_to_buffer(const char *url, BufferStruct *buffer);

char *fbgets(char *line, int line_size, FILE *file, char *buffer, size_t buffer_size, size_t *next_index);

MorseFeedError process_and_send(MorseFeedParams mfp);

void extract_urls(const char *base_url, const char *str, size_t start_index, size_t end_index,
                  StringVector *urls, StringVector *titles);

MorseFeedError read_state(const char *label, MorseFeedParams *mfp, StringVector *string_storage);
MorseFeedError save_state(const char *label, const MorseFeedParams *mfp);

#if DEBUG
void morsefeed_tests(void);
#endif

#endif /* morsefeed_h */
