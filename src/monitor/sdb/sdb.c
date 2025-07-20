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

#include "sdb.h"
#include "memory/paddr.h"
#include <cpu/cpu.h>
#include <isa.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin.
 */
static char *rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) { return -1; }

static int cmd_help(char *args);

static int cmd_si(char *args);

static int cmd_info(char *args);

static int cmd_x(char *args);

static int cmd_p(char *args);

static int cmd_w(char *args);

static int cmd_d(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler)(char *);
} cmd_table[] = {
    {"help", "Display information about all supported commands", cmd_help},
    {"c", "Continue the execution of the program", cmd_c},
    {"q", "Exit NEMU", cmd_q},

    /* TODO: Add more commands */
    {"si",
     "Pause the program after executing N instructions step by step; if N is "
     "not specified, it defaults to 1.",
     cmd_si},
    {"info", "Print program status", cmd_info},
    {"x",
     "FIND THE VALUE OF THE EXPRESSION EXPR AND USE THE RESULT AS THE STARTING "
     "MEMORY address, which outputs consecutive N 4 bytes in hexadecimal form",
     cmd_x},
    {"p",
     "Calculate the value of the expression EXPR. The operations supported by "
     "EXPR can be found in the section on expression evaluation in debugging.",
     cmd_p},
    {"w", "Set a watchpoint for EXPR", cmd_w},
    {"d", "Delet watchpoint with number N", cmd_d},
}; //数据定义
#define NR_CMD ARRLEN(cmd_table)
//函数定义
static int cmd_help(char *args) { /* extract the first argument */

  char *arg = strtok(
      args, " "); //从args向后的一个space后的第一个单词，去识别help的哪一个指令
  int i;
  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  } else {
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_si(char *args) {
  int n = 1; //默认是1
  if (args != NULL) {
    sscanf(args, "%d", &n);
  }
  cpu_exec(n); //执行n条
  return 0;
}
static int cmd_info(char *args) {
  if (strcmp(args, "r") == 0) {
    isa_reg_display(); // nvim src/isa/riscv32/reg.c
  } else if (strcmp(args, "w") == 0) {
    WP *wp = get_wp_head();
    if (wp == NULL) {
      printf("No watchpoints.\n");
    }
    while (wp != NULL) {
      printf("Watchpoint %d: %s = %u (0x%x)\n", wp->NO, wp->expr, wp->last_num,
             wp->last_num);
      wp = wp->next;
    }
  }
  return 0;
}
static int cmd_x(char *args) {
  int n;          //读取的内存单元个数
  char *expr_str; //志向表达式字符串的指针

  if (args == NULL) {
    printf("X N EXPR\n");
    return 0;
  }

  n = strtol(args, &expr_str, 10);

  while (expr_str[0] == ' ')
    expr_str++;

  bool success = true;
  uint32_t addr = expr(expr_str, &success); // sdb.h

  if (!success) {
    printf("Invalid expression:%s\n", expr_str);
    return 0;
  }
  for (int i = 0; i < n; i++) {
    uint32_t data = paddr_read(addr + i * 4, 4);
    printf("0x%08x: 0x%08x\n", addr + i * 4, data);
  }

  return 0;
}
static int cmd_p(char *args) {
  if (args == NULL) {
    printf("Usage: p EXPR\n");
    return 0;
  }
  bool success = true;
  word_t result = expr(args, &success);

  if (success) {
    printf("Result = %u (0x%x)\n", result, result);
  } else {
    printf("Invalid expression.\n");
  }

  return 0;
}
static int cmd_w(char *args) {
  if (args == NULL) {
    printf("w EXPR\n");
    return 0;
  }
  bool success = true;
  word_t num = expr(args, &success); //计算表达式的值

  if (!success) {
    printf("The evaluate expression ERROR:%s\n", args); //表达式错误（无效）
    return 0;
  }
  WP *new_wpnode = new_wp(); //申请一个在空闲链表的新的监视点节点

  strncpy(new_wpnode->expr, args,
          sizeof(new_wpnode->expr) - 1); // // 将表达式字符串保存到监视点中
  new_wpnode->last_num = num;            // 记录下该表达式的当前值
  printf("Watchpoint %d set: %s = %u (0x%x)\n", new_wpnode->NO,
         new_wpnode->expr, num, num);
  return 0;
}
//将表达式字符串保存到监视点中
static int cmd_d(char *args) {
  if (args == NULL) {
    printf("d N\n");
    return 0;
  }
  int no;
  sscanf(args, "%d", &no); // 解析用户输入的监视点编号
  WP *wp = get_wp_head();  // 获取活跃链表头

  while (wp != NULL && wp->NO != no) {
    wp = wp->next;
  }

  if (wp == NULL) {
    printf("No watchpoint number %d.\n", no);
    return 0;
  }

  free_wp(wp); // 正确传入 WP 指针
  printf("Watchpoint %d deleted.\n", no);
  return 0;
}

void sdb_set_batch_mode() { is_batch_mode = true; }

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL;) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) {
      continue;
    }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) {
          return;
        }
        break;
      }
    }

    if (i == NR_CMD) {
      printf("Unknown command '%s'\n", cmd);
    }
  }
}
void init_sdb() {

  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
