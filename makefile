# makefile for MineSwatter using Aztec C:

# II is a precompiled header file containing exec/exec.h, dos/dosextens.h,
# exec and dos prototypes and inline pragmas, and string.h.  INTU is another
# one containing all that plus intuition/intuition.h and the complete set of
# all clib/*.h prototypes and pragmas.

PRE = -hi INTU
FOBS = miner.o action.o images.o audio.o aztecstuff.o

F = -pe -wcru -sabfmnpu
O = -d AUTOFLASH -d FIDDLE_FONT
# Compiling options that can be set are:
#   AUTOFLASH makes win/lose color flashing happen simultaneously with other
#     events, instead of making other stuff wait as releases 3 and earlier did.
#   BIG_STACK uses a simple recursive method for filling in blank areas, that
#     may require 50K or more of stack to be safe with board sizes that can be
#     created in high resolution screen modes.  Noticeably faster than the other
#     method used in release 4 and up, which uses no large amounts of stack.
#   FIDDLE_FONT uses a fancy kluge to make the underscores on the three button
#     gadgets appear one pixel lower than they normally would.
#   HITBOMB_UP makes bombs that you step on appear with a raised border instead
#     of a recessed one.
#   RESETS adds a "Reset scores" option to the "Game" menu.

CFLAGS = $F $(PRE) $O

ram\:Miner : $(FOBS)
	ln +q -m +cd -o ram:Miner $(FOBS) -lc
	-@dr -l ram:Miner\#?

aztecstuff.o : aztecstuff.c
	cc $F -hi II aztecstuff

audio.o : audio.c
	cc $F audio

BOBS = miner.bo action.bo images.bo audio.bo aztecstuff.bo

b : ram:BMiner

ram\:BMiner : $(BOBS)
	ln +q -g -w -m +cd -o ram:BMiner $(BOBS) -lc
	-@dr -l ram:BMiner\#?

.c.bo :
	cc $(CFLAGS) -d DEBUG -bs -s0f0n -o $@ $<

aztecstuff.bo : aztecstuff.c
	cc $F -hi II -d DEBUG -bs -s0f0n -o aztecstuff.bo aztecstuff.c

audio.bo : audio.c
	cc $F -d DEBUG -bs -s0f0n -o audio.bo audio.c

action.o miner.o action.bo miner.bo : miner.h
