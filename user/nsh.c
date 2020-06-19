#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

//*****************START  from sh.c *******************


#define MAXARGS 10
#define MAXWORD 30
#define MAXLINE 100

int getcmd(char *buf, int nbuf)
{
    fprintf(2, "@ ");
    memset(buf, 0, nbuf);
    gets(buf, nbuf);
    if (buf[0] == 0) // EOF
        return -1;
    return 0;
}
char whitespace[] = " \t\r\n\v";
char args[MAXARGS][MAXWORD];
//*****************END  from sh.c ******************
void setargs(char *cmd, char* argv[])
{
    // 让argv的每一个元素都指向args的每一行
    for(int i=0;i<MAXARGS;i++){
        argv[i]=&args[i][0];
    }

    int i = 0; // 表示第i个word
    // int k = 0; // 表示word中第k个char
    int j = 0;

    for (; cmd[j] != '\n' && cmd[j] != '\0'; j++)
    {

        // 跳过之前的空格
        while (strchr(whitespace,cmd[j])){
            j++;
        }
        argv[i++]=cmd+j;
        // 只要不是空格，就j++,找到下一个空格
        while (strchr(whitespace,cmd[j])==0){
            j++;
        }
        cmd[j]='\0';
    }
    argv[i]=0;

}

void runcmd(char *cmd)
{

    char* argv[MAXARGS];
    setargs(cmd, argv);
    exec(argv[0], argv);
}

int main()
{
    char buf[MAXLINE];
    // Read and run input commands.
    while (getcmd(buf, sizeof(buf)) >= 0)
    {

        if (fork() == 0)
            runcmd(buf);
        wait(0);
    }

    exit(0);
}