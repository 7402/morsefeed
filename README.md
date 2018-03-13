## morsefeed

---

### Description

Command-line tool that converts and processes text to be used for Morse code
practice. Text sources can be disk files or web pages. Text can be output to
disk file, standard output, or directly to the mbeep tool. 

Most punctuation and special characters are converted, removed, or spelled-out.
There are options to filter out text at the beginning or end of a file or web
page. HTML tags are filtered out of web page text. 

If sent to mbeep, audio can be paused or resumed by typing space bar, or quit by
typing the letter q. There is an option to save the file position when quitting,
and resume transmission from that point when starting again.

There is an option to follow links from one web page, and use the text on the
linked pages for practice. This is useful for text-only news websites where the
main page just contains links to articles.

A lengthy series of options can be saved as a shortcut by using the -s option and
recalled by using the -r option.

Runs on Mac or Linux.

### Usage

Use **-i** option to specify input file for text to be converted.

Use **-u** option to specify URL for text to be converted.

Use **-o** option to specify output file for text to be converted.

Use **-m** option to send converted text to mbeep to play as morse code.

For more information, see man page.

### Build and install

* macOS

```
cd path_to_directory
make
make install
```

* Linux

```
sudo yum install libcurl-devel
cd path_to_directory
make
sudo make install
```

### Examples

Read text from input file and save formatted results to output file:

```
morsefeed -i foo.txt -o bar.txt
```

Read text from input file and send formatted results to mbeep; stop and resume
by hitting space bar; quit by typing letter 'q'; when quitting, remember
position in file, and when starting continue from previously saved position:

```
morsefeed -i foo.txt -p -m
```

Read text from web page, send formatted results to mbeep; stop and resume by hitting space bar; quit by typing
letter 'q':
```
morsefeed -u https://7402.org/files/tale2c.txt -m
```

Read text between specified strings on web page, send formatted results to mbeep at 15 wpm; stop and resume by
hitting space bar; quit by typing letter 'q':
```
morsefeed -u https://7402.org/files/sample.txt -a "after this" -b "before this" -m -w 15
```

Send CNN text-only articles to mbeep; stop and resume by hitting space bar; quit by "typing letter 'q'; type 'n' or 'N'
to skip ahead to next article:
```
morsefeed -u https://lite.cnn.io/en -a "Main Stories</strong>" -b "<hr/>" -L -A "Listen</a></div><hr/>" -B "<hr/>" -m
```

Send NPR text-only articles to mbeep, save shortcut for options under label "npr":

```
morsefeed -u http://text.npr.org/ -a "Top News Stories" -b "<p>Topics</p>" -L -A "Home</a>" -B "About NPR</a>" -m -s npr
```

Send NPR text-only articles to mbeep, using previously-saved shortcut:

```
morsefeed -r npr
```

### License

BSD

