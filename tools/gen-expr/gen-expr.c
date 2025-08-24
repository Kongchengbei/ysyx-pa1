/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";
//TODO:递归实现
static void gen_rand_num(){
  int num = rand() % 10000;
  char temp[16];
  sprintf(temp, "%d", num);
  strcat(buf, temp);//在原buf右侧加temp

}
static void gen_rand_op(){
  const char ops[] = "+-*/";
  char op = ops[rand() % 4];
  int len = strlen(buf);//计算长度，+1放到数后面
  buf[len] = op;
  buf[len + 1] = '\0';
}
static void gen_rand_expr_rec(int depth) {
  if (depth > 9) {
    gen_rand_num();
    return;//输出最后一项后直接返回
  }
  int type = rand() % 3;
  if (type == 0 ){
    gen_rand_num();
  } else if (type == 1) {
    strcat(buf, "(");
    gen_rand_expr_rec(depth + 1);
    strcat(buf, ")");
  }
  else {
    gen_rand_expr_rec(depth + 1);//先递归生成一个左操作数
    gen_rand_op();
    gen_rand_expr_rec(depth + 1);//有运算符之后生成右操作数
  }
}
static void gen_rand_expr() {
  buf[0] = '\0';//清空
  gen_rand_expr_rec(0);
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
