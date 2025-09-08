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

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include "memory/vaddr.h" //从虚拟地址读取内存数据
#include <regex.h>
// word_t vaddr_read(vaddr_t addr, int len);

enum {
  TK_NOTYPE = 256,
  TK_HEX,
  TK_DEC,
  TK_EQ,
  TK_PLUS,
  TK_NEQ,
  TK_MINUS,
  TK_AND,
  TK_MUL,
  TK_DIV,
  TK_LP,
  TK_RP,
  TK_REG,
  TK_NEG,
  TK_DEREF,
  /* TODO: Add more token types */
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

    /* TODO: Add more rules.
     * Pay attention to the precedence level of different rules.
     */
    {" +",
     TK_NOTYPE}, // spaces 空格不参与计算,匹配一个或多个空格，忽略不生成token
    {"==", TK_EQ},  // equal
    {"!=", TK_NEQ}, //不等于
    {"&&", TK_AND}, // 逻辑与：两个 &，用于布尔逻辑判断

    {"\\+", TK_PLUS},              // plus
    {"-", TK_MINUS},               //减法
    {"\\*", TK_MUL},               // 乘
    {"/", TK_DIV},                 // 除
    {"\\(", TK_LP},                //左括号
    {"\\)", TK_RP},                // 右括号
    {"\\$[a-zA-Z0-9]+", TK_REG},   // 寄存器，$开头
    {"0[xX][0-9a-fA-F]+", TK_HEX}, //十六进制
    {"[0-9]+", TK_DEC},            //十进制
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;     //定义类型（.type）
  char str[32]; //这个token对应的字符串(.str)
} Token;        // Token变量
// token[i].type --token的第i段的类型

static Token tokens[1000] __attribute__((used)) = {};//长度过短，修改
static int nr_token __attribute__((used)) = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    //正则匹配，使用每一条规则来拆分字符串，生成tokens
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++) { // NR_REGEX:规则总数
      Log("pos=%d, str=%s\n", position, e+position);
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 &&
          pmatch.rm_so == 0) {
      int substr_len = pmatch.rm_eo;
      if (substr_len == 0) {
        // 防止卡死，至少前进一步
        position++;
        break;
      } 

        char *substr_start = e + position;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s, position: %d", i,
            rules[i].regex, position, substr_len, substr_len, substr_start, position + substr_len);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
        for (int i = 0; i < nr_token; i++) {
        }
        switch (rules[i].token_type) {
        case TK_NOTYPE:
          break;
        case TK_HEX:
        case TK_DEC:
          tokens[nr_token].type = rules[i].token_type;
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';
          nr_token++;
          break;
        case TK_REG:
          tokens[nr_token].type = rules[i].token_type;
          
          strncpy(tokens[nr_token].str, substr_start + 1, substr_len - 1); //寄存器名不包括$
          tokens[nr_token].str[substr_len - 1] = '\0';
          nr_token++;
          break;
        default:
          tokens[nr_token].type = rules[i].token_type;
          tokens[nr_token].str[0] = '\0';
          nr_token++;
          break;

          // TODO();
        }

        break;
      }
    }


    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }


  }

  return true;
}
//判断tokens[p…q]是否被一对最外层的括号包裹
bool check_parentheses(int p, int q) {
  if (tokens[p].type != TK_LP || tokens[q].type != TK_RP) 
    return false;
  int count = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == TK_LP) count++;
    if (tokens[i].type == TK_RP) count--;
    if (count == 0 && i < q) return false; // 说明括号提前闭合
  }
  return count == 0;
}
//运算符优先级定义，数值越小，优先级约低
static int get_priority(int type) {
  switch (type) {
  case TK_AND:
    return 1;
  case TK_EQ:
  case TK_NEQ:
    return 2;
  case TK_PLUS:
  case TK_MINUS:
    return 3;
  case TK_MUL:
  case TK_DIV:
    return 4;
  case TK_NEG:
  case TK_DEREF:
    return 5; // 一元操作，优先级高
  default:
    return -1;
  }
}

static bool is_unary_context(int prev_type) { //是否为一元运算,prev_type--tokens[i-1].type
  return prev_type == TK_PLUS || prev_type == TK_MINUS || prev_type == TK_MUL ||
         prev_type == TK_DIV || prev_type == TK_EQ || prev_type == TK_NEQ ||
         prev_type == TK_AND || prev_type == TK_LP;
}


static word_t eval(int p, int q, bool *success) {
  if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1, success);
  }
  if (p > q) {
    *success = false;
    return 0;
  }
  if (p == q) {
    Token t = tokens[p];
    if (t.type == TK_DEC){
      *success = true;
      return strtoul(t.str, NULL, 10);
    }
    if (t.type == TK_HEX){
      *success = true;
      return strtoul(t.str, NULL, 16);
    }
    if (t.type == TK_REG){
      return isa_reg_str2val(t.str, success);
    }
    *success = false;
    return 0;
  }

  int op = -1;
  int min_pri = 9999;  //对应优先级最低
  int paren_depth = 0; //括号的层数
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == TK_LP) {
      paren_depth++;
    } else if (tokens[i].type == TK_RP) {
      paren_depth--;
    } else if (paren_depth == 0) {
      int pri = get_priority(tokens[i].type);
      if (pri >= 0 && pri <= min_pri) {
        min_pri = pri;
        op = i;
      }
    }
  }
  if (op == -1) {
    *success = false;
    return 0;
  }
  word_t num1 = 0;
  word_t num2 = 0; //两个存放操作数变量,分别为符号的左右两侧数，12从左到右

  if (tokens[op].type == TK_NEG || tokens[op].type == TK_DEREF) {
    //只有一个操作数的时候（-、解引用）
    num2 = eval(op + 1, q, success); //递归，求右侧表达式
    if (!*success)
      return 0;
    if (tokens[op].type == TK_NEG) //符号是-
      return -num2;
    if (tokens[op].type == TK_DEREF) //符号是*（解引用）
      return vaddr_read(num2, 4);    //取四字节

  } else {
    //左右两边都有操作数
    num1 = eval(p, op - 1, success);
    num2 = eval(op + 1, q, success);
    switch (tokens[op].type) {
    case TK_PLUS:
      return num1 + num2; // 加法
    case TK_MINUS:
      return num1 - num2; // 减法
    case TK_MUL:
      return num1 * num2; // 乘法
    case TK_DIV:
      return num2 ? num1 / num2 // 除法，注意不能除以 0
                  : (*success = false, 0);

    case TK_EQ:
      return num1 == num2; // 相等比较（返回 1 或 0）
    case TK_NEQ:
      return num1 != num2; // 不等比较
    case TK_AND:
      return num1 && num2; // 逻辑与（true && true 为真）

    default: // 如果是未知运算符
      *success = false;
      return 0;
    }
  }

  return 0;
}
// word_t expr(char *e, bool *success) {
word_t expr(char *e , bool *success) {
  
 // char *e = "(((((((87/88*(44)))))/38/46-((93+86))+2)))";

  if (!make_token(e)) {
    *success = false;
    return 0; //拆分失败
  }

  for (int i = 0; i < nr_token; i++) {
    //一元操作
    if (tokens[i].type == TK_MUL &&
        (i == 0 || is_unary_context(tokens[i - 1].type))) {
      tokens[i].type = TK_DEREF;
    }
    if (tokens[i].type == TK_MINUS &&
        (i == 0 || is_unary_context(tokens[i - 1].type))) {
      tokens[i].type = TK_NEG;
    }
  }
  //进入eval
  return eval(0, nr_token - 1, success);
}
/* TODO: Insert codes to evaluate the expression. */
/*TODO();*/
