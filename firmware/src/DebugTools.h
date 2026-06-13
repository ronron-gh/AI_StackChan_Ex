#ifndef _DEBUG_TOOLS_H
#define _DEBUG_TOOLS_H

#include <M5Unified.h>

/// 関数プロトタイプ宣言 /// 
void check_heap_free_size(void);
void check_heap_largest_free_block(void);
//void check_task_status(void);
uint32_t get_elapsed_time_micro(const char* str = "");
void i2c_scan(TwoWire& wire);
void check_board(void);
#endif