/* header file for Miner */

#include <Paul.h>


#define NAME       "Miner"
#define VERSION    "6"

#define SNAMELEN   22
#define SPERLEVEL  3

/* For future somethingorother, BOARDMAX_X should be a multiple of 8. */
#define BOARDMAX_X 80
#define BOARDMAX_Y 120

typedef enum {
    beginner, interm, expert, custom1, custom2, custom3, LEVELS
} LEVEL;

#define CUSLEVELS (LEVELS - custom1)

typedef struct {
    ushort time;
    char name[SNAMELEN];
} SCORE;

#define WORST     { 9999, "(nobody)" }


import struct Screen *scr;
import struct Window *bgw;

import LEVEL lvl;

import ushort boardsizes[LEVELS][2];
import ushort boardmines[LEVELS];
import ubyte board[BOARDMAX_X][BOARDMAX_Y];
import str levelabels[LEVELS];

import SCORE highscores[LEVELS][SPERLEVEL];

/* these are state bits in board[x][y]: */
#define MINED     1
#define TRIED     2
#define FLAGGED   4
#define TRY_ZERO  8
#define QUESSED   16
/* TRIED, FLAGGED, and QUESSED are mutually exclusive.  TRIED and MINED */
/* are mutually exclusive.  TRY_ZERO is only set if TRIED is also set.  */


/* these constants tell how to render a square.  Values 0 through 8 */
/* are used for tried squares with that many surrounding bombs.     */

#define BLANK     9
#define FLAG      10
#define BOMB      11
#define HITBOMB   12
#define QUESTION  13


/* constants related to visual layout... colors first: */

#define REQTEXT       2
#define REQBACK       4

#define LEFTMARGIN    70

#define LSQUAREWIDTH  19
#define LSQUAREHEIGHT 9
#define BSQUAREWIDTH  24
#define BSQUAREHEIGHT 11


/* these numbers correspond to the commands the user can give: */

#define C_NEW         0
#define C_PAUSE       1
#define C_SCORES      2
#ifdef RESETS
#  define C_RESET     3
#  define C_ABOUT     5
#  define C_QUIT      6
#else
#  define C_ABOUT     4
#  define C_QUIT      5
#endif

#define C_BEGINNER    10
#define C_INTERMED    11
#define C_EXPERT      12
#define C_CUSTOM1     13
#define C_CUSTOM2     14
#define C_CUSTOM3     15
#define C_ADJUST1     16
#define C_ADJUST2     17
#define C_ADJUST3     18

#define C_MODE        20
#define C_SOUND       21
#define C_BEATCLOCK   22
#define C_QUES        23
#define C_FLAGWIN     24


/* These constants denote different events that might have sound effects: */

#define SOUND_BOOM    0
#define SOUND_VICTORY 1
#define SOUND_TEST    2
#define SOUND_NTEST   3		/* if undefined, use TEST */
#define SOUND_RIPPLE  4		/* if undefined, use TEST */
#define SOUND_FLAG    5
/* any or all of these may have no sound file supplied. */
#define SOUND_COUNT   6


int sprintf(char *_s, const char *_format, ...);
