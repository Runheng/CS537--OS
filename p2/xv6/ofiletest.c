// copyright 2019 Runheng Lei
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    printf(1, "wrong number of argument.\n");
    exit();
  }
  // store number of files to open
  int n = atoi(argv[1]);
  if (n < argc - 2) {
    printf(1, "invalid number of files to open.\n");
    exit();
  }

  // store the number of the file that will be closed
  int file_to_close[argc - 2];
  for (int i = 0; i < argc - 2; i++) {
    if ((argv[i + 2])[0] == '-') {
      printf(1, "can not close a file with negative number.\n");
      exit();
    }
    int temp = atoi(argv[i + 2]);
    if (temp >= n) {
      printf(1, "invalid file number to close.\n");
      exit();
    }
    file_to_close[i] = temp;
  }

  // create a buffer to store file name
  char buffer[8];
  buffer[0] = 'o';
  buffer[1] = 'f';
  buffer[2] = 'i';
  buffer[3] = 'l';
  buffer[4] = 'e';
  buffer[7] = '\0';
  
  // open files
  for (int j = 0; j < n; j++) {
    // get the file name
    if (j < 10) {
      buffer[5] = 48 + j;
      buffer[6] = '\0';
    }
    else {
      int digit1 = j / 10;
      int digit0 = j % 10;
      buffer[5] = 48 + digit1;
      buffer[6] = 48 + digit0;
    }
    
    // open the file
    int fd = open(buffer, O_CREATE | O_RDWR);
    if (fd < 0) {
      printf(1, "can NOT open or create %s\n", buffer);
      exit();
    }
  }

  // close required files
  for (int k = 0; k < argc - 2; k++) {
    int s = close(file_to_close[k] + 3);
    if (s < 0) {
      printf(1, "can NOT close file");
      exit();
    }
  }

  // system calls
  int cnt = getofilecnt(getpid());
  int next = getofilenext(getpid());
  printf(1, "%d ", cnt);
  printf(1, "%d\n", next);

  exit();
}
