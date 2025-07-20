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
#include "common.h"
#include "sdb.h"
#include <stdio.h>
#include <string.h>
#define NR_WP 32

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;
bool check_watchpoints() {
  bool stop = false;      // 用于记录是否有监视点被触发
  WP *wp = get_wp_head(); // 获取当前正在使用的监视点链表头指针

  while (wp != NULL) { // 遍历链表
    bool success = true;
    word_t new_num = expr(wp->expr, &success); // 重新求值表达式

    if (!success) { // 表达式解析失败，跳过
      printf("Watchpoint %d: Failed to evaluate expression: %s\n", wp->NO,
             wp->expr);
      wp = wp->next;
      continue;
    }

    if (new_num != wp->last_num) { // 表达式值发生变化，触发监视点
      printf("\nWatchpoint %d triggered!\n", wp->NO);
      printf("Expr: %s\n", wp->expr);
      printf("Old value = %u (0x%x)\n", wp->last_num, wp->last_num);
      printf("New value = %u (0x%x)\n", new_num, new_num);

      wp->last_num = new_num;       // 更新监视点当前值
      nemu_state.state = NEMU_STOP; // 设置状态，让 CPU 停止执行
      stop = true;                  // 标记触发了监视点
    }
    wp = wp->next;
  }
  return stop; // 如果触发了任何监视点就返回 true
}
WP *get_wp_head() { return head; }
void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL; //（当前正在监控的监视点）调用 new_wp() 添加；调用 free_wp() 移除
  free_ = wp_pool; //空闲链表（未使用的监视点池）每次调用
                   // new_wp()，就从这里取出一个监视点
}

/* TODO: Implement the functionality of watchpoint */

WP *new_wp() {
  assert(free_ != NULL); //如果没有空闲监视点，终止

  WP *wp = free_;      //从free_中取一个
  free_ = free_->next; //把之前的指针移动到下一节点
  wp->next =
      head; //把新节点的 next 指针指向原来的链表头 head(管理正在使用的监视点)
  head = wp; //头指针指向新的监视点

  return wp;
}

void free_wp(WP *wp) { //释放监视点，从head放回free_
  if (wp == NULL)
    return;
  WP *prev = NULL;
  WP *cur = head;

  while (cur != NULL && cur != wp) {
    prev = cur;
    cur = cur->next;
  }
  if (prev == NULL)
    head = cur->next; // 从 head 中移除
  else
    prev->next = cur->next;

  cur->next = free_; // 加入 free_ 链表头部

  free_ = cur;
}
