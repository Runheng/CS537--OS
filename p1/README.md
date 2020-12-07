This is the explanation on the implementations of my-look.c, across.c, my-diff.c

1. my-look.c
    General purpose: This progam can be used to search through a dictionary file and print out the lines which contain a word that begins with given string (case insensitive)
    Implementation: After checking the validity of command-line args, use fopen() function to open a given file (if not given, use default one). If sucessful, use fgets() to read in each line. Then use strncmp() to compare given string and the string we get with required length, print out the string if strncmp() returns 0. Finally use fclose() to close the file and then successfully return from the file.

2. across.c 
    General purpose: This program is used to search through a dictionary file and print out words if the word is of given length and contains same subtring as given string input and that the substring starts at specified location. It could be used to fill out words in a cross-word puzzle.
    Implementation: After checking the validity of command-line args, use fopen() to open given file (if not sepcified, use default one). Then use a while loop to go through the file and use fgets() to store words from each line into a buffer. Then check if the word is purly lowercase letters, compare its length with the requirment. If both tests pass, use strncpy to get substring that starts from required location, and use strncmp() to compare this substring and string from input, if returns 0, print out buffer. Finally use fclose() to close the file and exit from program.

3. my-diff.c
    General purpose: This program is used to compare two files, and print out the lines that are different in two files.
    Implementation: First check the validity of command-line args, and then open two given files. Then use getline() to go through each line in file separately and save the line to two buffers. Then compare two buffer using strncmp(), if they are not the same, print out line num if previous line are the same, followed by the content in file1 and then the content in file2. If one of the file ends earlier, print out all lines left in the other file. Then free all allocated spaces, close the files, and exit from the program.
