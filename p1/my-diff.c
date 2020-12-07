// Copyright 2019 Runheng Lei
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

int main(int argc, char** argv) {
        // check if command-line arg is valid
        if (argc != 3) {
                printf("my-diff: invalid number of arguments\n");
                exit(1);
        }

        // process command-line args
        char* fileName1 = argv[1];
        char* fileName2 = argv[2];

        // open files and check if they open successfully
        FILE* fp1 = fopen(fileName1, "r");
        FILE* fp2 = fopen(fileName2, "r");
        if (fp1 == NULL || fp2 == NULL) {
                printf("my-diff: cannot open file\n");
                exit(1);
        }

        // get ready for the search
        char* line1 = NULL;
        char* line2 = NULL;
        size_t len = 0;
        ssize_t end1;
        ssize_t end2;
        int line_num = 1;
        int prev = 0;

        // get each line of the file and compare
        while ((end1 = getline(&line1, &len, fp1)) != -1 &&
               (end2 = getline(&line2, &len, fp2)) != -1) {
                if (strncmp(line1, line2, len) != 0) {
                        if (prev == 0) {
                                printf("%d\n", line_num);
                        }
                        printf("< %s> %s", line1, line2);
                        prev = 1;
                } else {
                        prev = 0;
                }
                line_num++;
        }

        // if both files reach the end, no further actions required
        // if file1 ends, but file2 is not ended, print file2
        if (end1 == -1 && (end2 = getline(&line2, &len, fp2)) != -1) {
                if (prev == 0) {
                        printf("%d\n", line_num);
                }
                printf("> %s", line2);
                while ((end2 = getline(&line2, &len, fp2)) != -1) {
                        printf("> %s", line2);
                        line_num++;
                }
        }
        // if file2 ends, but file1 does not, print file1
        if (end2 == -1 && end1 != -1) {
                if (prev == 0) {
                        printf("%d\n", line_num);
                }
                printf("< %s", line1);
                while ((end1 = getline(&line1, &len, fp1)) != -1) {
                        printf("< %s", line1);
                        line_num++;
                }
        }
        // free allocate space
        free(line1);
        free(line2);

        // close both files
        if (fclose(fp1) != 0 || fclose(fp2) !=0) {
                printf("my-diff: cannot close file\n");
                exit(1);
        }
        return 0;
}
