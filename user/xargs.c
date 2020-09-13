#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define LINE 128
#define PARAMS 10
int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    exit();
  }

  // xarg的第一个参数 就是要执行的命令
  // 其余的是该程序运行的参数
  char *cmd = argv[1];
  // line是作为参数使用的
  char line[LINE];
  // char *p=gets(line,LINE);
  char *params[PARAMS];
  int index = 0;
  // 例如：xargs echo hello,这样的，已经带了参数，就要把参数设置到
  // 最终执行时需要的参数数组里
  params[index++] = cmd;
  for (int i = 2; i < argc; i++)
  {
    params[index++] = argv[i];
  }

  int n = 0;
  while ((n = read(0, line, LINE)) > 0)
  {
    if (fork() == 0)
    {
      char *t = (char *)malloc(sizeof(char) * LINE);
      int c = 0;
      int i=0;
      for (; i < n; i++)
      {
        if (line[i] == '\n' || line[i] == ' ')
        {
          t[c]='\0';
          // 从标准输入读进来的也放进params
          params[index++] = t;
          // memset(t,0,LINE);
          c=0;
          t = (char *)malloc(sizeof(char) * LINE);
        }else
          t[c++] = line[i];
      }
      t[c] = '\0';
      params[index]=0;
      exec(cmd, params);
      printf("exec fail!\n");
      exit();
    }
    else
    {
      wait();
    }
  }
  exit();
}
