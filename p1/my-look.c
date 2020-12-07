// Copyright 2019 Runheng Lei
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

int main(int argc, char *argv[]) {
        // check if num of command-line arg is valid
        if (argc < 2 || argc > 3) {
                printf("my-look: invalid number of arguments\n");
                exit(1);  // print err-msg and exit
        }

        // store the string to look for in a var
        char* str = *(argv+1);
        int length = strlen(str);

        // check if specific file is given, and store the file name
        char* fileName;
        if (argc == 2) {
                fileName = "/usr/share/dict/words";
        }
        if (argc == 3) {
                fileName = *(argv+2);
        }

        // open the file an check if it opened successfully
        FILE *fp = fopen(fileName, "r");
        if (fp == NULL) {
                printf("my-look: cannot open file\n");
                exit(1);
        }

        // create a buffer to store result strings after searching the file
        int size = 1000;  // could be changed if a larger buffer is needed
        char buffer[1000];

        // go through the file and compare strings
        while (fgets(buffer, size, fp) != NULL) {
                // convert buffer data and compare string both into lowercase
                char temp_buffer[1000];
                for (int i = 0; buffer[i]; i++) {
                        temp_buffer[i] = tolower(buffer[i]);
                }
                for (int j = 0; str[j]; j++) {
                        str[j] = tolower(str[j]);
                }

                // compare and print
                if (strncmp(str, temp_buffer, length) == 0) {
                        printf("%s", buffer);
                }
        }

        // close the file
        if (fclose(fp) != 0) {
                printf("my-look: cannot close file\n");
                exit(1);
        }

        // non-error, successfully exit the program
        return 0;
}
