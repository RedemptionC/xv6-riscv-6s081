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
  printf("**********\n");
  for (int i = 0; i < argc; i++)
  {
    printf("%d %s\n", i, argv[i]);
  }
  printf("**************\n");
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
      // TODO: 这里应该创建一个新的内存空间，保存新进入的参数，还是\0结尾
      char *t = (char *)malloc(sizeof(char) * LINE);
      int c = 0;
      for (int i = 0; i < n; i++)
      {
        if (line[i] == '\n' || line[i] == ' ')
        {
          break;
        }
        t[c++] = line[i];
      }
      t[c] = '\0';
      params[index++] = t;
      printf("**********\n");
      for (int i = 0; i < index; i++)
      {
        printf("%d %s\n", i, params[i]);
      }
      printf("**************\n");

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
