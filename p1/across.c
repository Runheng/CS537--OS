// copyright 2019 Runheng Lei
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

int main(int argc, char**argv) {
        // check if command-line arg is valid
        if (argc != 4 && argc != 5) {
                printf("across: invalid number of arguments\n");
                exit(1);
        }

        // process all command-line args
        char* str = argv[1];
        int strt_pos = atoi(argv[2]);
        int word_length = atoi(argv[3]);

        // check if start position and required length are reasonable
        if ((strt_pos + strlen(str)) > word_length) {
                printf("across: invalid position\n");
                exit(1);
        }

        // store file name/ filepath
        char* file_name;
        if (argc == 4) {
                file_name = "/usr/share/dict/words";
        }
        if (argc == 5) {
                file_name = *(argv+4);
        }

        // open the file and check if it opens successfully
        FILE *fp = fopen(file_name, "r");
        if (fp == NULL) {
                printf("across: cannot open file\n");
                exit(1);
        }

        // create a buffer to store strings from each line of the file
        int size = 1000;
        char buffer[1000];

        // go through the file and store data into buffer
        while (fgets(buffer, size, fp) != NULL) {
                // if required word length is matched
                if (strlen(buffer) != word_length + 1) {
                        continue;
                }

                // if word is lowercased
                int test = 0;
                for (int i = 0; buffer[i]; i++) {
                        if ((buffer[i]<'a' || buffer[i]>'z') &&
                           ((int)buffer[i] != 10)) {
                                test = 1;
                                break;
                        }
                }
                if (test == 1) {continue;}

                // get the subtring of the word in file
                char* sub_str = malloc(sizeof(char) * strlen(str));
                strncpy(sub_str, (char*)(buffer+strt_pos), strlen(str));
                // compare subtring with given string
                if (strncmp(sub_str, str, strlen(str)) == 0) {
                        printf("%s", buffer);
                }
        }

        // close the file
        if (fclose(fp) != 0) {
                printf("across: cannot close file\n");
                exit(1);
        }
        // return 0 if non-error
        return 0;
}
