#include "dungeon.h"
#include <stdlib.h>
#include <stdbool.h>

#define BLANK       (-2)
#define CORRIDOR    (-1)
#define NODE_UNUSED (-2)
#define NODE_CROSS  (-1)
#define BFS_MAX_DEPTH 8

typedef struct _BOARD {
  int *board;
  int width;
  int height;
} BOARD;

typedef struct _PANEL {
  int r[2];
  int size[2];
  int room_r[2];
  int room_size[2];
  struct _PANEL *prev;
  struct _PANEL *next;
} PANEL;

typedef struct _NODE {
  int type; // DEF:NODE_UNUSED, DEF:NODE_CROSS, ROOM(NUMBER >= 0)
  int r[2];
  struct _NODE *edge[4]; // right, left, up, down
  int length;
  struct _NODE *from;
} NODE;

typedef struct _QUEUE {
  int back;
  int front;
  NODE **buff;
} QUEUE;

static void initialize_board(BOARD* board);
static void initialize_panel(PANEL* panel, const int panel_index_max);
static void initialize_node(NODE* node, const int node_index_max);
static void initialize_queue(QUEUE* queue, const int node_index_max);
static int divide_panel(const int p_index, int* p_used_index, PANEL* panel, const int panel_size_min, const int panel_index_max, const int corridor_margin);
static int find_largest_panel(const int p_used_index, const PANEL* panel, const int panel_size_min);
static void register_room(const int p_used_index, PANEL* panel, const int room_size_min, const int room_size_max, const int corridor_margin);
static void draw_long_line(BOARD* board, const int p_used_index, const PANEL* panel);
static void draw_room(BOARD* board, const int p_used_index, const PANEL* panel);
static void connect_room_corridor(BOARD* board, const int p_used_index, const PANEL* panel, const int corridor_margin, const int wall_margin);
static int register_node(NODE* node, const BOARD* board);
static int count_harline(const int x, const int y, const BOARD* board);
static void connect_node(NODE* node, const int n_used_index, const BOARD* board);
static void eliminate_node(NODE* eliminate, BOARD* board);
static void eliminate_edge(NODE* end1, NODE* end2, BOARD* board);
static int count_edge(NODE* n);
static void bfs(QUEUE* queue, const int node_index_max);
static void enqueue(NODE* node, QUEUE* queue, const int node_index_max);
static NODE* dequeue(QUEUE* queue, const int node_index_max);
static NODE* get_loop_goal(const int n_gate_index, const int n_used_index, NODE* node, BOARD* board, const int node_index_max);
static void eliminate_loop(NODE* goal, BOARD* board);
static void eliminate_all_loop(const int p_used_index, const int n_used_index, NODE* node, BOARD* board, const int node_index_max);
static void set_board(BOARD* board, const int x, const int y, const int value);
static int get_board(const BOARD* board, const int x, const int y);

void init_board(int* arg_board, const BOARD_PARAMETER parameter) {

  BOARD board;
  board.board  = arg_board;
  board.width  = parameter.width;
  board.height = parameter.height;

  const int panel_index_max = parameter.width * parameter.height / (parameter.panel_size_min-1)/(parameter.panel_size_min-1);
  const int node_index_max  = 4*panel_index_max;

  initialize_board(&board);

  PANEL *panel;
  panel = (PANEL*) malloc(sizeof(PANEL)*panel_index_max);
  initialize_panel(panel, panel_index_max);

  panel[0].size[0] = board.width;
  panel[0].size[1] = board.height;

  int p_used_index = 1;
  int largest_panel_index = 0;
  while (largest_panel_index >= 0) {
    divide_panel(largest_panel_index, &p_used_index, panel, parameter.panel_size_min, panel_index_max, parameter.corridor_margin);
    largest_panel_index = find_largest_panel(p_used_index, panel, parameter.panel_size_min);
  }
  draw_long_line(&board, p_used_index, panel);

  register_room(p_used_index, panel, parameter.room_size_min, parameter.room_size_max, parameter.corridor_margin);
  draw_room(&board, p_used_index, panel);

  connect_room_corridor(&board, p_used_index, panel, parameter.corridor_margin, parameter.wall_margin);

  NODE *node;
  node = (NODE*) malloc(sizeof(NODE)*node_index_max);
  initialize_node(node, node_index_max);

  int n_used_index = register_node(node, &board);

  connect_node(node, n_used_index, &board);

  for (int n_index=0; n_index<n_used_index; n_index++) {
    NODE *n = &(node[n_index]);
    if (n -> type == NODE_CROSS && count_edge(n) < 2) eliminate_node(n, &board);
  }

  eliminate_all_loop(p_used_index, n_used_index, node, &board, node_index_max);

  free(panel);
  free(node);
}

static void initialize_board(BOARD* board) {
  for (int x=0; x < board -> width; x++) {
    for (int y=0; y < board -> height; y++) {
      set_board(board, x, y, BLANK);
    }
  }
}

static void initialize_panel(PANEL* panel, const int panel_index_max) {
  for (int p_index=0; p_index<panel_index_max; p_index++) {
    panel[p_index].r[0] = 0;
    panel[p_index].r[1] = 0;
    panel[p_index].size[0] = 0;
    panel[p_index].size[1] = 0;
    panel[p_index].prev = NULL;
    panel[p_index].next = NULL;
  }
}

static void initialize_node(NODE* node, const int node_index_max) {
  for (int n_index=0; n_index<node_index_max; n_index++) {
    node[n_index].type = NODE_UNUSED;
    node[n_index].r[0] = -1;
    node[n_index].r[1] = -1;
    node[n_index].edge[0] = NULL;
    node[n_index].edge[1] = NULL;
    node[n_index].edge[2] = NULL;
    node[n_index].edge[3] = NULL;
    node[n_index].length  = node_index_max;
    node[n_index].from    = NULL;
  }
}

static void initialize_queue(QUEUE* queue, const int node_index_max) {
  queue -> back  = 0;
  queue -> front = 0;
  for (int q_index=0; q_index<node_index_max; q_index++) {
    queue -> buff[q_index] = NULL;
  }
}

static int divide_panel(const int p_index, int* p_used_index, PANEL* panel, const int panel_size_min, const int panel_index_max, const int corridor_margin) {
  if (*p_used_index == panel_index_max) return 0;

  PANEL *t = &(panel[p_index]);
  const int divide = t -> size[0] < t -> size[1] ? 1 : 0;
  if (t -> size[divide] <= 2*panel_size_min) return 0;

  const int keep = 1 - divide;

  PANEL *n = &(panel[*p_used_index]);
  *p_used_index = *p_used_index + 1;

  n -> next = t -> next;
  n -> prev = t;
  t -> next = n;

  const int size = panel_size_min + (rand() % ((t -> size[divide] - 2*panel_size_min)/corridor_margin)) * corridor_margin;
  n -> r[divide]    = t -> r[divide] + size;
  n -> r[keep]      = t -> r[keep];
  n -> size[divide] = t -> size[divide] - size;
  n -> size[keep]   = t -> size[keep];

  t -> size[divide] = size;

  return 1;
}

static int find_largest_panel(const int p_used_index, const PANEL* panel, const int panel_size_min) {
  int max_size = 2*panel_size_min + 1;
  int max_index = -1;
  for (int p_index=0; p_index<p_used_index; p_index++) {
    PANEL t = panel[p_index];
    for (int direction=0; direction<2; direction++) {
      if (max_size < t.size[direction]) {
        max_size  = t.size[direction];
        max_index = p_index;
      }
    }
  }

  return max_index;
}

static void register_room(const int p_used_index, PANEL* panel, const int room_size_min, const int room_size_max, const int corridor_margin) {
  for (int p_index=0; p_index<p_used_index; p_index++) {
    PANEL *t = &(panel[p_index]);
    for (int direction=0; direction<2; direction++) {

      const int tmp_room_size = room_size_min + (rand() % (t -> size[direction] - room_size_min - 2*corridor_margin - 1));
      const int room_size   = tmp_room_size < room_size_max ? tmp_room_size : room_size_max;
      const int room_origin = (1 + corridor_margin) + rand() % (t -> size[direction] - room_size - 2*corridor_margin - 1);
      t -> room_size[direction] = room_size;
      t -> room_r[direction]    = t -> r[direction] + room_origin;
    }
  }
}

static void draw_long_line(BOARD* board, const int p_used_index, const PANEL* panel) {
  for (int p_index=0; p_index<p_used_index; p_index++) {
    PANEL t = panel[p_index];
    for (int x=t.r[0]; x<t.r[0]+t.size[0]; x++) {
      set_board(board, x, t.r[1], CORRIDOR);
    }

    for (int y=t.r[1]; y<t.r[1]+t.size[1]; y++) {
      set_board(board, t.r[0], y, CORRIDOR);
    }
  }

  for (int x=0; x<board -> width; x++)  set_board(board, x, 0, BLANK);
  for (int y=0; y<board -> height; y++) set_board(board, 0, y, BLANK);
}

static void draw_room(BOARD* board, const int p_used_index, const PANEL* panel) {
  for (int p_index=0; p_index<p_used_index; p_index++) {
    PANEL t = panel[p_index];
    for (int x=t.room_r[0]; x<t.room_r[0]+t.room_size[0]; x++) {
      for (int y=t.room_r[1]; y<t.room_r[1]+t.room_size[1]; y++) {
        set_board(board, x, y, p_index);
      }
    }
  }
}

static void connect_room_corridor(BOARD* board, const int p_used_index, const PANEL* panel, const int corridor_margin, const int wall_margin) {
  for (int p_index=0; p_index<p_used_index; p_index++) {
    PANEL t = panel[p_index];
    if (t.r[0] > 0) {
      const int gate = wall_margin + rand() % (t.room_size[1] - 2*wall_margin - corridor_margin);
      int x = t.room_r[0] - 1;
      int y = ((t.room_r[1] + gate + corridor_margin)/corridor_margin)*corridor_margin;
      while (1) {
        if (get_board(board, x, y) == CORRIDOR) break;
        set_board(board, x, y, CORRIDOR);
        x--;
      }
    }

    if (t.r[0] + t.size[0] < board -> width) {
      const int gate = wall_margin + rand() % (t.room_size[1] - 2*wall_margin - corridor_margin);
      int x = t.room_r[0] + t.room_size[0];
      int y = ((t.room_r[1] + gate + corridor_margin)/corridor_margin)*corridor_margin;
      while (1) {
        if (get_board(board, x, y) == CORRIDOR) break;
        set_board(board, x, y, CORRIDOR);
        x++;
      }
    }

    if (t.r[1] > 0) {
      const int gate = wall_margin + rand() % (t.room_size[0] - 2*wall_margin - corridor_margin);
      int x = ((t.room_r[0] + gate + corridor_margin)/corridor_margin)*corridor_margin;
      int y = t.room_r[1] - 1;
      while (1) {
        if (get_board(board, x, y) == CORRIDOR) break;
        set_board(board, x, y, CORRIDOR);
        y--;
      }
    }

    if (t.r[1] + t.size[1] < board -> height) {
      const int gate = wall_margin + rand() % (t.room_size[0] - 2*wall_margin - corridor_margin);
      int x = ((t.room_r[0] + gate + corridor_margin)/corridor_margin)*corridor_margin;
      int y = t.room_r[1] + t.room_size[1];
      while (1) {
        if (get_board(board, x, y) == CORRIDOR) break;
        set_board(board, x, y, CORRIDOR);
        y++;
      }
    }
  }
}

static int register_node(NODE* node, const BOARD* board) {
  int n_index = 0;
  for (int x=0; x < board -> width; x++) {
    for (int y=0; y < board -> height; y++) {
      if (get_board(board, x, y) != CORRIDOR) continue;

      if (count_harline(x, y, board) == 1) {
        node[n_index].r[0] = x;
        node[n_index].r[1] = y;

        if (x <= 1                  ) node[n_index].type = NODE_CROSS;
        if (x == board -> width - 1 ) node[n_index].type = NODE_CROSS;
        if (y <= 1                  ) node[n_index].type = NODE_CROSS;
        if (y == board -> height - 1) node[n_index].type = NODE_CROSS;

        if (node[n_index].type != NODE_CROSS) {
          if (get_board(board, x+1, y) >= 0) node[n_index].type = get_board(board, x+1, y);
          if (get_board(board, x-1, y) >= 0) node[n_index].type = get_board(board, x-1, y);
          if (get_board(board, x, y+1) >= 0) node[n_index].type = get_board(board, x, y+1);
          if (get_board(board, x, y-1) >= 0) node[n_index].type = get_board(board, x, y-1);
        }
        n_index++;

      } else if (count_harline(x, y, board) > 2) {
        node[n_index].type = NODE_CROSS;
        node[n_index].r[0] = x;
        node[n_index].r[1] = y;
        n_index++;
      }
    }
  }
  return n_index;
}

static int count_harline(const int x, const int y, const BOARD* board) {
  int sum = 0;
  if (x < board -> width - 1  && get_board(board, (x+1), y) == CORRIDOR) sum++;
  if (x > 0                   && get_board(board, (x-1), y) == CORRIDOR) sum++;
  if (y < board -> height - 1 && get_board(board, x, (y+1)) == CORRIDOR) sum++;
  if (y > 0                   && get_board(board, x, (y-1)) == CORRIDOR) sum++;
  return sum;
}

static void connect_node(NODE* node, const int n_used_index, const BOARD* board) {
  for (int n_index=0; n_index<n_used_index; n_index++) {
    NODE *n;
    n = &(node[n_index]);

    for (int around=0; around<4; around++) {
      int d[2];
      if      (around == 0) d[0] = 1;
      else if (around == 1) d[0] = -1;
      else                  d[0] = 0;

      if      (around == 2) d[1] = 1;
      else if (around == 3) d[1] = -1;
      else                  d[1] = 0;

      int r[2];
      r[0] = n -> r[0] + d[0];
      r[1] = n -> r[1] + d[1];
      if (r[0] < 0) continue;
      if (r[1] < 0) continue;
      if (r[0] >= board -> width)  continue;
      if (r[1] >= board -> height) continue;

      if (get_board(board, r[0], r[1]) != CORRIDOR) continue;

      while (1) {
        if (count_harline(r[0], r[1], board) != 2) break;
        r[0] += d[0];
        r[1] += d[1];
      }

      for (int search_n_index=0; search_n_index<n_used_index; search_n_index++) {
        NODE *s;
        s = &(node[search_n_index]);
        if (s -> r[0] == r[0] && s -> r[1] == r[1]) {
          n -> edge[around] = s;
          break;
        }
      }
    }
  }
}

static void eliminate_node(NODE* eliminate, BOARD* board) {
  for (int around=0; around<4; around++) {
    if (eliminate -> edge [around] == NULL) continue;
    eliminate_edge(eliminate, eliminate -> edge[around], board);
  }

  eliminate -> type = NODE_UNUSED;
  if (eliminate -> r[0] < 0 || eliminate -> r[1] < 0) return;

  set_board(board, eliminate -> r[0], eliminate ->  r[1], BLANK);
  eliminate -> r[0] = -1;
  eliminate -> r[1] = -1;
}

static void eliminate_edge(NODE* end1, NODE* end2, BOARD* board) {
  for (int around=0; around<4; around++) {
    if (end1 -> edge[around] == end2) end1 -> edge[around] = NULL;
    if (end2 -> edge[around] == end1) end2 -> edge[around] = NULL;
  }

  if (end1 -> r[0] == end1 -> r[0]) {
    const int x = end1 -> r[0];
    const int min = (end1 -> r[1] < end2 -> r[1]) ? end1 -> r[1] : end2 -> r[1];
    const int max = (end1 -> r[1] < end2 -> r[1]) ? end2 -> r[1] : end1 -> r[1];
    for (int y=min+1; y<max; y++) {
      set_board(board, x, y, BLANK);
    }
  }

  if (end1 -> r[1] == end1 -> r[1]) {
    const int y = end1 -> r[1];
    const int min = (end1 -> r[0] < end2 -> r[0]) ? end1 -> r[0] : end2 -> r[0];
    const int max = (end1 -> r[0] < end2 -> r[0]) ? end2 -> r[0] : end1 -> r[0];
    for (int x=min+1; x<max; x++) {
      set_board(board, x, y, BLANK);
    }
  }

  if        (end1 -> type == NODE_CROSS && count_edge(end1) < 2 ) {
    eliminate_node(end1, board);
  } else if (end1 -> type != NODE_CROSS && count_edge(end1) == 0) {
    eliminate_node(end1, board);
  }

  if      (end2 -> type == NODE_CROSS && count_edge(end2) < 2 ) {
    eliminate_node(end2, board);
  } else if (end2 -> type != NODE_CROSS && count_edge(end2) == 0) {
    eliminate_node(end2, board);
  }
}

static int count_edge(NODE* n) {
  int total = 0;
  for (int around=0; around<4; around++) {
    if (n -> edge[around] != NULL) total++;
  }
  return total;
}

static void bfs(QUEUE* queue, const int node_index_max) {
  if (queue -> back == queue -> front) return;

  NODE *current = dequeue(queue, node_index_max);
  if (current -> length > BFS_MAX_DEPTH) return;

  for (int around=0; around<4; around++) {
    NODE *next = current -> edge[around];
    if (next == NULL) continue;
    if (next -> type != NODE_CROSS) continue;

    if (next -> length > current -> length) {
      next -> length = current -> length + 1;
      next -> from   = current;
      enqueue(next, queue, node_index_max);
    }
  }

  bfs(queue, node_index_max);
}

static void enqueue(NODE* node, QUEUE* queue, const int node_index_max) {
  queue -> buff[queue -> front] = node;
  queue -> front = (queue -> front + 1) % node_index_max;
}

static NODE* dequeue(QUEUE* queue, const int node_index_max) {
  NODE *back_node = queue -> buff[queue -> back];
  queue -> back = (queue -> back + 1) % node_index_max;
  return back_node;
}

static NODE* get_loop_goal(const int n_gate_index, const int n_used_index, NODE* node, BOARD* board, const int node_index_max) {
  for (int n_index=0; n_index<n_used_index; n_index++) {
    node[n_index].length = node_index_max;
    node[n_index].from   = NULL;
  }

  NODE* start = &(node[n_gate_index]);
  start -> length = 0;

  QUEUE queue;
  queue.buff = (NODE**) malloc(sizeof(NODE)*node_index_max);

  initialize_queue(&queue, node_index_max);
  enqueue(start, &queue, node_index_max);

  bfs(&queue, node_index_max);

  NODE* goal = NULL;
  for (int n_index=0; n_index<n_used_index; n_index++) {
    if (goal != NULL) break;
    if (n_index == n_gate_index) continue;

    NODE* tmp_goal = &(node[n_index]);
    if (start -> type != tmp_goal -> type) continue;

    for (int around=0; around<4; around++) {
      if (goal != NULL) break;
      NODE* prev_goal = tmp_goal -> edge[around];
      if (prev_goal == NULL) continue;
      if (prev_goal -> from == NULL) continue;
      if (prev_goal -> type != CORRIDOR) continue;

      goal = tmp_goal;
      goal -> length = prev_goal -> length + 1;
      goal -> from   = prev_goal;
    }
  }

  free(queue.buff);
  return goal;
}

static void eliminate_loop(NODE* goal, BOARD* board) {
  if (goal == NULL) return;

  int delete_edge_count = rand() % (goal -> length);
  NODE *delete_node = goal;
  while (delete_edge_count > 0) {
    delete_node = delete_node -> from;
    delete_edge_count--;
  }
  eliminate_edge(delete_node, delete_node -> from, board);
}

static void eliminate_all_loop(const int p_used_index, const int n_used_index, NODE* node, BOARD* board, const int node_index_max) {
  while (1) { // delete room loop
    bool end_flag = true;
    for (int n_index=0; n_index<n_used_index; n_index++) {
      if (node[n_index].type < 0) continue;
      NODE *goal = get_loop_goal(n_index, n_used_index, node, board, node_index_max);
      if (goal != NULL) {
        eliminate_loop(goal, board);
        end_flag = false;
      }
    }
    if (end_flag) break;
  }
}

static void set_board(BOARD* board, const int x, const int y, const int value) {
  board -> board[x*(board -> height) + y] = value;
}

static int get_board(const BOARD* board, const int x, const int y) {
  return board -> board[x*(board -> height) + y];
}
