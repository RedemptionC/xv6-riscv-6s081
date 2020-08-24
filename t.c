#include <stdio.h>

typedef long Align;

union header {
  struct
  {
    union header *ptr; // 8
    unsigned size;      // 4
    char a[5];
  } s;
//   Align x;              // 8
};

typedef union header Header;
void print(char * p){
  printf("%p\n",p);
}
int main(){
  int x=64;
  print(&x);
  print((char*)&x);
}