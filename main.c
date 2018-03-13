//
//  main.c
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

#include "morsefeed.h"
#include "text.h"
#include "vector.h"

#define STATE_FILE_NAME ".morsefeed"

void print_it(void *p);
void print_it(void *p) {
    char *txt = *(char **)p;
    printf("%s\n", txt);
}

int main(int argc, const char * argv[]) {
    MorseFeedError error = MF_NO_ERROR;
    const char *home = getenv("HOME");
    MorseFeedParams mfp;
    const char *state_label = NULL;
    StringVector string_storage = string_vector_create(0);

    mfp.in_file_name = NULL;
    mfp.in_file = NULL;
    mfp.out_file = NULL;
    mfp.url = NULL;
    mfp.state_path = NULL;

    mfp.words_per_row = DEFAULT;
    mfp.word_count = DEFAULT;
    mfp.fork_mbeep = false;
    mfp.save_and_use_position = false;
    mfp.follow_links = false;

    mfp.text_after = NULL;
    mfp.text_before = NULL;
    mfp.linked_text_after = NULL;
    mfp.linked_text_before = NULL;

    mfp.freq = DEFAULT;
    mfp.paris_wpm = DEFAULT;
    mfp.codex_wpm = DEFAULT;
    mfp.farnsworth_wpm = DEFAULT;

    // make path to state file
    if (home != NULL) {
        mfp.state_path = malloc(strlen(home) + strlen("/") + strlen(STATE_FILE_NAME) + 1);
        if (mfp.state_path != NULL) {
            strcpy(mfp.state_path, home);
            strcat(mfp.state_path, "/");
            strcat(mfp.state_path, STATE_FILE_NAME);
        }
    }
    
    for (int index = 1; index < argc && error == MF_NO_ERROR; index++) {
        //  -c words_per_row
        if (strcmp(argv[index], "-c") == 0 && index + 1 < argc) {
            mfp.words_per_row = atoi(argv[++index]);
            if (mfp.words_per_row < 1 || mfp.words_per_row > 100) error = MF_INVALID_VALUE;
            
        //  -n number of words
        } else if (strcmp(argv[index], "-n") == 0 && index + 1 < argc) {
            mfp.word_count = atoi(argv[++index]);
            if (mfp.word_count < 1) error = MF_INVALID_VALUE;
            
        //  -m --mbeep  (send to mbeep)
        } else if ((strcmp(argv[index], "--mbeep") == 0) ||
                   (strcmp(argv[index], "-m") == 0)) {
            mfp.fork_mbeep = true;
                
        //  -f  frequency [for mbeep]
        } else if (strcmp(argv[index], "-f") == 0 && index + 1 < argc) {
            mfp.freq = atof(argv[++index]);
            if (mfp.freq < 20.0 || mfp.freq > 20000.0) error = MF_INVALID_FREQUENCY;
            
        //  -w  --paris-wpm words per minute, PARIS standard [for mbeep]
        } else if (((strcmp(argv[index], "--paris-wpm") == 0) ||
                    (strcmp(argv[index], "-w") == 0)) && index + 1 < argc) {
            mfp.paris_wpm = atof(argv[++index]);
            if (mfp.paris_wpm < 5.0 || mfp.paris_wpm > 60.0) error = MF_INVALID_WPM;
            
        //  --codex-wpm  words per minute, CODEX standard [for mbeep]
        } else if (strcmp(argv[index], "--codex-wpm") == 0 && index + 1 < argc) {
            mfp.codex_wpm = atof(argv[++index]);
            if (mfp.codex_wpm < 5.0 || mfp.codex_wpm > 60.0) error = MF_INVALID_WPM;
            
        //  -x  --farnsworth character speed [for mbeep]
        } else if (((strcmp(argv[index], "--farnsworth") == 0) ||
                    (strcmp(argv[index], "-x") == 0)) && index + 1 < argc) {
            mfp.farnsworth_wpm = atof(argv[++index]);
            if (mfp.farnsworth_wpm < 5.0 || mfp.farnsworth_wpm > 60.0) error = MF_INVALID_WPM;
            
            
        //  -i  input file for text to be converted
        } else if (strcmp(argv[index], "-i") == 0 && index + 1 < argc) {
            if (mfp.in_file != NULL) {
                error = MF_FILE_ALREADY_OPEN_ERROR;
                
            } else {
                mfp.in_file_name     = argv[++index];
                mfp.in_file = fopen(mfp.in_file_name    , "r");
            }
            
            if (mfp.in_file == NULL) {
                error = MF_INPUT_FILE_OPEN_ERROR;
            }
        
        //  -u  input URL for text-only news site to be converted
        } else if (strcmp(argv[index], "-u") == 0 && index + 1 < argc) {
            mfp.url = argv[++index];

        //  -a  use text after string
        } else if (strcmp(argv[index], "-a") == 0 && index + 1 < argc) {
            mfp.text_after = argv[++index];

        //  -b  use text before string
        } else if (strcmp(argv[index], "-b") == 0 && index + 1 < argc) {
            mfp.text_before = argv[++index];

        //  -L (follow links)
        } else if (strcmp(argv[index], "-L") == 0) {
            mfp.follow_links = true;

        //  -A  use text after string
        } else if (strcmp(argv[index], "-A") == 0 && index + 1 < argc) {
            mfp.linked_text_after = argv[++index];

        //  -B  use text before string
        } else if (strcmp(argv[index], "-B") == 0 && index + 1 < argc) {
            mfp.linked_text_before = argv[++index];

        //  -p  (save and use position)
        } else if (strcmp(argv[index], "-p") == 0) {
            if (mfp.state_path == NULL) {
                error = MF_NO_STATE_PATH;

            } else {
                mfp.save_and_use_position = true;
            }

        //  -o  output file for converted text
        } else if (strcmp(argv[index], "-o") == 0 && index + 1 < argc && mfp.out_file == NULL) {
            mfp.out_file = fopen(argv[++index], "w");
            
            if (mfp.out_file == NULL) {
                error = MF_OUTPUT_FILE_OPEN_ERROR;
            }

        //  -s  save state for re-use
        } else if (strcmp(argv[index], "-s") == 0 && index + 1 < argc) {
            state_label = argv[++index];

        //  -r  read saved state
        } else if (strcmp(argv[index], "-r") == 0 && index + 1 < argc) {
            error = read_state(argv[++index], &mfp, &string_storage);

        //  -v --version    print version of mbeep
        } else if ((strcmp(argv[index], "--version") == 0) ||
                   (strcmp(argv[index], "-v") == 0)) {
            version();
            error = MF_EXIT;
            
        //  -h --help       print help for mbeep
        } else if ((strcmp(argv[index], "--help") == 0) ||
                   (strcmp(argv[index], "-h") == 0)) {
            usage();
            error = MF_EXIT;
            
        //  --man-page      print source for man page
        } else if (strcmp(argv[index], "--man-page") == 0) {
            man_page_source();
            error = MF_EXIT;
            
        //  --licence       print copyright and license
        } else if (strcmp(argv[index], "--license") == 0) {
            license();
            error = MF_EXIT;
#if DEBUG
        //  --test      run tests
        } else if (strcmp(argv[index], "--test") == 0) {
            vector_tests();
            morsefeed_tests();
            error = MF_EXIT;
#endif

        } else {
            error = MF_INVALID_OPTION;
        }
    }

    if (error == MF_NO_ERROR && state_label != NULL) {
        error = save_state(state_label, &mfp);
    }

    if (error == MF_NO_ERROR) {
        error = process_and_send(mfp);
    }

    if (mfp.in_file != NULL) {
        if (mfp.in_file != stdin) fclose(mfp.in_file);
        mfp.in_file = NULL;
    }
    
    if (mfp.out_file != NULL) {
        if (mfp.out_file == stdout) {
            fflush(stdout);
        } else {
            fclose(mfp.out_file);
        }
        
        mfp.out_file = NULL;
    }
    
    switch (error) {
        case MF_NO_ERROR:                                                       break;
        case MF_EXIT:                                                           break;
        case MF_PIPE_ERROR:         printf("Error: MF_PIPE_ERROR\n");           break;
        case MF_FORK_ERROR:         printf("Error: MF_FORK_ERROR\n");           break;
        case MF_INVALID_VALUE:      printf("Error: MF_INVALID_VALUE\n");        break;
        case MF_OUT_OF_MEMORY:      printf("Error: MF_OUT_OF_MEMORY\n");        break;
        case MF_INVALID_FREQUENCY:  printf("Error: MF_INVALID_FREQUENCY\n");    break;
        case MF_INVALID_WPM:        printf("Error: MF_INVALID_WPM\n");          break;
        case MF_INVALID_OPTION:     printf("Error: MF_INVALID_OPTION\n");       break;
        case MF_FILE_READ_ERROR:            printf("Error: MF_FILE_READ_ERROR\n");          break;
        case MF_URL_READ_ERROR:             printf("Error: MF_URL_READ_ERROR\n");           break;
        case MF_INPUT_FILE_OPEN_ERROR:      printf("Error: MF_INPUT_FILE_OPEN_ERROR\n");    break;
        case MF_OUTPUT_FILE_OPEN_ERROR:     printf("Error: MF_OUTPUT_FILE_OPEN_ERROR\n");   break;
        case MF_POSITION_FILE_OPEN_ERROR:   printf("Error: MF_POSITION_FILE_OPEN_ERROR\n"); break;
        case MF_FILE_ALREADY_OPEN_ERROR:    printf("Error: MF_FILE_ALREADY_OPEN_ERROR\n");  break;
        case MF_FILE_WRITE_ERROR:           printf("Error: MF_FILE_WRITE_ERROR\n");         break;
        case MF_PROGRAM_ERR:                printf("Error: MF_PROGRAM_ERR\n");              break;
        case MF_NO_STATE_PATH:              printf("Error: MF_NO_STATE_PATH\n");            break;
        case MF_UNKNOWN_SAVED_STATE:        printf("Error: MF_UNKNOWN_SAVED_STATE\n");      break;

        case MF_UNKNOWN:
        default:
            printf("Error: unknown %d\n", error);
            break;
    };
    
    string_vector_free(&string_storage);
    
    return 0;
}

