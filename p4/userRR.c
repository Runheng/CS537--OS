#include "types.h"
#include "pstat.h"
#include "param.h"
#include "syscall.h"
#include "user.h"
int
main (int argc, char** argv) 
{
  if (argc != 5) {
    printf(1,"Usage: USRR argument is not matching");
    exit();
  }
  
  // set up all parameters
  int timeslice = atoi(argv[1]);
  int iteration = atoi(argv[2]);
  char* job = argv[3];
  int jobcount = atoi(argv[4]);
   
   
  // printf(0,"start\n");
  int jobArray[jobcount];
  for(int k = 0; k < jobcount; k++) {
      jobArray[k]= fork2(0);
      //child process
      if(jobArray[k]==0){
      char* args[] = {job,0};
      exec(job,args);
      // parent do nothing 
      }else if(jobArray[k]>0){
      
      }
      
    }
    
     // printf(0,"make child\n");
  for(int i = 0; i < iteration; i++) {
    for(int k = 0; k < jobcount; k++) {
     //set pri high
     if(jobArray[k]>0){
     
     setpri(jobArray[k],1);
     sleep(timeslice);
     setpri(jobArray[k],0);
     }
     //sleep time slice 
     //set pri low 
    }
  }
  // printf(0,"kill child\n");
  // kill all children process
    for(int k = 0; k < jobcount; k++) {
      if(jobArray[k]>0){
      kill(jobArray[k]);
      }
      wait();
    }
}
