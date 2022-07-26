/* Miner by Paul Kienitz, Feb 1994, freely distributable, dedicated to (of  */
/* course, who else) Jay Miner.  My own implementation of the minesweeper   */
/* game, because the others available when I began it all sucked.           */

#define ASL_V38_NAMES_ONLY
#include <hardware/intbits.h>
#include <exec/interrupts.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <graphics/view.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <workbench/workbench.h>
#include <workbench/startup.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "miner.h"


#if defined(DEBUG) && defined(AZTEC_C)
#  define main Dmain	/* MUST match similar definition in aztecstuff.c! */
#endif


#define ITEM1WID      200
#define ITEM2WID      200
#define ITEM3WID      216
#define ITEMFLAGS     (ITEMTEXT | ITEMENABLED | HIGHCOMP | COMMSEQ)
#define MUXFLAGS      (ITEMFLAGS | CHECKIT)
#define TOGGLEFLAGS   (ITEMFLAGS | CHECKIT | MENUTOGGLE)
#define CLUDE(b)      (0x3F & ~(1 << (b)))
#define MIT(N, S)     struct IntuiText N = { 2, 1, JAM2, 4, 1, null, S, null }
#define MITC(N, S)    struct IntuiText N = { 2, 1, JAM2, 24, 1, null, S, null }

#define COOKIE        0xC2DC94DE
#define SCOREFILE     "PROGDIR:Miner.scores"
#define DEFBOOMFILE   "PROGDIR:Miner.BoomSound"
#define DEFWINFILE    "PROGDIR:Miner.WinSound"
#define DEFTESTFILE   "PROGDIR:Miner.TestSound"
#define DEFRIPPLEFILE "PROGDIR:Miner.RippleSound"
#define SOUNDARGLEN   128


void DrawEverything(void);
void NewBoard(void);
void Play(void);
void HideTheBoard(struct IntuiText *stuff);
void MyWaitPort(struct MsgPort *p);
bool LoadASample(ushort which, str filename);
void FreeSamples(void);


#ifdef AUTOFLASH
/* import struct ColorMap *ourcm; */
import struct Interrupt flasher;
#endif

import bool questioning, silence, flagwin;

#ifdef AZTEC_C
import struct WBStartup *WBenchMsg;
#endif


struct Process *me;

struct Library *IntuitionBase, *GfxBase, *GadToolsBase, *AslBase;
struct Library *DOSBase, *IconBase;

char scrtitle[] = NAME " v" VERSION "      by Paul Kienitz";

/* There must be SOUND_COUNT keywords in this template: */
char template[] = "BoomSound/K,WinSound/K,TestSound/K,"
		  "NeighborTestSound/K,RippleSound/K,FlagSound/K";

char soundargs[SOUND_COUNT][SOUNDARGLEN] = {
    DEFBOOMFILE, DEFWINFILE, DEFTESTFILE, "", DEFRIPPLEFILE, ""
};

struct TextAttr topaz9 = { "topaz.font", 9, 0, FPF_ROMFONT };
#ifdef FIDDLE_FONT
char fontcopyname[30];
struct TextAttr topaz9copy = { &fontcopyname[0], 9, 0, 0 };
struct TextFont *topazcopy;
bool copyingtopaz = false;
#endif

/* palette order: gray, black, white, red, blue, darker gray, green, yellow */
ushort mypalette[8] = {
    0xAAA, 0x000, 0xFFF, 0xF00, 0x00F, 0x999, 0x4F3, 0xFF0
}, myscrpens[] = { ~0 };


MITC(mot4, "Win with flags");

struct MenuItem mo4 = {
    null, 0, 48, ITEM3WID, 12, TOGGLEFLAGS, 0, &mot4, null, 'F', null, 0
};

MITC(mot3, "Use \"?\" marks");

struct MenuItem mo3 = {
    &mo4, 0, 36, ITEM3WID, 12, TOGGLEFLAGS, 0, &mot3, null, '?', null, 0
};

MITC(mot2, "Beat the clock");

struct MenuItem mo2 = {
    &mo3, 0, 24, ITEM3WID, 12, TOGGLEFLAGS, 0, &mot2, null, 'C', null, 0
};

MITC(mot1, "Sound effects");

struct MenuItem mo1 = {
    &mo2, 0, 12, ITEM3WID, 12, TOGGLEFLAGS, 0, &mot1, null, 'Z', null, 0
};

MIT(mot0, "Screen mode...");

struct MenuItem mo0 = {
    &mo1, 0, 0, ITEM3WID, 12, ITEMFLAGS, 0, &mot0, null, 'M', null, 0
};

struct Menu optionmenu = {
    null, 160, 0, 100, 11, MENUENABLED, " Options", &mo0, 0, 0, 0, 0
};


char mc1buf[24], mc2buf[24], mc3buf[24];

MIT(mct2, mc3buf);

struct MenuItem mc2 = {
    null, 150, 24, 240, 12, ITEMFLAGS & ~COMMSEQ, 0, &mct2, null, 0, null, 0
};

MIT(mct1, mc2buf);

struct MenuItem mc1 = {
    &mc2, 150, 12, 240, 12, ITEMFLAGS & ~COMMSEQ, 0, &mct1, null, 0, null, 0
};

MIT(mct0, mc1buf);

struct MenuItem mc0 = {
    &mc1, 150, 0, 240, 12, ITEMFLAGS & ~COMMSEQ, 0, &mct0, null, 0, null, 0
};

MIT(mbt6, "Customize         »");

struct MenuItem mb6 = {
    null, 0, 72, ITEM2WID, 12, ITEMTEXT | ITEMENABLED,
    0, &mbt6, null, 0, &mc0, 0
};

MITC(mbt5, "Custom #3");

struct MenuItem mb5 = {
    &mb6, 0, 60, ITEM2WID, 12, MUXFLAGS, CLUDE(5), &mbt5, null, '3', null, 0
};

MITC(mbt4, "Custom #2");

struct MenuItem mb4 = {
    &mb5, 0, 48, ITEM2WID, 12, MUXFLAGS, CLUDE(4), &mbt4, null, '2', null, 0
};

MITC(mbt3, "Custom #1");

struct MenuItem mb3 = {
    &mb4, 0, 36, ITEM2WID, 12, MUXFLAGS, CLUDE(3), &mbt3, null, '1', null, 0
};

MITC(mbt2, "Expert");

struct MenuItem mb2 = {
    &mb3, 0, 24, ITEM2WID, 12, MUXFLAGS, CLUDE(2), &mbt2, null, 'E', null, 0
};

MITC(mbt1, "Intermediate");

struct MenuItem mb1 = {
    &mb2, 0, 12, ITEM2WID, 12, MUXFLAGS, CLUDE(1), &mbt1, null, 'I', null, 0
};

MITC(mbt0, "Beginner");

struct MenuItem mb0 = {
    &mb1, 0, 0, ITEM2WID, 12, MUXFLAGS, CLUDE(0), &mbt0, null, 'B', null, 0
};

struct Menu boardmenu = {
    &optionmenu, 80, 0, 80, 11, MENUENABLED, " Board", &mb0, 0, 0, 0, 0
};


#ifdef RESETS

MIT(mgt6, "Quit");

struct MenuItem mg6 = {
    null, 0, 72, ITEM1WID, 12, ITEMFLAGS, 0, &mgt6, null, 'Q', null, 0
};

MIT(mgt5, "About...");

struct MenuItem mg5 = {
    &mg6, 0, 60, ITEM1WID, 12, ITEMFLAGS, 0, &mgt5, null, 'A', null, 0
};

MIT(mgt4, " ------------------ ");

struct MenuItem mg4 = {
    &mg5, 0, 48, ITEM1WID, 12, ITEMTEXT, 0, &mgt4, null, 0, null, 0
};

MIT(mgt3, "Reset scores");

struct MenuItem mg3 = {
    &mg4, 0, 36, ITEM1WID, 12, ITEMFLAGS, 0, &mgt3, null, 'R', null, 0
};

#else /* RESETS */

MIT(mgt5, "Quit");

struct MenuItem mg5 = {
    null, 0, 60, ITEM1WID, 12, ITEMFLAGS, 0, &mgt5, null, 'Q', null, 0
};

MIT(mgt4, "About...");

struct MenuItem mg4 = {
    &mg5, 0, 48, ITEM1WID, 12, ITEMFLAGS, 0, &mgt4, null, 'A', null, 0
};

MIT(mgt3, " ------------------ ");

struct MenuItem mg3 = {
    &mg4, 0, 36, ITEM1WID, 12, ITEMTEXT, 0, &mgt3, null, 0, null, 0
};

#endif RESETS

MIT(mgt2, "Best scores...");

struct MenuItem mg2 = {
    &mg3, 0, 24, ITEM1WID, 12, ITEMFLAGS, 0, &mgt2, null, 'S', null, 0
};

MIT(mgt1, "Pause");

struct MenuItem mg1 = {
    &mg2, 0, 12, ITEM1WID, 12, ITEMFLAGS, 0, &mgt1, null, 'P', null, 0
};

MIT(mgt0, "New game");

struct MenuItem mg0 = {
    &mg1, 0, 0, ITEM1WID, 12, ITEMFLAGS, 0, &mgt0, null, 'N', null, 0
};

struct Menu gamemenu = {
    &boardmenu, 0, 0, 80, 11, MENUENABLED, " Game", &mg0, 0, 0, 0, 0
};


struct MenuItem *(lvlitem[LEVELS]) = { &mb0, &mb1, &mb2, &mb3, &mb4, &mb5 };

struct NewGadget abutton = {
    5, 140, 60, 13, null, &topaz9, 0, PLACETEXT_IN, null, null
};

struct Gadget *freelist = null, *gagpause;

struct TagItem notags[1] = { { TAG_DONE, 0 } };


struct Screen *scr = null;
struct Window *bgw = null;
APTR myvi;
struct RastPort *bp;
bool fallback = false, anyaudio = false;
ulong screenmode;

const char processname[] = NAME;	/* for aztecstuff.c detacher */
#ifdef BIGSTACK
const long _stack = 40000;		/* likewise */
#define STACKTEXT "40000"
#else
const long _stack = 4000;
#endif


typedef struct {
    ulong cookie;
    ushort /* LEVEL */ current;
    ushort cbsizes[CUSLEVELS][2], cbmines[CUSLEVELS];
    ushort cssizes[CUSLEVELS][2], csmines[3];
    SCORE highs[LEVELS][SPERLEVEL];
    ulong cookie2;
    /* the following fields were added for version 4: */
    ulong scrmode;
    ushort /* bool */ quesmarx;
    /* the following fields were added for version 5: */
    ushort /* bool */ silent, winwithflags;
    /* random number seeds for each winning board? */
} SCORERECORD;

#define BIGENOUGHFOR(f) (offsetof(SCORERECORD, f) + \
			 sizeof(((SCORERECORD *) 0)->f))
#define MINSCORESIZE    BIGENOUGHFOR(cookie2)


SCORERECORD oldscores;

ushort boardsizes[LEVELS][2] = {
    {8, 8}, {16, 16}, {16, 30}, {12, 12}, {15, 20}, {21, 30}
};
ushort boardmines[LEVELS] = { 10, 40, 99, 28, 55, 120 };

SCORE highscores[LEVELS][SPERLEVEL] = {
    { WORST, WORST, WORST }, { WORST, WORST, WORST }, { WORST, WORST, WORST },
    { WORST, WORST, WORST }, { WORST, WORST, WORST }, { WORST, WORST, WORST }
};

char myname[SNAMELEN] = "";

str levelabels[LEVELS] = {
    "Beginner", "Intermediate", "Expert", "Custom #1", "Custom #2", "Custom #3"
};

ushort cuscoresizes[LEVELS - custom1][2] = { {12, 12}, {15, 20}, {21, 30} };
ushort cuscoremines[LEVELS - custom1] = { 28, 50, 120 };

ubyte board[BOARDMAX_X][BOARDMAX_Y];

LEVEL lvl = beginner;

ushort width, height, mines;
bool beater = false;


/* ----------------------------------------------------------------------- */


ulong Random(ushort choices)
{
    ulong raw = ((ulong) rand() << 15) + rand();
    return choices ? raw % choices : 0;
}


void VersionError(void)
{
#   define AUTOSTUFF   AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE
    static struct IntuiText ailine2 = {
	AUTOSTUFF, 15, 15, null, "AmigaDOS 2.04 or newer", null
    }, ailine1 = {
	AUTOSTUFF, 15, 5, null, NAME " v" VERSION " requires", &ailine2
    }, aineg = {
	AUTOSTUFF, AUTOLEFTEDGE, AUTOTOPEDGE, null, "Okay", null
    };

    if (!(IntuitionBase = OpenL("intuition")))
	return;
    AutoRequest(null, &ailine1, null, &aineg, 0, 0, 320, 62);
    CloseLibrary((adr) IntuitionBase);
}



void Error(str text, ...)
{
    static struct EasyStruct es = { sizeof(es), 0, "Miner", null, "Okay" };
    va_list v;

    va_start(v, text);
    es.es_TextFormat = text;
    es.es_Title =  beater ? "Miner: \"Beat the Clock\" interruption" : "Miner";
    EasyRequestArgs(bgw, &es, null, (adr) v);
    va_end(v);
}


void CopyArg(ushort which, str what)
{
    if (strlen(what) < SOUNDARGLEN)
	strcpy(soundargs[which], what);
    else {
	strncpy(soundargs[which], what, SOUNDARGLEN - 1);
	soundargs[which][SOUNDARGLEN - 1] = 0;
    }
}


void WBReadArgs(str template, struct WBStartup *wbm)
{
    struct WBArg *wa;
    struct DiskObject *bob;
    char ttword[40];
    str tp;
    ushort l, whicharg;
    BPTR oldcd;

    if (!wbm || !(IconBase = OpenL("icon")))
	return;
    wa = &wbm->sm_ArgList[0];
    oldcd = CurrentDir(wa->wa_Lock);
    if (bob = GetDiskObject(wa->wa_Name)) {
	for (whicharg = 0; *template; whicharg++) {
	    for (tp = template; *tp && *tp != '/' && *tp != ','; tp++) ;
	    l = tp - template;
	    if (l >= 40) l = 39;
	    strncpy(ttword, template, l);
	    ttword[l] = 0;
	    while (*tp && *tp != ',') tp++;
	    template = tp + !!*tp;
	    strupr(ttword);		/* v37 icon.library is case sensitive */
	    if (tp = FindToolType((ustr *) bob->do_ToolTypes, ttword))
		CopyArg(whicharg, tp);
	}
	FreeDiskObject(bob);
    }
    CurrentDir(oldcd);
    CloseLibrary(IconBase);
    IconBase = null;
}


/* The idea is that this function is called before the process is detached */
/* from the CLI.  I don't know how that would be handled with SAS C.  With */
/* Aztec C, the stuff in the file aztecstuff.c handles it.  If it returns  */
/* false, then the program does not run.  Currently it is setup to assume  */
/* it is not detaching, when compiled without Aztec C.                     */

bool PreDetachHook(void)
{
    struct RDArgs *ra;
    static long args[SOUND_COUNT] = { 0, 0, 0, 0, 0, 0 };
    ushort w;

    if (DOSBase->lib_Version < 37) {
	VersionError();
	return false;
    }
#ifdef AZTEC_C
    if (WBenchMsg)
	WBReadArgs(template, WBenchMsg);
    else
#endif
    {
	if (!(ra = ReadArgs(template, args, null))) {
	    PrintFault(IoErr(), "Miner");
	    return false;
	}
	for (w = 0; w < SOUND_COUNT; w++)
	    if (args[w])
		CopyArg(w, (str) args[w]);
	FreeArgs(ra);
    }
    return true;
}


void FreshenGadgets(void)
{
    static didonce = false;

    RefreshGList(freelist, bgw, null, ~0);
    if (!didonce)
	GT_RefreshWindow(bgw, null);
    didonce = true;
}


void SetPauseGadget(bool on)
{
    GT_SetGadgetAttrs(gagpause, bgw, null, GA_Disabled, (long) !on, TAG_DONE);
}


void PlantARandomMine(void)
{
    ushort x, y;
    do {
	x = Random(width);
	y = Random(height);
    } while (board[x][y]);
    board[x][y] = MINED;
}


void PutBombsInIt(void)
{
    ushort i;
    memset(board, 0, sizeof(board));
    for (i = 0; i < mines; i++)
	PlantARandomMine();
}


void NoteNewCustomSize(LEVEL ll)
{
    str buf = (ll == custom3 ? mc3buf : (ll == custom2 ? mc2buf : mc1buf));
    sprintf(buf, "#%c  (%u × %u, %uM)", '1' + (ll - custom1),
		boardsizes[ll][1], boardsizes[ll][0], boardmines[ll]);
}


void NoticeNewLevel(LEVEL ll)
{
    LEVEL i;
    width = boardsizes[ll][1];
    height = boardsizes[ll][0];
    mines = boardmines[ll];
    NoteNewCustomSize(custom1);
    NoteNewCustomSize(custom2);
    NoteNewCustomSize(custom3);
    for (i = beginner; i < LEVELS; i++)
	lvlitem[i]->Flags &= ~CHECKED;
    lvlitem[ll]->Flags |= CHECKED;
    PutBombsInIt();
}


void NukeDisplay(void)
{
    if (bgw) {
#ifdef AUTOFLASH
	RemIntServer(INTB_VERTB, &flasher);
#endif
	ClearMenuStrip(bgw);
	CloseWindow(bgw);
	bgw = null;
    }
    FreeGadgets(freelist);
    freelist = null;
    if (scr) {
	FreeVisualInfo(myvi);
	CloseScreen(scr);
	scr = null;
    }
#ifdef FIDDLE_FONT
    if (copyingtopaz) {
	RemFont(topazcopy);
	if (!topazcopy->tf_Accessors)
	    FREE(topazcopy);
	copyingtopaz = false;
    }
#endif
}


bool CreateDisplay(void)
{
    static struct NewScreen news = {
	0, 0, STDSCREENWIDTH, STDSCREENHEIGHT, 3, 0, 1, HIRES,
	CUSTOMSCREEN, &topaz9, scrtitle, null, null
    };
    static struct NewWindow neww = {
	0, 12, 640, 188, 0, 1,
	BUTTONIDCMP | IDCMP_MENUPICK | IDCMP_VANILLAKEY | IDCMP_MOUSEBUTTONS
		    | IDCMP_MOUSEMOVE | IDCMP_INTUITICKS | IDCMP_INACTIVEWINDOW,
	WFLG_BACKDROP | WFLG_BORDERLESS | WFLG_ACTIVATE | WFLG_NEWLOOKMENUS
		    | WFLG_SMART_REFRESH | WFLG_REPORTMOUSE,
	null, null, null, null, null, 0, 0, 0, 0, CUSTOMSCREEN
    };
    struct Gadget *preg;
    long errv = 0;
#ifdef FIDDLE_FONT
    struct TextFont *t9;
#endif

    if (!(scr = OpenScreenTags(&news, SA_Pens, myscrpens, SA_DisplayID,
					screenmode, SA_Interleaved, (long) TRUE,
					SA_ErrorCode, &errv, TAG_DONE))) {
	if (anyaudio && (errv == OSERR_NOMEM || errv == OSERR_NOCHIPMEM)) {
	    FreeSamples();
	    anyaudio = false;
	    scr = OpenScreenTags(&news, SA_Pens, myscrpens,
					SA_DisplayID, screenmode,
					SA_Interleaved, (long) TRUE, TAG_DONE);
	}
	if (!scr) {
	    screenmode = HIRES_KEY;
	    fallback = true;
	    if (!(scr = OpenScreenTags(&news, SA_Pens, myscrpens, TAG_DONE)))
		return false;
	}
    }
    LoadRGB4(&scr->ViewPort, mypalette, 8);
#ifdef AUTOFLASH
/*    ourcm = scr->ViewPort.ColorMap;	*/
#endif
    if (!(myvi = GetVisualInfoA(scr, notags))) {
	CloseScreen(scr);
	return false;
    }
    neww.Width = scr->Width;
    neww.TopEdge = scr->BarHeight /* + 1 */;
    neww.Height = scr->Height - neww.TopEdge;
    neww.Screen = scr;
#ifdef FIDDLE_FONT
    if (t9 = OpenFont(&topaz9)) {			/* should never fail */
	if (NEWP(topazcopy)) {
	    memcpy(topazcopy, t9, sizeof(*topazcopy));
	    topazcopy->tf_Baseline++;			/* the change we want */
	    sprintf(fontcopyname, "_topaz_copy_%lX.font", me);
	    topazcopy->tf_Message.mn_Node.ln_Name = fontcopyname;
	    topazcopy->tf_Flags &= ~FPF_ROMFONT;
	    topazcopy->tf_Message.mn_ReplyPort = null;	/* IMPORTANT! */
	    AddFont(topazcopy);
	    copyingtopaz = true;
	    abutton.ng_TextAttr = &topaz9copy;
	}
	CloseFont(t9);
    }
#endif

    preg = CreateContext(&freelist);
    abutton.ng_LeftEdge = 5;
    abutton.ng_Width = 60;
    abutton.ng_VisualInfo = myvi;
    abutton.ng_TopEdge = 115;
    abutton.ng_GadgetID = C_NEW;
    abutton.ng_GadgetText = "_New";
    preg = CreateGadget(BUTTON_KIND, preg, &abutton,
					GT_Underscore, '_', TAG_DONE);
    abutton.ng_TopEdge = 140;
    abutton.ng_GadgetID = C_PAUSE;
    abutton.ng_GadgetText = "_Pause";
    gagpause = preg = CreateGadget(BUTTON_KIND, preg, &abutton, GA_Disabled, 1L,
					GT_Underscore, '_', TAG_DONE);
    abutton.ng_TopEdge = 165;
    abutton.ng_GadgetID = C_QUIT;
    abutton.ng_GadgetText = "_Quit";
    preg = CreateGadget(BUTTON_KIND, preg, &abutton,
					GT_Underscore, '_', TAG_DONE);
    if (!preg || !(bgw = OpenWindow(&neww))) {
	NukeDisplay();
	return false;
    }
    mg1.Flags &= ~ITEMENABLED;
    if (questioning)
	mo3.Flags |= CHECKED;
    else
	mo3.Flags &= ~CHECKED;
    if (flagwin)
	mo4.Flags |= CHECKED;
    else
	mo4.Flags &= ~CHECKED;
    if (!anyaudio)
	mo1.Flags &= ~(CHECKED | ITEMENABLED);
    else if (silence)
	mo1.Flags &= ~CHECKED;
    else
	mo1.Flags |= CHECKED;
    SetMenuStrip(bgw, &gamemenu);
    bp = bgw->RPort;
    AddGList(bgw, freelist, 0, ~0, null);
    if (width > (bgw->Width - LEFTMARGIN) / LSQUAREWIDTH
				|| height > bgw->Height / LSQUAREHEIGHT)
	NoticeNewLevel(lvl = beginner);
#ifdef AUTOFLASH
    AddIntServer(INTB_VERTB, &flasher);
#endif
    return true;
}


bool ChangeScreenMode(void)		/* false means screen nonexistent! */
{
#ifdef SPECIAL_DEFAULT
    static struct DisplayMode defaultmode = {
	{ null, null, 0, 0, "default: High Res" },
	{
	    { DTAG_DIMS, ~0L, TAG_SKIP, sizeof(struct DimensionInfo) / 2 },
	    3, 640, 200    /* ... leave the rest uninitialized */
	},
	DIPF_IS_WB	/* probably several others should be here */
    };
    struct List deflist;
#else
    struct MonitorInfo mo;
    ulong defjammode = screenmode;
#endif
    ulong oldmode = screenmode;
    struct ScreenModeRequester *srq;
    bool srret;

#ifdef SPECIAL_DEFAULT
    NewList(&deflist);
    if (GetDisplayInfoData(null, (UBYTE *) &defaultmode.dm_DimensionInfo,
			sizeof(struct DimensionInfo), DTAG_DIMS, HIRES_KEY) > 0)
	AddHead(&deflist, &defaultmode.dm_Node);
    defaultmode.dm_DimensionInfo.Header.DisplayID = ~0L;
#else
    if (screenmode == HIRES_KEY && GetDisplayInfoData(null, (UBYTE *) &mo,
			sizeof(mo), DTAG_MNTR, HIRES_KEY) > 0)
	defjammode = mo.PreferredModeID;
#endif
    if (!(AslBase = OpenLibrary("asl.library", 38))) {
	Error("No screen mode requester available;\n"
			"asl.library version 38 or newer required.");
	return true;
    }
    if (!(srq = AllocAslRequestTags(ASL_ScreenModeRequest, ASLSM_Window, bgw,
#ifdef SPECIAL_DEFAULT
			ASLSM_CustomSMList, &deflist, ASLSM_InitialDisplayID,
			screenmode == HIRES_KEY ? ~0L : screenmode,
#else
			ASLSM_InitialDisplayID, defjammode,
#endif
			ASLSM_MinDepth, 3L, ASLSM_MaxDepth, 3L,
			ASLSM_TitleText, "Screen type", TAG_DONE))) {
	Error("Could not open screen mode requester.");
	return true;
    }
    srret = AslRequest(srq, (adr) notags);
    if (srret)
	screenmode = srq->sm_DisplayID;
#ifdef SPECIAL_DEFAULT
    if (screenmode == ~0L)
	screenmode = HIRES_KEY;
#endif
    FreeAslRequest(srq);
    CloseLibrary(AslBase);
    AslBase = null;
    if (screenmode != oldmode) {
	NukeDisplay();
	if (!CreateDisplay()) {
	    Error("Could not open new screen for " NAME);
	    screenmode = HIRES_KEY;		/* redundant safety */
	    return false;
	}
	NewBoard();
    }
    return true;
}


void ReadScoreFile(void)
{
    BPTR hand = OOpen(SCOREFILE);
    SCORERECORD sf;
    LEVEL ll;
    short i;

    screenmode = HIRES_KEY;
    if (hand) {
	if ((i = Read(hand, &sf, sizeof(sf))) >= BIGENOUGHFOR(cookie2)
			&& sf.cookie == COOKIE && sf.cookie2 == COOKIE) {
	    if (i >= BIGENOUGHFOR(scrmode) && sf.scrmode)
		screenmode = sf.scrmode;
	    if (i < BIGENOUGHFOR(quesmarx))
		sf.quesmarx = false;
	    if (i < BIGENOUGHFOR(silent))
		sf.silent = false;
	    if (i < BIGENOUGHFOR(winwithflags))
		sf.winwithflags = false;
	    questioning = !!sf.quesmarx;
	    silence = !!sf.silent;
	    flagwin = !!sf.winwithflags;
	    lvl = sf.current;
	    for (i = 0; i < CUSLEVELS; i++) {
		boardsizes[i + custom1][0] = sf.cbsizes[i][0];
		boardsizes[i + custom1][1] = sf.cbsizes[i][1];
		boardmines[i + custom1] = sf.cbmines[i];
		cuscoresizes[i][0] = sf.cssizes[i][0];
		cuscoresizes[i][1] = sf.cssizes[i][1];
		cuscoremines[i] = sf.csmines[i];
	    }
	    for (ll = beginner; ll < LEVELS; ll++)
		for (i = 0; i < SPERLEVEL; i++)
		    highscores[ll][i] = sf.highs[ll][i];
	    oldscores = sf;
	} else
	    Error("Not a valid score file:\n%s", SCOREFILE);
	Close(hand);
    }
    NoticeNewLevel(lvl);
}


void WriteScoreFile(void)
{
    BPTR hand;
    SCORERECORD sf;
    LEVEL ll;
    short i;

    sf.cookie = sf.cookie2 = COOKIE;
    sf.current = lvl;
    for (i = 0; i < CUSLEVELS; i++) {
	sf.cbsizes[i][0] = boardsizes[i + custom1][0];
	sf.cbsizes[i][1] = boardsizes[i + custom1][1];
	sf.cbmines[i] = boardmines[i + custom1];
	sf.cssizes[i][0] = cuscoresizes[i][0];
	sf.cssizes[i][1] = cuscoresizes[i][1];
	sf.csmines[i] = cuscoremines[i];
    }
    for (ll = beginner; ll < LEVELS; ll++)
	for (i = 0; i < SPERLEVEL; i++)
	    sf.highs[ll][i] = highscores[ll][i];
    sf.scrmode = screenmode == HIRES_KEY ? 0 : screenmode;
    sf.quesmarx = questioning;
    sf.silent = silence;
    sf.winwithflags = flagwin;
    if (sf == oldscores)
	return;
    if (!(hand = NOpen(SCOREFILE)) || Write(hand, &sf, sizeof(sf)) < sizeof(sf))
	Error("Cannot write score file\n%s", SCOREFILE);
    if (hand)
	Close(hand);
}


void Customize(LEVEL ll)		/* Do not call in mid-play. */
{
    struct NewGadget aslider = {
	95, 20, 300, 10, null, &topaz9, 0, PLACETEXT_LEFT, null, null
    };
    struct NewWindow cusnw = {
	0, 0, 450, 106, 0, 1, SLIDERIDCMP | BUTTONIDCMP,
	WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE,
	null, null, "Parameters for custom board #0",
	null, null, 0, 0, 0, 0, CUSTOMSCREEN
    };
    struct Window *cusw = null;
    struct IntuiMessage *im;
    struct Gadget *preg, *orig = null, *mines;
    long maxwid, maxhite, maxmines, startwid, starthite, startmines, ratio;
    char line[60];
    ushort gid, centage, lastcentage = 9999;

    cusnw.Title[29] = '1' + ll - custom1;
    maxwid = (bgw->Width - LEFTMARGIN) / LSQUAREWIDTH;
    maxhite = bgw->Height / LSQUAREHEIGHT;
    if (maxwid > BOARDMAX_X)
	maxwid = BOARDMAX_X;
    if (maxhite > BOARDMAX_Y)
	maxhite = BOARDMAX_Y;
    if ((starthite = boardsizes[ll][0]) > maxhite)
	starthite = maxhite;
    if ((startwid = boardsizes[ll][1]) > maxwid)
	startwid = maxwid;
    maxmines = startwid * starthite / 2;
    if (!maxmines)
	maxmines = 1;
    if ((startmines = boardmines[ll]) > maxmines)
	startmines = maxmines;
    centage = 1000 * startmines / (startwid * starthite);

    preg = CreateContext(&orig);
    aslider.ng_VisualInfo = myvi;	/* abutton is already myvi'd */
    abutton.ng_LeftEdge = 55;		/* was 5 */
    abutton.ng_Width = 80;		/* was 60 */
    abutton.ng_TopEdge = 86;
    abutton.ng_GadgetText = "Okay";
    abutton.ng_GadgetID = 203;
    preg = CreateGadgetA(BUTTON_KIND, preg, &abutton, notags);
    abutton.ng_LeftEdge = 315;
    abutton.ng_GadgetText = "Cancel";
    abutton.ng_GadgetID = 204;
    preg = CreateGadgetA(BUTTON_KIND, preg, &abutton, notags);
    aslider.ng_GadgetText = "Width: ";
    aslider.ng_TopEdge = 20;
    aslider.ng_GadgetID = 200;
    preg = CreateGadget(SLIDER_KIND, preg, &aslider, GTSL_Min, 1L,
			    GTSL_Max, maxwid, GTSL_Level, startwid,
			    GTSL_LevelFormat, "%3ld", GTSL_MaxLevelLen, 5L,
			    GTSL_LevelPlace, PLACETEXT_RIGHT,
			    GA_RelVerify, 1L, TAG_DONE);
    aslider.ng_GadgetText = "Height:";
    aslider.ng_TopEdge = 36;
    aslider.ng_GadgetID = 201;
    preg = CreateGadget(SLIDER_KIND, preg, &aslider, GTSL_Min, 1L,
			    GTSL_Max, maxhite, GTSL_Level, starthite,
			    GTSL_LevelFormat, "%3ld", GTSL_MaxLevelLen, 5L,
			    GTSL_LevelPlace, PLACETEXT_RIGHT,
			    GA_RelVerify, 1L, TAG_DONE);
    aslider.ng_GadgetText = "Mines: ";
    aslider.ng_TopEdge = 52;
    aslider.ng_GadgetID = 202;
    mines = preg = CreateGadget(SLIDER_KIND, preg, &aslider, GTSL_Min, 1L,
			    GTSL_Max, maxmines, GTSL_Level, startmines,
			    GTSL_LevelFormat, "%3ld", GTSL_MaxLevelLen, 5L,
			    GTSL_LevelPlace, PLACETEXT_RIGHT,
			    GA_RelVerify, 1L, TAG_DONE);
    cusnw.FirstGadget = orig;
    cusnw.Screen = scr;
    cusnw.LeftEdge = (scr->Width - cusnw.Width) / 2;
    cusnw.TopEdge = (scr->Height - cusnw.Height) / 2;

    if (!preg || !(cusw = OpenWindow(&cusnw))) {
	FreeGadgets(orig);
	Error("Could not open window\nfor customizing boards");
	return;
    }
    GT_RefreshWindow(cusw, null);
    do {
	centage = 1000 * startmines / (startwid * starthite);
	if (centage != lastcentage) {
	    sprintf(line, "Density of mines = %d.%d percent  ",
					centage / 10, centage % 10);
	    Move(cusw->RPort, 73, 75);
	    SetAPen(cusw->RPort, 4);		/* blue */
	    Text(cusw->RPort, line, strlen(line));
	}
	lastcentage = centage;
	MyWaitPort(cusw->UserPort);
	gid = 0;
	im = GT_GetIMsg(cusw->UserPort);
	if (im && im->Class == IDCMP_GADGETUP)
	    switch (gid = ((struct Gadget *) im->IAddress)->GadgetID) {
		case 200:  case 201:	/* Width / Height */
		    ratio = 1000 * startwid * starthite / startmines;
		    if (ratio < 2000) ratio = 2000;
		    *(gid == 200 ? &startwid : &starthite) = im->Code;
		    maxmines = startwid * starthite / 2;
		    if (!maxmines) maxmines = 1;
		    startmines = 1000 * startwid * starthite / ratio;
		    if (!startmines) startmines = 1;
		    GT_SetGadgetAttrs(mines, cusw, null, GTSL_Max, maxmines,
					GTSL_Level, startmines, TAG_DONE);
		    break;
		case 202:			/* Mines */
		    startmines = im->Code;
		    break;
		case 203:			/* Okay */
		    boardsizes[ll][0] = starthite;
		    boardsizes[ll][1] = startwid;
		    boardmines[ll] = startmines;
		    NoticeNewLevel(lvl);
		    break;
		case 204:			/* Cancel */
		    break;
	    }
	GT_ReplyIMsg(im);
    } while (gid != 203 && gid != 204);
    CloseWindow(cusw);
    FreeGadgets(orig);
    if (gid == 203 && lvl == ll)
	NewBoard();
    while (im = (adr) GetMsg(bgw->UserPort))		/* flush events */
	ReplyMsg((adr) im);
}



void GetPlayerName(ushort nth)
{
    static struct NewGadget astring = {
	192, 26, 232, 15, "Enter your name:", &topaz9, 0,
	PLACETEXT_LEFT /* | NG_HIGHLABEL */, null, null
    };
    struct Gadget *preg, *orig = null;
    static struct NewWindow namenw = {
	0, 0, 450, 87, 0, 1, STRINGIDCMP
				| IDCMP_ACTIVEWINDOW | IDCMP_INTUITICKS,
	WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE,
	null, null, "New high score", null, null, 0, 0, 0, 0, CUSTOMSCREEN
    };
    struct Window *namw;
    struct RastPort *narp;
    struct IntuiMessage *im;
    char line[80];
    ulong class;
    ushort code, i;

    namenw.Screen = scr;
    namenw.LeftEdge = (scr->Width - namenw.Width) / 2;
    namenw.TopEdge = (scr->Height - namenw.Height) / 2;
    preg = CreateContext(&orig);
    astring.ng_VisualInfo = myvi;
    preg = CreateGadget(STRING_KIND, preg, &astring,
				GA_TabCycle, 0L, GTST_String, myname,
				GTST_MaxChars, SNAMELEN - 1, TAG_DONE);
    if (!(namenw.FirstGadget = preg) || !(namw = OpenWindow(&namenw))) {
	Error("Could not open window\nto ask player's name");
	return;
    }
/* It is documented that we can read the StringInfo's Buffer field from the  */
/* SpecialInfo of the gadget returned by CreateGadget.  Therefore it is a    */
/* normal string gadget and we can use ActivateGadget on it, and *PROBABLY*  */
/* it is okay to set the BufferPos field of the StringInfo...                */
    ((struct StringInfo *) preg->SpecialInfo)->BufferPos = strlen(myname);
    GT_RefreshWindow(namw, null);
    ClearMenuStrip(bgw);	/* cheap substitute for requester muffling */
    sprintf(line, "You got the %sbest %s score!", (nth > 1 ? "third "
				: (nth ? "second " : "")), levelabels[lvl]);
    narp = namw->RPort;
    Move(narp, (namw->Width - 10 * strlen(line)) / 2 - 1, 21);
    /* should be 2 pixels to the right of center...  ^^^ but this looks right */
    SetAPen(narp, 7);			/* yellow */
    Text(narp, line, strlen(line));
    Move(narp, 24, 52);
    SetAPen(narp, 3);			/* red */
    sprintf(line, "Previous %s high scores:", levelabels[lvl]);
    Text(narp, line, strlen(line));
    SetAPen(narp, 4);			/* blue */
    for (i = 0; i < 3; i++) {
	Move(narp, 64, 62 + 9 * i);
	sprintf(line, "%-21s  %4u", highscores[lvl][i].name,
					highscores[lvl][i].time);
	Text(narp, line, strlen(line));
    }
    ActivateGadget(preg, namw, null);
    for (;;) {
	MyWaitPort(namw->UserPort);
	if (im = GT_GetIMsg(namw->UserPort)) {
	    class = im->Class, code = im->Code;
	    GT_ReplyIMsg(im);
	    strcpy(myname, ((struct StringInfo *) preg->SpecialInfo)->Buffer);
	    for (i = 0; myname[i]; i++)
		if (myname[i] > ' ')
		    break;
	    if (class == IDCMP_GADGETUP && code != '\t' && myname[i] > ' ')
		break;		/* valid name */
	    ActivateGadget(preg, namw, null);
	}
    }
    CloseWindow(namw);
    FreeGadgets(orig);
    while (im = (adr) GetMsg(bgw->UserPort))		/* flush events */
	ReplyMsg((adr) im);
    ResetMenuStrip(bgw, &gamemenu);
}


void ShowHighScores(void)
{
    static char headers[LEVELS / 2][56];
    static struct IntuiText itexpert3 = {
	REQTEXT, REQBACK, JAM2, 5, 125, null, headers[expert], null
    }, itinterm2 = {
	REQTEXT, REQBACK, JAM2, 5, 75, null, headers[interm], &itexpert3
    }, itbeg1 = {
	REQTEXT, REQBACK, JAM2, 5, 25, null, headers[beginner], &itinterm2
    }, ithighscores = {
	3, REQBACK, JAM2, 5, 10, null,
	"               *****  HIGH SCORES  *****", &itbeg1
    };
    char scores[LEVELS][SPERLEVEL][56];
    struct IntuiText itscores[LEVELS / 2][SPERLEVEL];
    struct IntuiText *it;
    LEVEL ll, l2;
    short i;

    for (ll = beginner; ll < LEVELS / 2; ll++) {
	sprintf(headers[ll], "%s (%u × %u, %uM)                     ",
				levelabels[ll], boardsizes[ll][1],
				boardsizes[ll][0], boardmines[ll]);
	l2 = ll + LEVELS / 2;
	sprintf(headers[ll] + 29, "%s (%u × %u, %uM)", levelabels[l2],
				cuscoresizes[ll][1], cuscoresizes[ll][0],
				cuscoremines[ll]);
	for (i = 0; i < SPERLEVEL; i++) {
	    it = &itscores[ll][i];
	    *it = ithighscores;
	    it->FrontPen = 6;		/* green */
	    it->TopEdge = 10 * i + 50 * (int) ll + 35;
	    it->IText = scores[ll][i];
	    it->NextText = it + 1;
	    sprintf(scores[ll][i], "%-21s%4u    %-21s%4u",
				highscores[ll][i].name, highscores[ll][i].time,
				highscores[l2][i].name, highscores[l2][i].time);
	}
    }
    it->NextText = &ithighscores;
    HideTheBoard(&itscores[0][0]);
}


#ifdef RESETS

void ResetHighScores(void)
{
    static struct IntuiText iline = {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE, 15, 5, null,
	"Really reset high scores?", null
    },  iokay = {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
	AUTOLEFTEDGE, AUTOTOPEDGE, null, "Okay", null
    },  icancel = {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
	AUTOLEFTEDGE, AUTOTOPEDGE, null, "Cancel", null
    };
    static SCORE wurst = WORST;
    ushort i;
    LEVEL ll;

    if (AutoRequest(bgw, &iline, &iokay, &icancel, 0, 0, 320, 62))
	for (ll = beginner; ll < LEVELS; ll++)
	    for (i = 0; i < SPERLEVEL; i++)
		highscores[ll][i] = wurst;
}

#endif


void ElevateScore(ushort seconds)
{
    static SCORE wurst = WORST;
    short i, j;
    SCORE *hl = &highscores[lvl][0];
    LEVEL cl;

    if (lvl >= custom1 && (height != cuscoresizes[cl = lvl - custom1][0]
			|| width != cuscoresizes[cl][1]
			|| mines != cuscoremines[cl]) && seconds < wurst.time) {
	cuscoresizes[cl][0] = height;
	cuscoresizes[cl][1] = width;
	cuscoremines[cl] = mines;
	for (i = 0; i < SPERLEVEL; i++)
	    hl[i] = wurst;
    }
    for (i = 0; i < SPERLEVEL; i++)
	if (seconds < hl[i].time) {
	    GetPlayerName(i);
	    if (!myname[0])
		break;
	    for (j = SPERLEVEL - 2; j >= i; j--)
		hl[j + 1] = hl[j];
	    hl[i].time = seconds;
	    memset(hl[i].name, 0, SNAMELEN);	/* wipe trailing garbage */
	    strcpy(hl[i].name, myname);
	    ShowHighScores();
	    break;
	}
}


void main(void)
{
    struct DateStamp dd;
    int ret = 20, w;

    if (!(IntuitionBase = OpenL("intuition")) || !(GfxBase = OpenL("graphics")))
	exit(9999);
#ifndef AZTEC_C
    if (!PreDetachHook())	/* aztecstuff.c calls this before main() */
	exit(30);
#endif
    GadToolsBase = OpenL("gadtools");
    if (!GadToolsBase) {
	Error(NAME " " VERSION " requires\ngadtools.library");
	exit(20);
    }
    me = ThisProcess();
#ifdef BIG_STACK
    if (me->pr_StackSize < _stack) {
	Error("Stack size must be at\nleast " STACKTEXT " in " NAME " icon");
	exit(20);
    }
#endif
    DateStamp(&dd);
    srand(dd.ds_Days ^ (dd.ds_Minute << 8) ^ (dd.ds_Tick << 4));
    for (w = 0; w < SOUND_COUNT; w++)
	anyaudio |= LoadASample(w, soundargs[w]);
    ReadScoreFile();
    if (CreateDisplay()) {
	ret = 0;
	DrawEverything();
	Play();
	WriteScoreFile();
	NukeDisplay();
    } else
	Error("Could not open screen for " NAME);
    FreeSamples();
    CloseLibrary(GadToolsBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    exit(ret);
}
