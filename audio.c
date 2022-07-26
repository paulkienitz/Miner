/* asynchronous one-shot sample loading/playing routines, fairly */
/* generic, used by Miner but easy to use with other stuff.      */

#include <exec/memory.h>
#include <devices/audio.h>
#include <dos/dos.h>
#include <paul.h>

void BeginIO(struct IORequest *ior);

/* from datatypes/soundclass.h: */
#define CMP_NONE     0
#define CMP_FIBDELTA 1

struct VoiceHeader
{
    ULONG		 vh_OneShotHiSamples;
    ULONG		 vh_RepeatHiSamples;
    ULONG		 vh_SamplesPerHiCycle;
    UWORD		 vh_SamplesPerSec;
    UBYTE		 vh_Octaves;
    UBYTE		 vh_Compression;
    ULONG		 vh_Volume;
};


#define AUDIO_PRIORITY 20
#define DEFAULT_SPEED  16000
#ifndef MAX_SAMPLES
#  define MAX_SAMPLES    20
#endif


private ustr bodies[MAX_SAMPLES];	/* chip ram sample data pointers */
private ulong lengths[MAX_SAMPLES], speeds[MAX_SAMPLES];
private bool stereo[MAX_SAMPLES];

private bool used = false;
private struct MsgPort *mpa = null;
private struct IOAudio ioa, ioa2;
#define ioar  ioa.ioa_Request
#define ioar2 ioa2.ioa_Request


/* called automatically if you don't use it before StartSound */
bool InitAudio(void)
{
    static ubyte unitlist[] = { 12, 10, 5, 3, 8, 4, 2, 1 };
    ushort i, j, u;

    if (mpa)
	return true;
    if (!(mpa = CreateMsgPort()))
	return false;
    ioar.io_Message.mn_ReplyPort = mpa;
    ioar.io_Message.mn_Node.ln_Pri = AUDIO_PRIORITY;
    ioa.ioa_Data = unitlist;
    ioa.ioa_Length = sizeof(unitlist);
    if (OpenDevice(AUDIONAME, 0L, &ioar, 0L)) {
	DeleteMsgPort(mpa);
	mpa = null;
	return false;
    }
    u = (ulong) ioar.io_Unit;
    ioar.io_Message.mn_Node.ln_Pri = AUDIO_PRIORITY;
    ioar.io_Message.mn_Node.ln_Type = 0;
    ioar.io_Command = CMD_WRITE;
    ioar.io_Flags = ADIOF_PERVOL;
    ioa.ioa_Cycles = 1;
    for (j = 0; j < 4; j++) {
	i = 1 << (4 - j & 3);	/* 1, 8, 4, 2 -- avoid reversing stereo */
	if (u & i) {
	    if (u != i) {
		ioa2 = ioa;
		ioar.io_Unit = (adr) i;
		ioar2.io_Unit = (adr) (u & ~i);
	    } else
		ioar2.io_Device = null;
	    break;
	}
    }
    return true;
}


/* call to be friendly to other apps that want sound */
void ReleaseChannels(void)
{
    if (!mpa) return;
    CloseDevice(&ioar);
    DeleteMsgPort(mpa);
    mpa = (adr) ioar.io_Device = ioar2.io_Device = null;
    used = false;
}


bool StartSound(ushort which, bool waitfinish, ushort volume)
{
    if (which >= MAX_SAMPLES || !bodies[which])
	return false;
    if (!mpa && !InitAudio())
	return false;
    if (used) {
	if (!waitfinish) {
	    if (!CheckIO(&ioar))
		AbortIO(&ioar);
	    if (ioar2.io_Device && !CheckIO(&ioar2))
		AbortIO(&ioar2);
	}
	WaitIO(&ioar);
	if (ioar2.io_Device)
	    WaitIO(&ioar2);
    } else
	used = true;
    ioa.ioa_Data = bodies[which];
    if (stereo[which]) {
	ioa.ioa_Length = ioa2.ioa_Length = (lengths[which] / 2) & ~1L;
	ioa2.ioa_Data = bodies[which] + (lengths[which] & ~1L) - ioa.ioa_Length;
    } else {
	ioa.ioa_Length = ioa2.ioa_Length = lengths[which] & ~1L;
	ioa2.ioa_Data = ioa.ioa_Data;
    }
    ioa.ioa_Period = ioa2.ioa_Period = 3579547 / speeds[which];
    ioa.ioa_Volume = ioa2.ioa_Volume = volume;
    BeginIO(&ioar);
    if (ioar2.io_Device)
	BeginIO(&ioar2);
    return true;
}


void FibDeltaUnpack(register str start, str end, register str out)
{
    static char delta[16] = {
	-34, -21, -13, -8, -5, -3, -2, -1, 0, 1, 2, 3, 5, 8, 13, 21
    };
    register char current = start[1];
    register ubyte v;

    for (start += 2; start < end; start++) {
	v = (ubyte) *start;
	*out++ = current += delta[v >> 4];
	*out++ = current += delta[v & 0xF];
    }
}


/* This function is loosely based on code by Richard Lee Stockton. */
/* Does not support looping or envelopes.                          */

bool LoadASample(ushort which, str filename)
{
    BPTR fh;
    ubyte tbuf[256];
#define tbufIL ((ulong *) (tbuf + i))
    ushort i, l, compression;
    long start, rawlength;

    if (!filename[0] || which >= MAX_SAMPLES)
	return false;
    if (bodies[which])
	FreeMem(bodies[which], lengths[which]);
    stereo[which] = false;
    compression = rawlength = lengths[which] = speeds[which] = 0;
    bodies[which] = null;
    if (!(fh = OOpen(filename)))
	return false;
    l = Read(fh, tbuf, 256);
    if (l <= 0)
	goto fail;
    if (l > 12 && ((ulong *) tbuf)[2] == '8SVX') {
	for (i = 12; i < l; i += (tbufIL[1] + 9) & ~1L) {
	    if (tbufIL[0] == 'VHDR') {           /* samples per second */
		struct VoiceHeader *vhed = (adr) (tbuf + i + 8);
		speeds[which] = vhed->vh_SamplesPerSec;
		compression = vhed->vh_Compression;
	    } else if (tbufIL[0] == 'CHAN') {  	 /* channel assignment */
		if (tbuf[i + 7] == 6 || tbuf[i + 11] == 6)
		    stereo[which] = true;
	    } else if (tbufIL[0] == 'BODY') {    /* size of sound data */
		rawlength = tbufIL[1];
		start = i + 8;
		break;
	    }
	}
    }
#ifdef HANDLE_RAW_SAMPLES
    if (!speeds[which] || !rawlength) {
	speeds[which] = DEFAULT_SPEED;
	Seek(fh, 0, OFFSET_END);
	rawlength = Seek(fh, 0, OFFSET_BEGINNING);     /* size of file */
	start = 0;
    }
#endif
    if (rawlength <= 0 || compression > CMP_FIBDELTA)
	goto fail;
    if (stereo[which]) {
	lengths[which] = compression ? 2 * (rawlength - 4) : rawlength;
	if (lengths[which] > 262144 || rawlength & 1)
	    goto fail;
    } else {
	lengths[which] = compression ? 2 * (rawlength - 2) : rawlength;
	if (lengths[which] > 131072)
	    lengths[which] = 131072;
	if (rawlength > lengths[which])
	    if (compression)
		goto fail;
	    else
		rawlength = lengths[which];
    }
    if (bodies[which] = AllocCP(lengths[which])) {    /* chip ram */
	Seek(fh, start, OFFSET_BEGINNING);
	start = lengths[which] - rawlength;           /* 0 when !compression */
	if (Read(fh, bodies[which] + start, rawlength) < rawlength) {
	    FreeMem(bodies[which], lengths[which]);
	    bodies[which] = null;
	}
	if (compression == CMP_FIBDELTA) {
	    if (stereo[which]) {
		str split = bodies[which] + (lengths[which] - rawlength / 2);
		FibDeltaUnpack(bodies[which] + start, split, bodies[which]);
		FibDeltaUnpack(split,  bodies[which] + lengths[which],
				bodies[which] + lengths[which] / 2);
	    } else
		FibDeltaUnpack(bodies[which] + start,
				bodies[which] + lengths[which], bodies[which]);
	}
    }
  fail:
    Close(fh);
    return !!bodies[which];
}


void FreeSamples(void)
{
    ushort w;
    if (used) {
	if (!CheckIO(&ioar))
	    AbortIO(&ioar);
	WaitIO(&ioar);
	if (ioar2.io_Device) {
	    if (!CheckIO(&ioar2))
		AbortIO(&ioar2);
	    WaitIO(&ioar2);
	}
    }
    for (w = 0; w < MAX_SAMPLES; w++)
	if (bodies[w]) {
	    FreeMem(bodies[w], lengths[w]);
	    bodies[w] = null;
	}
    ReleaseChannels();
}
