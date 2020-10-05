#include <stdio.h>
#include <stdlib.h>
#include "dungeon.h"

static void print_board_pbm(FILE* fp, int* board, const BOARD_PARAMETER parameter);

int main(int argc, char *argv[]) {
  if (argc != 4) return 1;

  const int seed = atoi(argv[3]);
  srand(seed);

  BOARD_PARAMETER parameter;
  parameter.width           = atoi(argv[1]);
  parameter.height          = atoi(argv[2]);
  parameter.panel_size_min  = 12;
  parameter.room_size_min   = 5;
  parameter.room_size_max   = 10;
  parameter.corridor_margin = 2;
  parameter.wall_margin     = 1;

  int *board = (int *) malloc(parameter.width * parameter.height * sizeof(int));
  init_board(board, parameter);
  print_board_pbm(stdout, board, parameter);

  free(board);
  return 0;
}

static void print_board_pbm(FILE* fp, int* board, const BOARD_PARAMETER parameter) {
  fprintf(fp, "P1\n");
  fprintf(fp, "%d %d\n", parameter.width, parameter.height);

  for (int y=0; y<parameter.height; y++) {
    for (int x=0; x<parameter.width; x++) {
      if (board[x*parameter.height+y] == -2) fprintf(fp, "1");
      else                                   fprintf(fp, "0");
    }
    fprintf(fp, "\n");
  }
}
