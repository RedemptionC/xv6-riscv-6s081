// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"
// #include "kernel/fs.h"

// #define LINE 128
// #define PARAMS 10
// int main(int argc,char* argv[]){
//     if(argc<2){
//         exit();
//     }
//     char *cmd=argv[1];
//     // line是作为参数使用的
//     char line[LINE];
//     // char *p=gets(line,LINE);
//     char *params[PARAMS];
//     int index=0;
//     char *p=line;
//     // 例如：xargs echo hello,这样的，已经带了参数，就要把参数设置到
//     // 最终执行时需要的参数数组里
//     params[index++]=cmd;
//     for(int i=2;i<argc;i++){
//         params[index++]=argv[i];
//     }
//     // while(strlen(p)!=0){
//     //     if(fork()==0){
//     //         params[index++]=p;
//     //         exec(cmd,params);
//     //         printf("exec fail!\n");
//     //         exit();
//     //     }else{
//     //         wait();
//     //         gets(line,LINE);
//     //     }
//     // }
//     int n=0;
//     while((n=read(0,line,LINE))>0){
//         if(fork()==0){
//             params[index++]=p;
//             exec(cmd,params);
//             printf("exec fail!\n");
//             exit();
//         }else{
//             wait();
//             // gets(line,LINE);
//         }
//     }
//     exit();
// }
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  char buf2[512];
  char buf[32][32];
  char *pass[32];
    // 因为pass本来就是指针的数组，让他们指向实际的字符串保存的地方
  for (int i = 0; i < 32; i++)
    pass[i] = buf[i];

  int i;
  for (i = 1; i < argc; i++)
    strcpy(buf[i - 1], argv[i]);

  int n;
  while ((n = read(0, buf2, sizeof(buf2))) > 0) {
    int pos = argc - 1;
    char *c = buf[pos];
    for (char *p = buf2; *p; p++) {
      if (*p == ' ' || *p == '\n') {
        *c = '\0';
        pos++;
        c = buf[pos];
      } else
        *c++ = *p;
    }
    *c = '\0';
    pos++;
    pass[pos] = 0;

    if (fork()) {
      wait();
    } else
      exec(pass[0], pass);
  }

  if (n < 0) {
    printf("xargs: read error\n");
    exit();
  }

  exit();
}