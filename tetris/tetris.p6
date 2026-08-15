/*
 * Tetris in P6.
 *
 * Everything below is plain P6, compiled by the p6 compiler in this repository. Reading keys, the
 * clock and random numbers use the language's built-ins key(ms), clock and random(n), which the
 * compiler turns into calls to its runtime (runtime/p6.c). Drawing is printing strings with ANSI
 * escape codes.
 *
 * P6 integers are 40-trit balanced ternary, and every operation on them goes through the runtime's
 * software ternary ALU: an addition or comparison costs thousands of x86 instructions, a division
 * over a million. So the game is written to do as little integer arithmetic as possible:
 *   - no division at all (rotations are precomputed offset tables, built with subtraction);
 *   - the well is padded with walls and a floor, so collision tests need no bounds checks;
 *   - rows are pointers, so clearing a line shifts pointers instead of copying cells;
 *   - the ghost piece comes from column heights instead of trial drops;
 *   - only cells that change are redrawn, by moving the cursor to them.
 *
 * Controls: arrows or WASD to move, up or W to rotate, Z to rotate back, space to drop,
 * P to pause, R to restart after a game over, Q or Escape to quit.
 */

/* What the player asked for (see action). */
int kLeft = 1;
int kRight = 2;
int kRotate = 3;
int kSoft = 4;
int kHard = 5;
int kQuit = 6;
int kPause = 7;
int kRotateBack = 8;
int kRestart = 9;

/*
 * The well: 23 rows of 14 cells. Rows 0-1 are hidden headroom, 2-21 are visible and 22 is the
 * floor; columns 0-1 and 12-13 are walls, two deep so that no wall kick can reach past a row's
 * ends. A cell is 0 when empty, a piece number + 1, or 8 for wall and floor.
 */
[[int]] row;
[int] rowfill;  // filled cells in each row
[int] top;      // the highest filled row in each column (22, the floor, when empty)

/* Cell i of piece p turned r times is (offx[k], offy[k]) from the piece's corner, k = 16p + 4r + i. */
[int] offx;
[int] offy;
[string] colour;

/* A 7-bag: every piece once, shuffled, before any repeats. */
[int] bag;
int bagpos = 7;

int cur = 0;
int nxt = 0;
int rot = 0;
int px = 0;
int py = 0;

/* The piece as drawn on screen: its cells, and how far below them the ghost is. */
[int] pcx;
[int] pcy;
int gd = 0;

int score = 0;
int lines = 0;
int level = 0;
int interval = 800;
int over = 0;
int paused = 0;

//---------------------------------------------------------------------------
// Pieces
//---------------------------------------------------------------------------

setshape(int p, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) -> void {
  int k = p * 16;
  offx[k] = x0;
  offy[k] = y0;
  offx[k + 1] = x1;
  offy[k + 1] = y1;
  offx[k + 2] = x2;
  offy[k + 2] = y2;
  offx[k + 3] = x3;
  offy[k + 3] = y3;
}

/* Fills turns 1-3 of piece p from turn 0, turning clockwise inside an n-by-n box (n = 0: no turning). */
turns(int p, int n) -> void {
  int k = p * 16 + 4;
  int last = p * 16 + 16;
  while (k < last) {
    if (n == 0) {
      offx[k] = offx[k - 4];
      offy[k] = offy[k - 4];
    } else {
      offx[k] = n - 1 - offy[k - 4];
      offy[k] = offx[k - 4];
    }
    k = k + 1;
  }
}

/* 1 if piece p, turned r times, fits with its corner at (x, y). */
fits(int p, int r, int x, int y) -> int {
  int k = p * 16 + r * 4;
  int last = k + 4;
  while (k < last) {
    if (row[y + offy[k]][x + offx[k]] != 0) return 0;
    k = k + 1;
  }
  return 1;
}

nextpiece() -> int {
  int i = 0;
  int j = 0;
  int t = 0;
  if (bagpos >= 7) {
    while (i < 7) {
      bag[i] = i;
      i = i + 1;
    }
    i = 6;
    while (i > 0) {
      j = random(i + 1);
      t = bag[i];
      bag[i] = bag[j];
      bag[j] = t;
      i = i - 1;
    }
    bagpos = 0;
  }
  bagpos = bagpos + 1;
  return bag[bagpos - 1];
}

//---------------------------------------------------------------------------
// Drawing
//---------------------------------------------------------------------------

/* Moves the cursor to well cell (x, y): screen line y + 4, column 2x + 11. */
cursor(int x, int y) -> void {
  "\33[", y + 4, ";", x + x + 11, "H" !
}

/* Paints one cell: v > 0 a piece colour, v < 0 the ghost, 0 empty. Hidden rows are skipped. */
paint(int x, int y, int v) -> void {
  if (y > 1) {
    cursor(x, y);
    if (v > 0) {
      colour[v - 1] !
      "  \33[0m" !
    } elif (v < 0) "\33[48;5;237m  \33[0m" !
    else "\33[38;5;238m ·\33[0m" !
  }
}

/* Redraws well rows a to b. */
wellrows(int a, int b) -> void {
  int x = 0;
  int v = 0;
  [int] r = null;
  while (a <= b) {
    cursor(2, a);
    r = row[a];
    x = 2;
    while (x < 12) {
      v = r[x];
      if (v > 0) {
        colour[v - 1] !
        "  \33[0m" !
      } else "\33[38;5;238m ·\33[0m" !
      x = x + 1;
    }
    a = a + 1;
  }
}

/* How far the piece can drop, from the column heights (or by trial when it is tucked under a ledge). */
ghost() -> int {
  int d = 99;
  int i = 0;
  int t = 0;
  while (i < 4) {
    t = top[pcx[i]] - pcy[i] - 1;
    if (t < d) d = t;
    i = i + 1;
  }
  if (d < 0) {
    d = 0;
    while (fits(cur, rot, px, py + d + 1) == 1) d = d + 1;
  }
  return d;
}

erase() -> void {
  int i = 0;
  while (i < 4) {
    paint(pcx[i], pcy[i] + gd, 0);
    paint(pcx[i], pcy[i], 0);
    i = i + 1;
  }
}

/* Records the piece's cells and draws it with its ghost. */
show() -> void {
  int i = 0;
  int k = cur * 16 + rot * 4;
  while (i < 4) {
    pcx[i] = px + offx[k + i];
    pcy[i] = py + offy[k + i];
    i = i + 1;
  }
  gd = ghost();
  i = 0;
  if (gd > 0) {
    while (i < 4) {
      paint(pcx[i], pcy[i] + gd, -1);
      i = i + 1;
    }
  }
  i = 0;
  while (i < 4) {
    paint(pcx[i], pcy[i], cur + 1);
    i = i + 1;
  }
}

/* The next-piece box. */
preview() -> void {
  int i = 0;
  int k = nxt * 16;
  "\33[8;39H        \33[9;39H        " !
  while (i < 4) {
    "\33[", offy[k + i] + 8, ";", offx[k + i] + offx[k + i] + 39, "H" !
    colour[nxt] !
    "  \33[0m" !
    i = i + 1;
  }
}

stats() -> void {
  "\33[8;4H       \33[8;4H", score !
  "\33[13;4H       \33[13;4H", level !
  "\33[18;4H       \33[18;4H", lines !
}

banner(string a, string b) -> void {
  "\33[14;15H", a !
  "\33[16;15H", b !
}

/* The whole screen: frame, boxes, an empty well and the controls under it. */
frame() -> void {
  int r = 0;
  "\33[2J\33[H" !!
  " ┌────────────────────────────────────────────┐" !!
  " │          Tetris Implemented in P6          │" !!
  " └────────────────────────────────────────────┘" !!
  "             ┌────────────────────┐" !!
  " ┌────────┐  │                    │  ┌────────┐" !!
  " │ Score: │  │                    │  │ Next:  │" !!
  " │        │  │                    │  │        │" !!
  " └────────┘  │                    │  │        │" !!
  "             │                    │  └────────┘" !!
  " ┌────────┐  │                    │" !!
  " │ Level: │  │                    │" !!
  " │        │  │                    │" !!
  " └────────┘  │                    │" !!
  "             │                    │" !!
  " ┌────────┐  │                    │" !!
  " │ Lines: │  │                    │" !!
  " │        │  │                    │" !!
  " └────────┘  │                    │" !!
  while (r < 6) {
    "             │                    │" !!
    r = r + 1;
  }
  "             └────────────────────┘" !!
  "" !!
  "\33[38;5;245m ←→ move       ↑ rotate         z rotate back" !!
  " ↓ soft drop   space hard drop  p pause q quit\33[0m" !
}

//---------------------------------------------------------------------------
// Rules
//---------------------------------------------------------------------------

/* Brings in the next piece; the game is over when it has no room. */
spawn() -> void {
  cur = nxt;
  nxt = nextpiece();
  rot = 0;
  px = 5;
  py = 2;
  if (cur == 0) py = 1; // the I piece sits on the second row of its box
  preview();
  if (fits(cur, rot, px, py) == 0) {
    over = 1;
    banner("     GAME  OVER     ", "  r: again  q: quit ");
  } else show();
}

/* Removes row y: the rows above move down one, and an empty row goes in at the top. */
removerow(int y) -> void {
  [int] gone = row[y];
  int x = 2;
  while (y > 0) {
    row[y] = row[y - 1];
    rowfill[y] = rowfill[y - 1];
    y = y - 1;
  }
  while (x < 12) {
    gone[x] = 0;
    x = x + 1;
  }
  row[0] = gone;
  rowfill[0] = 0;
}

/* Writes the piece into the well, clears full rows and scores them. */
settle() -> void {
  int i = 0;
  int n = 0;
  int y = 21;
  int x = 2;
  while (i < 4) {
    row[pcy[i]][pcx[i]] = cur + 1;
    rowfill[pcy[i]] = rowfill[pcy[i]] + 1;
    if (pcy[i] < top[pcx[i]]) top[pcx[i]] = pcy[i];
    if (pcy[i] < 2) over = 1;
    i = i + 1;
  }

  while (y > 1) {
    if (rowfill[y] == 10) {
      removerow(y);
      n = n + 1;
    } else y = y - 1;
  }

  if (n > 0) {
    while (x < 12) {
      y = 2;
      while (row[y][x] == 0) y = y + 1;
      top[x] = y;
      x = x + 1;
    }
    wellrows(2, 21);
    if (n == 1) score = score + 100 * (level + 1);
    elif (n == 2) score = score + 300 * (level + 1);
    elif (n == 3) score = score + 500 * (level + 1);
    else score = score + 800 * (level + 1);
    lines = lines + n;
    while (lines >= (level + 1) * 10) {
      level = level + 1;
      interval = interval - 70;
    }
    if (interval < 80) interval = 80;
  }
  stats();

  if (over == 1) banner("     GAME  OVER     ", "  r: again  q: quit ");
  else spawn();
}

/* One step down, or settle when there is no room. Returns 1 if the piece moved. */
fall() -> int {
  if (gd > 0) {
    erase();
    py = py + 1;
    show();
    return 1;
  }
  settle();
  return 0;
}

harddrop() -> void {
  int d = gd;
  erase();
  py = py + d;
  score = score + d + d;
  show();
  settle();
}

/* Moves to (x, y) turned r times, if the piece fits there. */
moveto(int r, int x, int y) -> int {
  if (x < 0 || x > 11 || y < 0) return 0;
  if (fits(cur, r, x, y) == 0) return 0;
  erase();
  rot = r;
  px = x;
  py = y;
  show();
  return 1;
}

/* Turns by d quarter turns, nudging sideways (0, -1, +1, -2, +2) or up when blocked. */
turn(int d) -> void {
  int r = rot + d;
  if (r > 3) r = r - 4;
  if (moveto(r, px, py) == 1) return;
  if (moveto(r, px - 1, py) == 1) return;
  if (moveto(r, px + 1, py) == 1) return;
  if (moveto(r, px - 2, py) == 1) return;
  if (moveto(r, px + 2, py) == 1) return;
  moveto(r, px, py - 1);
}

newgame() -> void {
  int y = 0;
  int x = 0;
  [int] r = null;
  while (y < 23) {
    r = row[y];
    r[0] = 8;
    r[1] = 8;
    r[12] = 8;
    r[13] = 8;
    x = 2;
    while (x < 12) {
      if (y == 22) r[x] = 8;
      else r[x] = 0;
      x = x + 1;
    }
    rowfill[y] = 0;
    y = y + 1;
  }
  x = 0;
  while (x < 14) {
    top[x] = 22;
    x = x + 1;
  }
  score = 0;
  lines = 0;
  level = 0;
  interval = 800;
  over = 0;
  paused = 0;
  bagpos = 7;
  frame();
  wellrows(2, 21);
  stats();
  nxt = nextpiece();
  spawn();
}

/* The action for a key, from the code key() returns: a character's code, or negative for an arrow. */
action(int c) -> int {
  if (c == -4 || c == 97) return kLeft;                    // left arrow, a
  if (c == -3 || c == 100) return kRight;                  // right arrow, d
  if (c == -1 || c == 119) return kRotate;                 // up arrow, w
  if (c == 122) return kRotateBack;                        // z
  if (c == -2 || c == 115) return kSoft;                   // down arrow, s
  if (c == 32) return kHard;                               // space
  if (c == 112) return kPause;                             // p
  if (c == 114) return kRestart;                           // r
  if (c == 113 || c == 27 || c == 3 || c == 4) return kQuit; // q, Escape, Ctrl-C, end of input
  return 0;
}

//---------------------------------------------------------------------------
// Setup and main loop
//---------------------------------------------------------------------------

setup() -> void {
  setshape(0, 0, 1, 1, 1, 2, 1, 3, 1); // I
  setshape(1, 1, 0, 2, 0, 1, 1, 2, 1); // O
  setshape(2, 1, 0, 0, 1, 1, 1, 2, 1); // T
  setshape(3, 1, 0, 2, 0, 0, 1, 1, 1); // S
  setshape(4, 0, 0, 1, 0, 1, 1, 2, 1); // Z
  setshape(5, 0, 0, 0, 1, 1, 1, 2, 1); // J
  setshape(6, 2, 0, 0, 1, 1, 1, 2, 1); // L
  turns(0, 4);
  turns(1, 0);
  turns(2, 3);
  turns(3, 3);
  turns(4, 3);
  turns(5, 3);
  turns(6, 3);

  colour[0] = "\33[48;5;81m";  // cyan
  colour[1] = "\33[48;5;221m"; // yellow
  colour[2] = "\33[48;5;141m"; // purple
  colour[3] = "\33[48;5;114m"; // green
  colour[4] = "\33[48;5;203m"; // red
  colour[5] = "\33[48;5;75m";  // blue
  colour[6] = "\33[48;5;215m"; // orange
}

begin
  int k = 0;
  int t = 0;
  int due = 0;
  int quit = 0;
  int y = 0;

  row = [23];
  while (y < 23) {
    row[y] = [14];
    y = y + 1;
  }
  rowfill = [23];
  top = [14];
  offx = [112];
  offy = [112];
  colour = [7];
  bag = [7];
  pcx = [4];
  pcy = [4];

  setup();
  "\33[?1049h\33[?25l" !
  newgame();
  due = clock + interval;

  while (quit == 0) {
    t = due - clock;
    if (over == 1 || paused == 1) t = -1; // nothing falls: wait for a key
    elif (t < 0) t = 0;
    k = action(key(t));

    if (k == kQuit) quit = 1;
    elif (over == 1) {
      if (k == kRestart) {
        newgame();
        due = clock + interval;
      }
    } elif (k == kPause) {
      paused = 1 - paused;
      if (paused == 1) banner("                    ", "       PAUSED       ");
      else {
        wellrows(2, 21);
        show();
        due = clock + interval;
      }
    } elif (paused == 0) {
      if (k == kLeft) moveto(rot, px - 1, py);
      elif (k == kRight) moveto(rot, px + 1, py);
      elif (k == kRotate) turn(1);
      elif (k == kRotateBack) turn(3);
      elif (k == kSoft) {
        if (fall() == 1) score = score + 1;
        due = clock + interval;
      } elif (k == kHard) {
        harddrop();
        due = clock + interval;
      }
      if (over == 0 && clock >= due) {
        fall();
        due = clock + interval;
      }
    }
  }

  "\33[0m\33[?25h\33[?1049l" !
  "Final score: ", score !!
end
