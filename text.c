//
// text.c
// morsefeed
//
// Copyright (C) 2018-2021 Michael Budiansky. All rights reserved.
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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "text.h"

void version(void)
{
    printf("morsefeed 0.5.1\n");
}

void usage(void)
{
    printf("Tool for converting and processing text to be used for Morse code practice.\n"
           "\n"
           "Usage:\n"
           "  morsefeed ( (-i <input_path) | (-u <URL> [-L [-A <string] [-B string]]) )\n"
           "            [-a <string] [-b string]\n"
           "            ( ([-o <output_file>] [-c <words_per_row>] [-n <number_of_words>]) |\n"
           "              (-m [-p] [-f <freq>] ([-w <wpm>] | [--codex-wpm <wpm>]) [-x <speed>])\n"
           "            )\n"
           "            [-s <label>]\n"
           "  morsefeed -r <label>\n"
           "  morsefeed -h | --help\n"
           "  morsefeed -v | --version\n"
           "  morsefeed --license\n"
           "  morsefeed --man-page\n"
           "\n"
           "Options:\n"
           "  -i <file_path>         Input file for text to be converted\n"
           "  -u <URL>               Input URL for text to be converted\n"
           "  -a <string>            Use input text after string\n"
           "  -b <string>            Use input text before string\n"
           "  -L                     Follow links on web page at URL to get text to be converted\n"
           "  -A <string>            Use linked input text after string\n"
           "  -B <string>            Use linked input text before string\n"
           "  -o <file_path>         Output file for converted text\n"
           "  -m                     Send converted text to mbeep\n"
           "  -p                     Remember position in input stream and use when resuming\n"
           "  -s <label>             Save options for reuse with named label\n"
           "  -r <label>             Load options previously saved with named label\n"
           "  -c <words_per_row>     Number of words per row [default: 5]\n"
           "  -n <number_of_words>   Number of words to print\n"
           "\n"
           "  -h --help     Show this screen.\n"
           "  --version     Show version.\n"
           "  --license     Show software copyright and license\n"
           "  --man_page    Show source for man page\n"
           "\n"
           "Options passed to mbeep:\n"
           "  -f <freq>         Frequency of tone in Hz [default: 750 for code, else 440]\n"
           "  -w <wpm>          Morse code speed in PARIS words per minute [default: 20]\n"
           "  --codex-wpm <wpm> Morse code speed in CODEX words per minute [default: 16 2/3]\n"
           "  -x <speed>        Character speed for Farnsworth Morse code timing\n"
           "  --wss <speed>     Word speed with extra space between words\n"
           "  --fcc             Print effective FCC code test speed after sending.\n"
           "  --wav             Output file name for .wav file.\n"
           "\n");

}

void license(void)
{
    printf("Copyright (C) 2018-2021 Michael Budiansky. All rights reserved.\n"
           "\n"
           "Redistribution and use in source and binary forms, with or without modification, are permitted\n"
           "provided that the following conditions are met:\n"
           "\n"
           "Redistributions of source code must retain the above copyright notice, this list of conditions\n"
           "and the following disclaimer.\n"
           "\n"
           "Redistributions in binary form must reproduce the above copyright notice, this list of conditions\n"
           "and the following disclaimer in the documentation and/or other materials provided with the\n"
           "distribution.\n"
           "\n"
           "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\" AND ANY EXPRESS OR\n"
           "IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND\n"
           "FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR\n"
           "CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n"
           "DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n"
           "DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,\n"
           "WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY\n"
           "WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n");
}

void man_page_source(void)
{
    printf(".TH morsefeed 1\n"
           "\n"
           ".SH NAME\n"
           "morsefeed \\- tool for converting and processing text to be used for Morse code practice\n"
           "\n"
           ".SH SYNOPSIS\n"
           ".nf\n"
           "\\fBmorsefeed\\fR ( (\\fB\\-i\\fR \\fIFILE\\fR) | (\\fB\\-u\\fR \\fIURL\\fR [\\fB\\-L\\fR [\\fB\\-A\\fR \\fISTRING\\fR] [\\fB\\-B\\fR \\fISTRING\\fR]]) )\n"
           "    [\\fB\\-a\\fR \\fISTRING\\fR] [\\fB\\-b\\fR \\fISTRING\\fR]\n"
           "    [ ([\\fB\\-o\\fR \\fIFILE\\fR] [\\fB\\-c\\fR \\fIWORDS_PER_ROW\\fR] [\\fB\\-n\\fR \\fIWORD_COUNT\\fR]) | \n"
           "      (\\fB\\-m\\fR [\\fB\\-p\\fR] [\\fB\\-f\\fR \\fIFREQ\\fR] ([\\fB\\-w\\fR \\fIWPM\\fR] | [\\fB\\--codex-wpm\\fR \\fIWPM\\fR]) [\\fB\\-x\\fR \\fICHAR_SPEED\\fR] [\\fB\\-\\-wss\\fR \\fIWORD_SPEED\\fR] [\\fB\\-\\-fcc\\fR] [\\fB\\-\\-wav\\fR \\fIWAV_FILE_NAME\\fR]) ]\n"
           "    [\\fB\\-s\\fR \\fILABEL\\fR]\n"
           "\\fBmorsefeed\\fR \\fB\\-r\\fR \\fILABEL\\fR\n"
           "\\fBmorsefeed\\fR \\fB\\-h\\fR | \\fB\\-v\\fR | \\fB\\-\\-license\\fR | \\fB\\-\\-man\\-page\\fR\n"
           ".fi\n"
           "\n"
           ".SH DESCRIPTION\n"
           "Command\\-line tool that converts and processes text to be used for Morse code practice.\n"
           "Text sources can be disk files or web pages.\n"
           "Most punctuation and special characters are converted, removed, or spelled\\-out."
           "\n"
           "Text can be output to disk file, standard output, or directly to mbeep tool.\n"
           "If sent to mbeep, audio can be paused or resumed by typing space bar, or quit by typing the letter q.\n"
           "There is an option to save the file position when quitting, and resume transmission from that point when starting again."
           "\n"
           "There are options to filter out text at the beginning or end of a file or web page. "
           "HTML tags are filtered out of web page text. There is an option to follow links from one web page, and use the text on "
           "the linked pages for practice. This is useful for text\\-only news websites where the main page just contains links to articles."
           "\n"
           "A lengthy series of options can be saved as shortcut by using the \\fB\\-s\\fR option and recalled by using the \\fB\\-r\\fR option."
           "\n"
           "\n"

           ".SH OPTIONS\n"
           
           "\n"
           ".TP\n"
           ".BR \\-i \" \" \\fIFILE\\fR\n"
           "Input file for text to be converted.\n"
           
           "\n"
           ".TP\n"
           ".BR \\-u \" \" \\fIURL\\fR\n"
           "Input URL for text to be converted.\n"
           
           "\n"
           ".TP\n"
           ".BR \\-a \" \" \\fISTRING\\fR\n"
           "Use input text after string.\n"
           
           "\n"
           ".TP\n"
           ".BR \\-b \" \" \\fISTRING\\fR\n"
           "Use input text before string.\n"
           
           "\n"
           ".TP\n"
           ".BR \\-L\n"
           "Follow links on web page at URL to get text to be converted.\n"
           
           "\n"
           ".TP\n"
           ".BR \\-A \" \" \\fISTRING\\fR\n"
           "Use linked input text after string.\n"
           
           "\n"
           ".TP\n"
           ".BR \\-B \" \" \\fISTRING\\fR\n"
           "Use linked input text before string.\n"
           
           "\n"
           ".TP\n"
           ".BR \\-o \" \" \\fIFILE\\fR\n"
           "Output file for converted text.\n"
           
           "\n"
           ".TP\n"
           ".BR \\-m\n"
           "Send converted text to mbeep.\n"
           
           "\n"
           ".TP\n"
           ".BR \\-p\n"
           "Remember position in input stream and use when resuming.\n"

           "\n"
           ".TP\n"
           ".BR \\-s \" \" \\fILABEL\\fR\n"
           "Save options for reuse with named label.\n"
           
           "\n"
           ".TP\n"
           ".BR \\-r \" \" \\fILABEL\\fR\n"
           "Load options previously saved with named label.\n"

           ".TP\n"
           ".BR \\-c \" \" \\fIWORDS_PER_ROW\\fR\n"
           "Number of words per row. Default is 1 with \\-m option, otherwise 5.\n"
           "\n"
           ".TP\n"
           ".BR \\-n \" \" \\fINUMBER_OF_WORDS\\fR\n"
           "Total number of words to print. Default is all.\n"
           "\n"

           "\n"
           ".TP\n"
           ".BR \\-f \" \" \\fIFREQ\\fR\n"
           "(Passed to mbeep.) Frequency of tone in Hz. Default is 750.\n"
           "\n"
           ".TP\n"
           ".BR \\-w \", \" \\-\\-paris\\-wpm \" \" \\fIWPM\\fR\n"
           "(Passed to mbeep.) Morse code speed in words per minute (PARIS standard). Default is 20.\n"
           "\n"
           ".TP\n"
           ".BR \\-\\-codex\\-wpm \" \" \\fIWPM\\fR\n"
           "(Passed to mbeep.) Morse code speed in words per minute (CODEX standard). Default is 16 2/3.\n"
           "\n"
           ".TP\n"
           ".BR \\-x \", \" \\-\\-farnsworth \" \" \\fICHAR_SPEED\\fR\n"
           "(Passed to mbeep.) Character speed for Farnsworth Morse code timing. Default is same as words per minute.\n"
           "\n"
           ".TP\n"
           ".BR \\-\\-wss \" \" \\fIWORD_SPEED\\fR\n"
           "(Passed to mbeep.) Word speed with extra space between words. Default is same as words per minute.\n"
           "\n"
           ".TP\n"
           ".BR \\-\\-fcc\n"
           "(Passed to mbeep.) Print effective FCC code test speed after sending.\n"
           "\n"
           ".TP\n"
           ".BR \\-\\-wav\n"
           "(Passed to mbeep.) Output file name for .wav file.\n"
           "\n"
           ".TP\n"
           ".BR \\-h \", \" \\-\\-help\\fR\n"
           "Show help message.\n"
           "\n"
           ".TP\n"
           ".BR \\-v \", \" \\-\\-version\n"
           "Show version.\n"
           "\n"
           ".TP\n"
           ".BR \\-\\-license\n"
           "Show software copyright and license.\n"
           "\n"
           ".TP\n"
           ".BR \\-\\-man\\-page\n"
           "Show source for this man page\n"
           "\n"
           ".SH EXAMPLES\n"
           "Read text from input file and save formatted results to output file:\n"
           ".PP\n"
           ".nf\n"
           ".RS\n"
           "\\fBmorsefeed -i foo.txt -o bar.txt\\fR\n"
           ".RE\n"
           ".fi\n"
           ".PP\n"
           "\n"

           "Read text from input file and send formatted results to mbeep; stop and resume by "
           "hitting space bar; quit by typing letter 'q'; when quitting, remember position in file, "
           "and when starting continue from previously saved position:"
           "\n"
           ".PP\n"
           ".nf\n"
           ".RS\n"
           "\\fBmorsefeed -i foo.txt -p -m\\fR\n"
           ".RE\n"
           ".fi\n"
           ".PP\n"
           "\n"

           "Read text from web page, send formatted results to mbeep; stop and resume by "
           "hitting space bar; quit by typing letter 'q':\n"
           ".PP\n"
           ".nf\n"
           ".RS\n"
           "\\fBmorsefeed -u https://7402.org/files/tale2c.txt -m\\fR\n"
           ".RE\n"
           ".fi\n"
           ".PP\n"
           "\n"

           "Read text between specified strings on web page, send formatted results to "
           "mbeep at 15 wpm; stop and resume by hitting space bar; quit by typing letter 'q':\n"
           ".PP\n"
           ".nf\n"
           ".RS\n"
           "\\fBmorsefeed -u https://7402.org/files/sample.txt -a \"after this\" -b \"before this\" -m -w 15\\fR\n"
           ".RE\n"
           ".fi\n"
           ".PP\n"
           "\n"

           "Send CNN text\\-only articles to mbeep; stop and resume by hitting space bar; quit by "
           "typing letter 'q'; type 'n' or 'N' to skip ahead to next article:\n"
           ".PP\n"
           ".nf\n"
           ".RS\n"
           "\\fBmorsefeed -u https://lite.cnn.io/en -a \"Main Stories</strong>\" -b \"<hr/>\" -L -A \"Listen</a></div><hr/>\" -B \"<hr/>\" -m\\fR\n"
           ".RE\n"
           ".fi\n"
           ".PP\n"
           "\n"

           "Send NPR text\\-only articles to mbeep, save shortcut for options under label \"npr\":\n"
           ".PP\n"
           ".nf\n"
           ".RS\n"
           "\\fBmorsefeed -u http://text.npr.org/ -a \"Top News Stories\" -b \"<p>Topics</p>\" -L -A \"Home</a>\" -B \"About NPR</a>\" -m -s npr\\fR\n"
           ".RE\n"
           ".fi\n"
           ".PP\n"
           "\n"

           "Send NPR text\\-only articles to mbeep, using previously\\-saved shortcut:\n"
           ".PP\n"
           ".nf\n"
           ".RS\n"
           "\\fBmorsefeed -r npr\\fR\n"
           ".RE\n"
           ".fi\n"
           ".PP\n"
           "\n"

           ".SH SEE ALSO\n"
           ".BR mbeep (1)\n"
           "\n"
           ".SH NOTES\n"
           "Punctuation marks that are copied without being spelled\\-out are: period, comma, "
           "question mark, fraction bar, and equals sign.\n"
           "Apostropes are removed, therefore contractions will appear as one word.\n"
           "\n"
           "Web page unformatted list items (<ul>) are separated by the \\fBAA\\fR prosign. Text "
           "blocks from linked web pages (\\fB-L\\fR option) are separated by the \\fBBT\\fR prosign.\n"
           "\n"
           ".SH AUTHOR\n"
           "Michael Budiansky \\fIhttps://www.7402.org/email\\fR\n"
           "\n");
}


