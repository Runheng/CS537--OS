// Copyright 2019 Runheng Lei
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef struct {
    pid_t pid;
    int background;
    int stop;
    int redir;
    char **command;
} job;

int job_count = 0;  // keep track of jid as array index
int main(int argc, char **argv) {
    job *alljobs = malloc(1000 * sizeof(job));  // store all jobs

    if (argc != 1 && argc != 2) {
        char* err = "Usage: mysh [batchFile]\n";
        write(STDERR_FILENO, err, strlen(err));
        exit(1);  // invalid command-line arg
    }

    // batch mode
    FILE *fp;
    if (argc == 2) {
        char *fileName = argv[1];
        fp = fopen(fileName, "r");
        if (fp == NULL) {
            char* buffer = malloc(512);
            strcpy(buffer, "Error: Cannot open file ");
            strcat(buffer, fileName);
            strcat(buffer, "\n");
            write(STDERR_FILENO, buffer, strlen(buffer));
            free(buffer);
            exit(1);
        }
    }

    // interactive mode
    while (1) {
        job new_job;  // create a new job
        char *bf = malloc(512);
        size_t size = 514;
        if (argc == 1) {
            write(1, "mysh> ", 6);  // prompt user
            if (getline(&bf, &size, stdin) == -1) {
                exit(0);
            }  // read user input
        } else if (argc == 2) {
            if (getline(&bf, &size, fp) == -1) return 0;  // read file line
            write(1, bf, strlen(bf));  // print command line from file
        }
        if (strlen(bf) > 514) { write(STDERR_FILENO,
                "Command-line too long\n", 22); }

        // split the command
        char **token = malloc(sizeof(char*)*512);
        char **token_2 = malloc(sizeof(char*)*512);
        char *bf2 =  malloc(512);
        strcpy(bf2, bf);

        // remove extra spaces
        char *line = strtok(bf, " \n\t");
        int i = 0;
        while (line != NULL) {
            token[i] = line;
            i++;
            line = strtok(NULL, " \n\t");
        }

        // check if redirection occurs
        line = strtok(bf2, " >\n\t");
        int j = 0;
        while (line != NULL) {
            token_2[j] = line;
            j++;
            line = strtok(NULL, " >\n\t");
        }
        if (i == j) {
            new_job.redir = 0;
        } else if ((i - j) == 1 && strcmp(token[i - 2], ">") == 0) {
            new_job.redir = 1;
            token[i - 2] = NULL;
            token[i - 1] = NULL;
            i -= 2;
        } else {
            char* err = "Invalid redirection command\n";
            write(STDERR_FILENO, err, strlen(err));
            continue;
        }
        // an empty command line
        if (i == 0) { continue; }
        // build-in shell command : exit
        if (strcmp(token[0], "exit") == 0 && (i == 1 ||
            (i == 2 && strcmp(token[1], "&") == 0))) {
            free(token);
            free(token_2);
            free(bf);
            free(bf2);
            free(alljobs);
            token = token_2 = NULL;
            bf = bf2 = NULL;
            alljobs = NULL;
            return 0;
        }
        // build-in shell command : jobs
        if (strcmp(token[0], "jobs") == 0 && (i == 1 ||
            (i == 2 && strcmp(token[2], "&") == 0))) {
            for (int k = 0; k < job_count; k++) {
                // only print jobs that are background and is still running
                if (alljobs[k].background == 1 && alljobs[k].stop == 0) {
                    int status;
                    pid_t ret_val = waitpid(alljobs[k].pid, &status, WNOHANG);

                    // if int return is pid, status changed, process terminated
                    if (ret_val == alljobs[k].pid || ret_val == -1) {
                        alljobs[k].stop = 1;
                    }
                    // if not terminated
                    if (alljobs[k].stop == 0) {
                        char *temp = malloc(512);
                        temp[0] = '0' + k;
                        strcat(temp, " :");
                        int r = 0;
                        while (alljobs[k].command[r] != NULL) {
                            strcat(temp, " ");
                            strcat(temp, alljobs[k].command[r]);
                            r++;
                        }
                        strcat(temp, "\n");
                        write(1, temp, strlen(temp));
                        free(temp);
                        temp = NULL;
                    }
                }
            }
            continue;
        }
        // build-in shell command : wait
        if (strcmp(token[0], "wait") == 0 && (i == 2 ||
            (i == 3 && strcmp(token[2], "&") == 0))) {
            // if jid not assigned or it's a foreground job, invalid
            if (atoi(token[1]) > job_count ||
                       alljobs[atoi(token[1])].background == 0) {
                char *err = malloc(50);
                strcpy(err, "Invalid JID ");
                strcat(err, token[1]);
                strcat(err, "\n");
                write(STDERR_FILENO, err, strlen(err));
                free(err);
                err = NULL;
                continue;
            }
            int jid = atoi(token[1]);
            if (alljobs[jid].stop == 0) {
                int status;
                waitpid(alljobs[jid].pid, &status, 0);
                alljobs[jid].stop = 1;
            }
            char *msg = malloc(50);
            strcpy(msg, "JID ");
            strcat(msg, token[1]);
            strcat(msg, " terminated\n");
            write(1, msg, strlen(msg));
            free(msg);
            msg = NULL;
            continue;
        }

        // check if this job works background
        if (strcmp(token[i - 1], "&") == 0) {
            new_job.background = 1;
            token[i - 1] = NULL;
        } else { new_job.background = 0; }

        // store this job to job array
        new_job.command = token;
        new_job.stop = 0;
        pid_t pid = fork();
        new_job.pid = pid;
        alljobs[job_count] = new_job;

        // start executing the process
        if (pid < 0) {
            write(STDERR_FILENO, "fork child-process failed\n", 36);
            exit(1);
        } else if (pid == 0) {  // if current process is a child process
            // create or open the file if redirection
            if (new_job.redir == 1) {
                int fd = open(token_2[j - 1],
                              O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
                if (fd == -1) {
                    char *err = "error in create or open output file\n";
                    write(STDERR_FILENO, err, strlen(err));
                    continue;
                }
                dup2(fd, STDOUT_FILENO);
            }
            int exe_ret = execvp(new_job.command[0], new_job.command);
            if (exe_ret == -1) {
                char *err = new_job.command[0];
                strcat(err, ": Command not found\n");
                write(STDERR_FILENO, err, strlen(err));
                new_job.stop = 1;
                exit(1);
            }
        } else {  // if current process is a parent process
            // if foreground, wait until job complete
            if (new_job.background == 0) {
                int status;
                while (wait(&status) != pid) {}
                alljobs[job_count].stop = 1;
            }
            // if background, start the job and immediately return
        }
        job_count++;
        token = NULL;  // refresh token
        token_2 = NULL;
    }
}
