/* Startup code and so forth for Aztec C, causing it to detach from CLI  */
/* and not bother setting up stdio or argv or other stuff we don't need. */
/* DO NOT link this file to anything compiled with other C compilers.    */

#include <exec/memory.h>
#include <dos/dosextens.h>
#include <stdlib.h>
#include <paul.h>

#ifdef DEBUG
#  define main Dmain	/* MUST also be done in main program source file!! */
#endif


import char processname[];
import long _stack;

void main(void);
bool PreDetachHook(void);


int Enable_Abort;
void *DOSBase;
struct Message *WBenchMsg;

private void *_oldtrap, **_trapaddr, (*_cln)(void);

private bool detested = true;
private BPTR *detseg = (adr) ~0, progdir = 0;
private struct MemList *mem = (adr) ~0;
private ulong memsz = ~0;


void exit(int ret)
{
    if (_cln)
	(*_cln)();
    if (_trapaddr)
	*_trapaddr = _oldtrap;
    SetProgramDir(0);
    if (progdir)
	UnLock(progdir);
    CloseLibrary(DOSBase);
    if (WBenchMsg) {
	Forbid();
	ReplyMsg(WBenchMsg);
    }
    Exit((long) ret);
}


private void do_detach(struct Process *pp)
{
    struct CommandLineInterface *cli;
    BPTR *ss;
    long c;

    if (!detested) {			/* do this to survive detachment */
	for (c = 0, ss = detseg; ss; ss = gbip(*ss)) {
	    mem->ml_me[c].me_Addr = (adr) (ss - 1);
	    mem->ml_me[c++].me_Length = ss[-1];
	}
	AddTail((struct List *) &pp->pr_Task.tc_MemEntry, &mem->ml_Node);
	pp->pr_ConsoleTask = null;
	SetProgramDir(progdir);
	pp->pr_WindowPtr = null;
    } else if (pp->pr_CLI) {		/* do this to get detached */
	detested = false;
	cli = gbip(pp->pr_CLI);
	progdir = DupLock(GetProgramDir());
	detseg = gbip(cli->cli_Module);
	for (c = 0, ss = detseg; ss; c++, ss = gbip(*ss)) ;
	memsz = sizeof(struct MemList) + (c - 1) * sizeof(struct MemEntry);
	if (!progdir || !(mem = AllocP(memsz)))
	    goto failure;
	mem->ml_NumEntries = c;
	mem->ml_Node.ln_Name = null;
	mem->ml_Node.ln_Pri = 0;
	mem->ml_Node.ln_Type = NT_MEMORY;
	if ((c = pp->pr_Task.tc_Node.ln_Pri) > 5)
	    c = 5;
	if (!_stack)
	    _stack = cli->cli_DefaultStack * 4;
	if (CreateProc(processname, c, cli->cli_Module, _stack))
	    cli->cli_Module = 0;
	else {
	    FreeMem(mem, memsz);
	    goto failure;
	}
	Exit(0);   /* do NOT clean up! */
      failure:
	if (progdir)
	    UnLock(progdir);
	if (Output())
	    Write(Output(), "\nNO MEMORY!\n", 12);
	exit(20);
    }					/* else WB launch -- do nothing */
}


void _main(long alen, char *aptr)
{
    struct Process *pp = ThisProcess();
    if (!pp->pr_CLI && detested) {
	WaitPort(&pp->pr_MsgPort);
	WBenchMsg = GetMsg(&pp->pr_MsgPort);
    }
    if (PreDetachHook()) {
#ifndef DEBUG
	do_detach(pp);
#endif
	main();
	exit(0);
    } else
	exit(20);
}
