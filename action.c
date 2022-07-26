/* event loop and responses to user actions for Miner */

#include <exec/interrupts.h>
#include <graphics/gfxmacros.h>
#include <intuition/intuitionbase.h>
#include <ctype.h>
#include "miner.h"


void NoticeNewLevel(LEVEL ll);
void PlunkSquareImage(short sx, short sy, ushort which);
void PlunkBignum(short x, short y, bool redness, short value);
void FillInBlankBoard(void);
void PutBombsInIt(void);
void FreshenGadgets(void);
void SetPauseGadget(bool on);
void ElevateScore(ushort seconds);
void ShowHighScores(void);
void Customize(LEVEL ll);
void PlantARandomMine(void);
void Error(str text, ...);
#ifdef RESETS
void ResetHighScores(void);
#endif
bool ChangeScreenMode(void);
bool StartSound(ushort which, bool waitfinish, ushort volume);
void ReleaseChannels(void);


typedef bool (*IntuiHandler)(struct IntuiMessage *);


import struct MenuItem mo1, mo2, mo3, mo4, *(lvlitem[LEVELS]);
import APTR myvi;
import struct RastPort *bp;
import struct TextAttr topaz9;

import struct TagItem notags[1];
import UWORD background[16];
import ushort mypalette[8];

import ushort width, height, mines;
import bool beater, fallback, anyaudio;

import struct IntuitionBase *IntuitionBase;


#define ABOUTLINES 12
#define ABOUTWIDTH 550

char abtxt[ABOUTLINES][56] = {
    "   " NAME " v" VERSION ":  freeware by Paul Kienitz, " __DATE__,
    "          Dedicated to Jay Miner, of course.",
    "",
    "The object of the game is to click on every square that",
    "does NOT have a mine in it.  If you click on a mined",
    "square, you lose.  (BOOM!)  When you click on a safe",
    "square, a number appears in it showing how many of the",
    "eight squares next to it have mines.  Clicking the",
    "right mouse button on a square marks it with a flag as",
    "a place where you think a mine is.  Use the menu to",
    "select board size.  The three best times for each board",
    "size are remembered in the high scores file."
};

#define ABOUT(n) {REQTEXT,REQBACK,JAM2,10,10*n+15,null,abtxt[n],&rtabout[n+1]}

struct IntuiText rtabout[ABOUTLINES] = {
    ABOUT(0), ABOUT(1), ABOUT(2), ABOUT(3), ABOUT(4), ABOUT(5),
    ABOUT(6), ABOUT(7), ABOUT(8), ABOUT(9), ABOUT(10),
    { REQTEXT, REQBACK, JAM2, 10, 10 * 11 + 15, null, abtxt[11], null }
};

char pausestring[60];

struct IntuiText rtpause = {
    REQTEXT, REQBACK, JAM2, 150, 90, null, pausestring, null
};

struct IntuiText rtclick = {
    7, REQBACK, JAM2, 150, 150, null,
    "-- CLICK MOUSE OR PRESS ANY KEY TO CONTINUE --", null
};


#ifdef AUTOFLASH

import struct Process *me;

/* struct ColorMap *ourcm; */

typedef short Pair[2];

/* In these arrays, a pair beginning with -1 terminates the  */
/* list.  The second number in the pair is a count of vblank */
/* intervals and must always be greater than zero.           */

Pair loseflash[] = {
    { 0xF11, 1}, { 0xD77, 5 }, { -1, 12 }
};

Pair winflash[] = {
    { 0x5C5, 5 }, { 0x77D, 5 }, { 0xD77, 5 },
    { 0x7B7, 5 }, { 0x99C, 5 }, { 0xC99, 5 },
    { 0x7B7, 5 }, { 0x99C, 5 }, { 0xC99, 5 },
    { 0x5C5, 5 }, { 0x77D, 5 }, { 0xD77, 5 },
    { 0x3D3, 5 }, { 0x66F, 5 }, { 0xF66, 5 },
    { 0x3D3, 5 }, { 0x66F, 5 }, { 0xF66, 5 },
    { 0x0F0, 5 }, { -1, 8 }	/* approximately two seconds total */
};

volatile Pair *curtable = null;
volatile ushort curtabix, ticksofar, whichpen, resetshade;

void Rupt(void);		/* assembly func */

struct Interrupt flasher = {
    { null, null, NT_INTERRUPT, 6, "Miner screen flasher" },	/* node */
    null, Rupt
};

private ulong boomseconds = 0, boommicros;

#endif AUTOFLASH


short boardleft, boardtop, boardright, boardbot, minesleft, trycount;
ushort squarewidth = BSQUAREWIDTH, squareheight = BSQUAREHEIGHT, seconds;

bool playing = false, finished = false, clocking = false, pushbomb = false;
bool countdown = false, questioning = false, stifle = false, silence = false;
bool flagwin = false, littlesquares, whoosh;

private ulong startsecs;
private long startmicros;

/* -------------------------------------------------------------------- */


void CountASquare(short x, short y);	/* forward reference */


#ifdef AUTOFLASH
#  asm
	xref	_curtable
;;;;	xref	_RuptWork
	xref	_me
	xref	_geta4
	xref	_LVOSignal
	xdef	_Rupt

_Rupt:	movem.l	a4/a6,-(sp)
	jsr	_geta4
	tst.l	_curtable
	beq.s	out
;;;;	bsr	_RuptWork	; this function does the actual work
	move.l	_me,a1		; NO, make main process call it!
	move.l	#$4000,d0	; SIGBREAKF_CTRL_E
	jsr	_LVOSignal(a6)
out:	movem.l	(sp)+,a4/a6
	moveq	#0,d0		; Z flag MUST be set on exit
	rts
#  endasm


void RuptWork(void)	/* called once per vblank if curtable is non-null */
{
    static short was = -1, willbe, steps, sr, sg, sb, er, eg, eb;
    static bool lastentry = false;
    ushort togo, r, g, b;

    if (was == -1) {			/* just starting a new table */
	curtabix = steps = ticksofar = 0;
#ifndef ONEPEN
	whichpen = 0;
#endif
	willbe = resetshade = mypalette[whichpen];
    }
    if (++ticksofar > steps) {		/* starting a new entry in the table */
	was = willbe;
	ticksofar = 1;
	if (lastentry) {		/* no further entries -- quit */
	    curtable = null;
	    lastentry = false;
	    was = -1;
#ifdef ONEPEN
	    if (whichpen == 0)		/* in case of DisplayBeep */
#endif
		CurrentTime(&boomseconds, &boommicros);
	    return;
	}
	steps = curtable[curtabix][1];	/* must always be greater than zero */
	if ((willbe = curtable[curtabix][0]) == -1) {      /* final entry */
	    willbe = resetshade;
	    lastentry = true;
	}
	sr = was >> 8, sg = (was >> 4) & 0xF, sb = was & 0xF;
	er = willbe >> 8, eg = (willbe >> 4) & 0xF, eb = willbe & 0xF;
	curtabix++;
    }
    togo = steps - ticksofar;
    r = (er * ticksofar + sr * togo + 1) / steps;	/* +1 for dither */
    g = (eg * ticksofar + sg * togo) / steps;
    b = (eb * ticksofar + sb * togo + 2) / steps;	/* +2 for dither */
#ifndef ONEPEN
    SetRGB4(&scr->ViewPort, 5, r - 1, g - 1, b - 1);
#endif
    SetRGB4(&scr->ViewPort, whichpen, r, g, b);
/*    SetRGB4CM(ourcm, whichpen, r, g, b);			*/
/*    MakeVPort(&IntuitionBase->ViewLord, &scr->ViewPort);	*/
/*    MrgCop(&IntuitionBase->ViewLord);				*/
}


#  define StartColorFlash(pen, table)  (whichpen = pen, curtable = table)


void MyWaitPort(struct MsgPort *p)
{
    ulong sigz, s, m, pf = bit(p->mp_SigBit);
    for (;;) {
	if (boomseconds) {
	    CurrentTime(&s, &m);
	    if (s > boomseconds + 1 + (m < boommicros)) {
		ushort c = mypalette[0];
		SetRGB4(&scr->ViewPort, 0, c >> 8, (c >> 4) & 0xF, c & 0xF);
		boomseconds = 0;
	    }
	}
	if (p->mp_MsgList.lh_Head->ln_Succ)	/* any message waiting? */
	    return;
	sigz = Wait(pf | SIGBREAKF_CTRL_E);
	if (sigz & SIGBREAKF_CTRL_E)
	    RuptWork();
    }
}

#else /* AUTOFLASH */

private void SwingColor(ushort which, ushort willbe, ushort steps)
{
    ushort i, ni, r, g, b, was = GetRGB4(scr->ViewPort.ColorMap, which);
    ushort sr = was >> 8, sg = (was >> 4) & 0xF, sb = was & 0xF;
    ushort er = willbe >> 8, eg = (willbe >> 4) & 0xF, eb = willbe & 0xF;

    for (i = 1; i <= steps; i++) {
	ni = steps - i;
	r = (er * i + sr * ni + 1) / steps;	/* dither */
	g = (eg * i + sg * ni) / steps;
	b = (eb * i + sb * ni + 2) / steps;	/* dither */
	WaitTOF();
	SetRGB4(&scr->ViewPort, which, r, g, b);
    }
}

void MyWaitPort(struct MsgPort *p)
{ WaitPort(p); }

#endif AUTOFLASH


void MakeANoise(ushort which)
{
    ushort volume = which >= SOUND_TEST ? 32 : 64;
    if (silence)
	return;
    if (StartSound(which, which == SOUND_VICTORY, volume))
	return;
    if (which == SOUND_RIPPLE || which == SOUND_NTEST)
	StartSound(SOUND_TEST, false, volume);
}


void ShowAllMines(short sx, short sy)
{
    bool onboard = sx >= 0 && sy >= 0;
    short ix, iy, soff = onboard && board[sx][sy] & TRIED;

    finished = true;
    if (onboard)
	for (ix = sx - soff; ix <= sx + soff; ix++)
	    for (iy = sy - soff; iy <= sy + soff; iy++)
		if (ix >= 0 && ix < width && iy >= 0 && iy < height)
		    if ((board[ix][iy] & ~QUESSED) == MINED) {	/* !FLAGGED */
			PlunkSquareImage(ix, iy, HITBOMB);
			board[ix][iy] /* &= ~(MINED | QUESSED) */ = 0;
		    }
    for (ix = 0; ix < width; ix++)
	for (iy = 0; iy < height; iy++) {
#ifdef AUTOFLASH
	    if (SetSignal(0, SIGBREAKF_CTRL_E) & SIGBREAKF_CTRL_E)
		RuptWork();
#endif
	    if (board[ix][iy] & MINED)
		PlunkSquareImage(ix, iy, BOMB);
	}
}


void StartClock(void)
{
    clocking = true;
    CurrentTime(&startsecs, (ulong *) &startmicros);
}


#define StopClock()  (clocking = false)


void StopPlaying(bool boom, short sx, short sy)
{
    playing = false;
    StopClock();
    SetPauseGadget(false);
    OffMenu(bgw, FULLMENUNUM(0, C_PAUSE, NOSUB));
    OnMenu(bgw, FULLMENUNUM(2, C_MODE - 20, NOSUB));
    OnMenu(bgw, FULLMENUNUM(1, C_ADJUST1 - C_BEGINNER, NOSUB));
    if (boom) {
#ifdef AUTOFLASH
	StartColorFlash(0, loseflash);
	if (!stifle)
	    MakeANoise(SOUND_BOOM);
	ShowAllMines(sx, sy);
#else
	ushort oc0 = mypalette[0];
	SwingColor(0, 0xF00, 1);
	ShowAllMines(sx, sy);
	SwingColor(0, 0xD77, 5);
	SwingColor(0, oc0, 12);
#endif
    }
}


#define DrawMineCount()  PlunkBignum(5, 76, minesleft < 0, minesleft)


void DrawTime(void)
{
    long tobeat = highscores[lvl][SPERLEVEL - 1].time - 1;
    PlunkBignum(5, 36, false, seconds);
    if (countdown && seconds > tobeat) {
	StopPlaying(false, -1, -1);
	ShowAllMines(-1, -1);
	beater = true;
	Error("You didn't finish in %ld seconds!  That's\n"
			"the time required to make the high scores\n"
			"list in the %s category.", tobeat, levelabels[lvl]);
	beater = false;
    }
}


void DrawEverything(void)
{
    squarewidth = BSQUAREWIDTH;		/* be optimistic */
    squareheight = BSQUAREHEIGHT;
    boardleft = LEFTMARGIN + ((bgw->Width + 1 - LEFTMARGIN)
					- width * squarewidth) / 2;
    boardtop = (bgw->Height + 1 - height * squareheight) / 2;
    if (littlesquares = (boardleft < LEFTMARGIN || boardtop < 0)) {
	squarewidth = LSQUAREWIDTH;
	squareheight = LSQUAREHEIGHT;
	boardleft = LEFTMARGIN + ((bgw->Width + 1 - LEFTMARGIN)
					- width * squarewidth) / 2;
	boardtop = (bgw->Height + 1 - height * squareheight) / 2;
    }
    boardbot = boardtop + height * squareheight;
    boardright = boardleft + width * squarewidth;
    minesleft = mines;
    seconds = trycount = 0;
    SetDrMd(bp, JAM2);
    SetBPen(bp, 0);			/* background gray */
    SetAPen(bp, 5);			/* slightly darker gray */
    SetAfPt(bp, background, 4);		/* little bomb shapes */
    RectFill(bp, LEFTMARGIN, 0, bgw->Width - 1, bgw->Height - 1);
    SetAfPt(bp, null, 0);
    RectFill(bp, 2, 3, LEFTMARGIN - 3, bgw->Height - 2);
    SetAPen(bp, 1);			/* shadow */
    Move(bp, LEFTMARGIN - 2, 2);
    Draw(bp, 0, 2);
    Draw(bp, 0, bgw->Height - 1);
    Move(bp, 1, bgw->Height - 2);
    Draw(bp, 1, 3);
    SetAPen(bp, 2);			/* shine */
    Move(bp, 1, bgw->Height - 1);
    Draw(bp, LEFTMARGIN - 1, bgw->Height - 1);
    Draw(bp, LEFTMARGIN - 1, 2);
    Move(bp, LEFTMARGIN - 2, bgw->Height - 2);
    Draw(bp, LEFTMARGIN - 2, 3);
    SetDrMd(bp, JAM2);
    Move(bp, 6, 31);
    SetAPen(bp, 2);
    SetBPen(bp, 5);
    Text(bp, " Time:", 6);
    DrawTime();
    Move(bp, 6, 71);
    SetAPen(bp, 2);
    Text(bp, "Mines:", 6);
    DrawMineCount();
    FreshenGadgets();
    FillInBlankBoard();
    if (fallback) {
	fallback = false;
	Error("Could not create selected screen\n"
				"type; using default screen mode.");
    }
}


void Win(void)
{
    short ix, iy;

    stifle = finished = true;
    StopPlaying(false, -1, -1);
    minesleft = 0;
    DrawMineCount();
    for (ix = 0; ix < width; ix++)
	for (iy = 0; iy < height; iy++) {
	    register ubyte q = board[ix][iy];
	    if (q & MINED) {
		if (!(q & FLAGGED))
		    PlunkSquareImage(ix, iy, FLAG);
	    } else if (!(q & TRIED))
		CountASquare(ix, iy);
	}
    stifle = false;
#ifdef AUTOFLASH
    StartColorFlash(5, winflash);
    MakeANoise(SOUND_VICTORY);
#else
    SwingColor(5, 0x3D3, 5);
    SwingColor(5, 0x66F, 5);
    SwingColor(5, 0xF66, 5);
    SwingColor(5, 0x3D3, 5);
    SwingColor(5, 0x66F, 5);
    SwingColor(5, 0xF66, 5);
    SwingColor(5, 0x3D3, 5);
    SwingColor(5, mypalette[5], 6);
#endif
    ElevateScore(seconds);
}


void TryFlagWin(void)
{
    register ushort ix, iy;
    if (minesleft || !flagwin)
	return;
    for (ix = 0; ix < width; ix++)
	for (iy = 0; iy < height; iy++) {
	    register ubyte c = board[ix][iy] & (FLAGGED | MINED);
	    if (c == FLAGGED || c == MINED)
		return;		/* all flags and mines must match */
	}
    Win();
}


void ToggleFlagged(short sx, short sy)
{
    ubyte *bp = &board[sx][sy];
    if (*bp & QUESSED) {
	MakeANoise(SOUND_FLAG);
	*bp &= ~(FLAGGED | QUESSED);
	PlunkSquareImage(sx, sy, BLANK);
    } else {
	MakeANoise(SOUND_FLAG);
	*bp &= ~QUESSED;
	*bp ^= FLAGGED;
	if (*bp & FLAGGED) {
	    minesleft--;
	    PlunkSquareImage(sx, sy, FLAG);
	} else {
	    minesleft++;
	    if (questioning) {
		*bp |= QUESSED;
		PlunkSquareImage(sx, sy, QUESTION);
	    } else
		PlunkSquareImage(sx, sy, BLANK);
	}
	DrawMineCount();
	TryFlagWin();
    }
}


void HiliteNeighbors(short sx, short sy)
{
    struct IntuiMessage *im;
    short ix, iy;
    bool release;
    ulong secs, micros;
    ushort lastseconds = seconds;

    for (ix = sx - 1; ix <= sx + 1; ix++)
	for (iy = sy - 1; iy <= sy + 1; iy++)
	    if (ix >= 0 && ix < width && iy >= 0 && iy < height &&
				!(board[ix][iy] & (TRIED | FLAGGED)))
		PlunkSquareImage(ix, iy, 0);		/* temporary */
    do {
	MyWaitPort(bgw->UserPort);
	im = GT_GetIMsg(bgw->UserPort);
	release = im && im->Class != IDCMP_INTUITICKS
				&& im->Class != IDCMP_MOUSEMOVE;
	if (im)
	    GT_ReplyIMsg(im);
	if (clocking) {
	    CurrentTime(&secs, &micros);
	    seconds = secs - startsecs - (micros < startmicros);
	    if (lastseconds != seconds)
		DrawTime();
	    lastseconds = seconds;
	}
    } while (!release && playing);
    for (ix = sx - 1; ix <= sx + 1; ix++)
	for (iy = sy - 1; iy <= sy + 1; iy++)
	    if (ix >= 0 && ix < width && iy >= 0 && iy < height)
		if (!(board[ix][iy] & (playing ? TRIED | FLAGGED : 0xFF)))
		    PlunkSquareImage(ix, iy, board[ix][iy] & QUESSED
						? QUESTION : BLANK);
}


private void GropeAround(short sx, short sy)
{
    register short ix, iy;
    for (ix = sx - 1; ix <= sx + 1; ix++)
	for (iy = sy - 1; iy <= sy + 1; iy++)
	    if (ix >= 0 && ix < width && iy >= 0 && iy < height
				&& !(board[ix][iy] & TRIED))
		CountASquare(ix, iy);		/* stretch that stack! */
}


void CountASquare(short sx, short sy)
{
    register short ix, iy, c = 0;
    short oldminecount;
#ifndef BIGSTACK
    bool anymore;
    static short depth = -1;
#endif

    if (board[sx][sy] & TRIED)
	return;				/* should never happen */
    if (!++depth)
	oldminecount = minesleft;
    if (board[sx][sy] & (FLAGGED | QUESSED)) {	/* yes it can happen */
	if (board[sx][sy] & FLAGGED)
	    minesleft++;
	board[sx][sy] &= ~(FLAGGED | QUESSED);
    }
    board[sx][sy] |= TRIED;
    trycount++;
    for (ix = sx - 1; ix <= sx + 1; ix++)
	for (iy = sy - 1; iy <= sy + 1; iy++)
	    if (ix >= 0 && ix < width && iy >= 0 && iy < height
				&& board[ix][iy] & MINED)
		c++;
    PlunkSquareImage(sx, sy, c);
    if (c > 0 || finished) {
	if (!stifle && !depth)
	    MakeANoise(SOUND_TEST);
	depth--;
	return;
    }
    if (!depth && (!stifle || !whoosh))
	MakeANoise(SOUND_RIPPLE);
    whoosh = true;
#ifdef BIG_STACK
    GropeAround(sx, sy);
#else
    if (depth > 20) {			/* value 20 from trial and error */
	board[sx][sy] |= TRY_ZERO;
	depth--;
	return;
    }
    GropeAround(sx, sy);		/* incomplete if depth exceeds limit */
    if (!depth)
	do {
	    anymore = false;
	    for (ix = 0; ix < width; ix++)
		for (iy = 0; iy < height; iy++)
		    if (board[ix][iy] & TRY_ZERO) {
			anymore = true;
			board[ix][iy] &= ~TRY_ZERO;
			GropeAround(ix, iy);
		    }
		    
	} while (anymore);
#endif
    if (!depth && minesleft != oldminecount)
	DrawMineCount();
    depth--;
}


bool AutoTryNeighbors(short sx, short sy)
{
    short ix, iy, unflagged = 0, quessed = 0, oldmc;
    bool stifleded;

    if (!(board[sx][sy] & TRIED))	/* should never happen */
	return false;
    for (ix = sx - 1; ix <= sx + 1; ix++)
	for (iy = sy - 1; iy <= sy + 1; iy++)
	    if (ix >= 0 && ix < width && iy >= 0 && iy < height) {
		register ubyte f = board[ix][iy];
		if (f & MINED)
		    unflagged++;
		if (f & FLAGGED)
		    unflagged--;
		if (f & QUESSED)
		    quessed++;
	    }
    if (unflagged | quessed)	/* mines and flags must be equal in number, */
	return false;		/* and there must be no question marks.     */
    for (ix = sx - 1; ix <= sx + 1; ix++)
	for (iy = sy - 1; iy <= sy + 1; iy++)
	    if (ix >= 0 && ix < width && iy >= 0 && iy < height
				&& !(board[ix][iy] & (TRIED | FLAGGED)))
		if (board[ix][iy] & MINED) {
		    StopPlaying(true, sx, sy);		/* MULTI-BOOM! */
		    return true;
		}
    /* don't try to do a BOOM in the middle of this: */
    oldmc = minesleft;
    whoosh = false;
    for (ix = sx - 1; ix <= sx + 1; ix++)
	for (iy = sy - 1; iy <= sy + 1; iy++)
	    if (ix >= 0 && ix < width && iy >= 0 && iy < height
				&& !(board[ix][iy] & (TRIED | FLAGGED))) {
		stifleded = stifle;
		stifle = true;
		CountASquare(ix, iy);
		if (!whoosh && !stifleded)
		    MakeANoise(SOUND_NTEST);
	    }
    stifle = false;
    if (minesleft != oldmc)
	DrawMineCount();
    if (!flagwin && trycount >= width * height - mines)
	Win();
    return true;
}


void NewBoard(void)
{
    StopPlaying(false, -1, -1);
    minesleft = mines;
    seconds = trycount = 0;
    playing = finished = false;
    PutBombsInIt();
    DrawEverything();
}


void HideTheBoard(struct IntuiText *stuff)
{
    static short rxybor[6] = { 569, 0, 0, 0, 0, 186 },
			rxybor2[6] = { 0, 187, 569, 187, 569, 1 };
    static struct Border reqbor2 = {
	0, 0, 1, REQBACK, JAM2, 3, rxybor2, null
    };
    static struct Border reqbor = {
	0, 0, 2, REQBACK, JAM2, 3, rxybor, &reqbor2
    };
    static struct Requester req = {
	null, LEFTMARGIN + 2, 2, 570, 188, 0, 0, null, &reqbor,
	&rtclick, NOISYREQ, REQBACK, null, { 0 }, null, null, null, { 0 }
    };
    struct IntuiMessage *im;
    struct IntuiText *it;
    ulong secs;
    long micros;
    bool click, stretch = boardtop < 2;

    CurrentTime(&secs, (ulong *) &micros);
    startsecs = secs - startsecs;
    startmicros = micros - startmicros;
    if (startmicros < 0)
	startsecs--, startmicros += 1000000;
    /* startXXX are now elapsed time currently past in clock */

    req.Width = bgw->Width - (LEFTMARGIN + 2);
    rxybor[0] = rxybor2[2] = rxybor2[4] = req.Width - 1;
    req.Height = bgw->Height + stretch - 2;
    req.TopEdge = 2 - stretch;
    rxybor[5] = (rxybor2[1] = rxybor2[3] = req.Height - 1) - 1;
    rtclick.LeftEdge = (req.Width - 10 * strlen(rtclick.IText)) / 2;
    rtclick.TopEdge = req.Height - 16;
    if (!stuff) {
	sprintf(pausestring, "Clock is pausing at %ld.%ld seconds",
					startsecs, startmicros / 100000);
	rtpause.LeftEdge = (req.Width - 10 * strlen(pausestring)) / 2;
	stuff = &rtpause;
    } else
	for (it = stuff; it; it = it->NextText)
	    it->LeftEdge = (req.Width - ABOUTWIDTH) / 2;
    rtclick.NextText = stuff;
    while (im = (adr) GetMsg(bgw->UserPort))	/* discard queued events */
	ReplyMsg((adr) im);
    if (Request(&req, bgw)) {
	do {
	    MyWaitPort(bgw->UserPort);
	    im = GT_GetIMsg(bgw->UserPort);
	    click = im && !(im->Code & IECODE_UP_PREFIX) &&
				((im->Class == IDCMP_MOUSEBUTTONS
				     && im->Code == IECODE_LBUTTON)
				 || (im->Class == IDCMP_VANILLAKEY
				     && !(im->Qualifier & IEQUALIFIER_REPEAT)));
	    if (im)
		GT_ReplyIMsg(im);
	} while (!click);
	EndRequest(&req, bgw);
    } else
	DisplayBeep(scr);

    CurrentTime(&secs, (ulong *) &micros);
    startsecs = secs - startsecs;
    startmicros = micros - startmicros;
    if (startmicros < 0)
	startsecs--, startmicros += 1000000;
}


private bool PlayEvent(struct IntuiMessage *im)
{
    short sx, sy, oc0;
    bool tried;
    bool BetweenGameEvent(struct IntuiMessage *im);

    if (im->Class == IDCMP_MOUSEBUTTONS) {
	if (im->Code & IECODE_UP_PREFIX)
	    return false;
	if (im->Code != IECODE_LBUTTON && im->Code != IECODE_RBUTTON)
	    return false;	/* ignore middle button */
	sx = (im->MouseX - boardleft) / squarewidth;
	sy = (im->MouseY - boardtop) / squareheight;
	if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
	    if (pushbomb && board[sx][sy] & MINED) {
		PlantARandomMine();		/* first click always safe */
		board[sx][sy] &= ~MINED;
	    }
	    pushbomb = false;
	    tried = !!(board[sx][sy] & TRIED);
	    if (im->Code == IECODE_RBUTTON) {
		if (tried) {
		    if (!AutoTryNeighbors(sx, sy) && playing)
			HiliteNeighbors(sx, sy);
		} else
		    ToggleFlagged(sx, sy);
	    } else if (tried) {
		if (!AutoTryNeighbors(sx, sy) && playing)
		    HiliteNeighbors(sx, sy);
	    } else if (!(board[sx][sy] & (QUESSED | FLAGGED))) {
		if (board[sx][sy] & MINED)
		    StopPlaying(true, sx, sy);		/* BOOM! */
		else {
		    oc0 = minesleft;
		    CountASquare(sx, sy);
		    if (minesleft != oc0)
			DrawMineCount();
		    if (!flagwin && trycount >= width * height - mines)
			Win();
		}
	    }
	}
    } else if (im->Class == IDCMP_GADGETUP || im->Class == IDCMP_MENUPICK
					|| im->Class == IDCMP_VANILLAKEY)
	return BetweenGameEvent(im);
    else if (im->Class == IDCMP_INACTIVEWINDOW)
	HideTheBoard(null);
    return false;
}


private short Decode(struct IntuiMessage *im)
{
    short i = -1;
    char c;

    if (im->Class == IDCMP_MENUPICK) {
	i = C_BEGINNER * MENUNUM(im->Code) + ITEMNUM(im->Code);
	if (i == C_ADJUST1)			/* "Customize" submenu */
	    i += SUBNUM(im->Code);
	if (i >= C_SOUND)			/* a checkmarked item? */
	    i = -1;				/* we test checkmark directly */
    } else if (im->Class == IDCMP_VANILLAKEY) {
	c = toupper(im->Code);
	if (im->Qualifier & IEQUALIFIER_RCOMMAND
					|| c == 'N' || c == 'P' || c == 'Q')
	    switch (c) {
		case 'N': return C_NEW;
		case 'P': return playing ? C_PAUSE : -1;
		case 'S': return C_SCORES;
		case '?':
		case '/': return C_QUES;
		case 'C': return C_BEATCLOCK;
		case 'Z': return anyaudio ? C_SOUND : -1;
		case 'F': return C_FLAGWIN;
#ifdef RESETS
		case 'R': return C_RESET;
#endif
		case 'M': return playing ? -1 : C_MODE;
		case 'A': return C_ABOUT;
		case 'Q': return C_QUIT;
		case 'B': return C_BEGINNER;
		case 'I': return C_INTERMED;
		case 'E': return C_EXPERT;
		case '1': return C_CUSTOM1;
		case '2': return C_CUSTOM2;
		case '3': return C_CUSTOM3;
	    }
    } else
	return ((struct Gadget *) im->IAddress)->GadgetID;
    return i;
}


private bool BetweenGameEvent(struct IntuiMessage *im)
{
    LEVEL ll, oldll;
    short i;
    bool formersilence = silence, formerflagwin = flagwin;

    switch (im->Class) {
	case IDCMP_MENUPICK:
	    silence = !(mo1.Flags & CHECKED);		/* reversed sense */
	    countdown = !!(mo2.Flags & CHECKED);
	    questioning = !!(mo3.Flags & CHECKED);
	    flagwin = !!(mo4.Flags & CHECKED);
	    /* VvVvVvVvVvVvVvVvVvVvV   FALL THROUGH: */
	case IDCMP_GADGETUP:
	case IDCMP_VANILLAKEY:
	    switch (i = Decode(im)) {
		case C_NEW:
		    NewBoard();
		    break;
		case C_PAUSE:
		case C_ABOUT:
		    HideTheBoard(i == C_ABOUT ? rtabout : null);
		    break;
		case C_SCORES:
		    ShowHighScores();
		    break;
		case C_SOUND:
		    if (anyaudio) {
			if (silence = !silence)		/* reversed sense */
			    mo1.Flags &= ~CHECKED;
			else
			    mo1.Flags |= CHECKED;
		    }
		    break;
		case C_BEATCLOCK:
		    if (countdown = !countdown)
			mo2.Flags |= CHECKED;
		    else
			mo2.Flags &= ~CHECKED;
		    break;
		case C_QUES:
		    if (questioning = !questioning)
			mo3.Flags |= CHECKED;
		    else
			mo3.Flags &= ~CHECKED;
		    break;
		case C_FLAGWIN:
		    if (flagwin = !flagwin)
			mo4.Flags |= CHECKED;
		    else
			mo4.Flags &= ~CHECKED;
		    break;
#ifdef RESETS
		case C_RESET:
		    ResetHighScores();
		    break;
#endif
		case C_MODE:
		    if (!ChangeScreenMode())
			return true;
		    break;
		case C_QUIT:
		    return true;
		case C_BEGINNER:
		case C_INTERMED:
		case C_EXPERT:
		case C_CUSTOM1:
		case C_CUSTOM2:
		case C_CUSTOM3:
		    oldll = lvl;
		    if (im->Class == IDCMP_MENUPICK) {
			for (ll = beginner; ll < LEVELS; ll++)
			    if (lvlitem[ll]->Flags & CHECKED) {
				lvl = ll;
				break;
			    }
		    } else
			lvl = beginner + (i - C_BEGINNER);
		    NoticeNewLevel(lvl);
		    if (width > (bgw->Width - LEFTMARGIN) / LSQUAREWIDTH
				    || height > bgw->Height / LSQUAREHEIGHT) {
			Error("That board size is too big\n"
					"to fit on this screen!");
			NoticeNewLevel(lvl = oldll);
		    }
		    NewBoard();
		    break;
		case C_ADJUST1:
		case C_ADJUST2:
		case C_ADJUST3:
		    Customize(custom1 + i - C_ADJUST1);
		    break;
	    }
	    break;
	case IDCMP_MOUSEBUTTONS:
	    if (im->MouseX >= boardleft && im->MouseX < boardright
			    && im->MouseY >= boardtop && im->MouseY < boardbot
			    && !(im->Code & IECODE_UP_PREFIX) && !finished) {
		playing = true;
		StartClock();
		SetPauseGadget(true);
		OnMenu(bgw, FULLMENUNUM(0, C_PAUSE, NOSUB));
		OffMenu(bgw, FULLMENUNUM(2, C_MODE - 20, NOSUB));
		OffMenu(bgw, FULLMENUNUM(1, C_ADJUST1 - C_BEGINNER, NOSUB));
		pushbomb = true;
		return PlayEvent(im);
	    }
	    break;
	default:
	    break;
    }
    if (silence && !formersilence)
	ReleaseChannels();
    if (flagwin && !formerflagwin)
	TryFlagWin();
    else if (formerflagwin && !flagwin)
	if (trycount >= width * height - mines)
	    Win();
    return false;
}


void Play(void)
{
    struct IntuiMessage mim, *im;
    ushort lastseconds = seconds;

    for (;;) {
	MyWaitPort(bgw->UserPort);
	while (im = GT_GetIMsg(bgw->UserPort)) {
	    mim = *im;
	    GT_ReplyIMsg(im);
	    if (!finished && mim.MouseX >= boardleft && mim.MouseY >= boardtop
			    && mim.MouseX < boardright && mim.MouseY < boardbot)
		bgw->Flags |= WFLG_RMBTRAP;
	    else
		bgw->Flags &= ~WFLG_RMBTRAP;
	    if (clocking) {			    /* paranoia VVV */
		seconds = mim.Seconds - startsecs - (mim.Seconds > startsecs
						 && mim.Micros < startmicros);
		if (lastseconds != seconds)
		    DrawTime();
		lastseconds = seconds;
	    }
	    if (playing ? PlayEvent(&mim) : BetweenGameEvent(&mim))
		return;
	}
    }
}
