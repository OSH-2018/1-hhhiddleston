#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#define  MAXPIPE 10

int main(){
  char cmd[256];
  char *args[128];
  while(1){
    printf("# ");
    fflush(stdin);  // clean the buffer
    fgets(cmd, 256, stdin); // read the input
    int i=0,j;
    for(;cmd[i]!='\n';i++){
      ;
    }
    cmd[i]='\0'; // end the cmd
    int flag; // 0 for argument env ; 1 for space env
    args[0] = cmd;
    while(*args[0] == ' ')
      args[0]++;
    for(i=0;*args[i];i++){
      flag=0;
      for(args[i+1]=args[i]+1;*args[i+1];args[i+1]++){
        if(*args[i+1]==' '){
          flag = 1;
          *args[i+1] = '\0';
        }
        else{
          if(flag==1){
            break;
          }
        }
      }
    }
    args[i]=NULL;

    int pipe_num = 0;
    int cmd_pos[MAXPIPE];
    cmd_pos[0] = 0;
    for(i=0;args[i]!=NULL;i++)
      if(strcmp(args[i], "|") == 0){
        args[i] = NULL;
        pipe_num += 1;
        cmd_pos[pipe_num] = i+1;
      }
    int cmd_num = pipe_num+1;

    // no cmd
    if(!args[0])
      continue;

    // internal cmd
    if(strcmp(args[0], "cd") == 0){
      // change directory
      if(args[1])
        chdir(args[1]);
      continue;
    }
    if(strcmp(args[0], "exit") == 0){
      return 0;
    }

    // create pipes
    int pipe_fd[MAXPIPE][2];
    pid_t pid;
    for(i=0;i<pipe_num;i++)
      pipe(pipe_fd[i]);
    // create processes
    for(i=0;i<cmd_num;i++){
      pid = fork();
      if(pid==0){
        if(pipe_num){
          if(i==0){
            // first child
            dup2(pipe_fd[i][1], 1);
            close(pipe_fd[i][0]);
            close(pipe_fd[i][1]);
            for(j=1;j<pipe_num;j++){
              close(pipe_fd[j][0]);
              close(pipe_fd[j][1]);
            }
          }
          else if(i==pipe_num){
            // last child
            dup2(pipe_fd[i-1][0], 0);
            close(pipe_fd[i-1][0]);
            close(pipe_fd[i-1][1]);
            for(j=0;j<pipe_num-1;j++){
              close(pipe_fd[j][0]);
              close(pipe_fd[j][1]);
            }
          }
          else{
            dup2(pipe_fd[i-1][0], 0);
            close(pipe_fd[i-1][0]);

            dup2(pipe_fd[i][1], 1);
            close(pipe_fd[i][1]);
            for(j=0;j<pipe_num;j++){
              if((j!=i-1)||j!=i){
                close(pipe_fd[j][0]);
                close(pipe_fd[j][1]);
              }
            }
          }
        }
        execvp(args[cmd_pos[i]], args+cmd_pos[i]);
      }
      else{
        // father process
        for(i = 0; i < pipe_num; ++i)  {
          close(pipe_fd[i][0]);
          close(pipe_fd[i][1]);
        }
        for(i = 0; i < cmd_num; ++i)  {
          wait(NULL);
        }
      }
    }
  }
}
