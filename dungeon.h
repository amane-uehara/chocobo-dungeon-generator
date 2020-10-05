#ifndef _DUNGEON_H_
#define _DUNGEON_H_

typedef struct _BOARD_PARAMETER {
  int width;           // 100
  int height;          //  50
  int panel_size_min;  //  12  > room_size_min + corridor_margin*2 + 1
  int room_size_min;   //   5
  int room_size_max;   //  10
  int corridor_margin; //   2
  int wall_margin;     //   1
} BOARD_PARAMETER;

void init_board(int* board, const BOARD_PARAMETER parameter);

#endif
