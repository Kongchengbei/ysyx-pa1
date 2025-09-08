/***************************************************************************************
 * Copyright (c) 2014-2024 Zihao Yu, Nanjing University
 *
 * NEMU is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 *PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 *KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 *NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 ***************************************************************************************/

#include <common.h>
//TODO
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "monitor/sdb/expr.h"


void init_monitor(int, char *[]);
void am_init_monitor();
void engine_start();
int is_exit_status_bad();
//TODO
void test_expr(const char *filename){
  FILE *fp = fopen(filename, "r");
 char line[1024];
 int pass = 0 ;
 int total = 0;
while (fgets(line, sizeof(line), fp)){
  unsigned expected = 0 ;
  char expr_buf[1024];
  if (sscanf(line, "%u %[^\n]",&expected , expr_buf) !=2)
    continue;
  bool success = false ;
  uint32_t result = expr(expr_buf, &success);//expr函数
  total++;
  if (success && result == expected){
    pass ++;
    printf("PASS %s= %u\n", expr_buf,result);
  }else {
    printf("FALSE %s, expect %u, but restlt %u \n", expr_buf, expected,result);
    break;
  }
}
printf("PASS: %d \n",pass);
fclose(fp);
}
int main(int argc, char *argv[]) {
 
  /* Initialize the monitor. */
#ifdef CONFIG_TARGET_AM
  am_init_monitor();
#else
  init_monitor(argc, argv);
#endif
const char *test_file = "tools/gen-expr/build/input";
test_expr(test_file);
return 0;
  /* Start engine. */
  engine_start();

  return is_exit_status_bad();//这是nemu退出状态检查
}
