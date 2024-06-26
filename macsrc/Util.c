#include "Util.h"
#include "Log.h"
#include <stdio.h>
#include <string.h>
#include "Resources.h"

GrafPtr sGrafPtr;

//	-------------------------------------------------------------------
//	Generic assertion => break into debugger
//	-------------------------------------------------------------------

int my_strlen( const char *msg )
{
	int s = 0;
	while (*msg++)
		s++;
	return s;
}

void my_strcpy( char *d, const char *s )
{
	while ((*d++=*s++))
		;
}

void my_memcpy( void *d, const void *s, unsigned long len )
{
	char *dd = d;
	const char *ss = s;
	while (len--)
		*dd++ = *ss++;
}

int my_memcmp( void *d, void *s, unsigned long len )
{
	char *dd = d;
	const char *ss = s;
	while (len--)
	{
		int r = *dd++ - *ss++;
		if (r)
			return r;
	}
	return 0;
}

void DebugLong( long l )
{
	Str255 s;
	NumToString( l, s );
	DebugStr( s );
}

void assert( int v, const char *msg )
{
	if (!v)
	{
		Str255 buffer;
		my_strcpy( (char *)(buffer+1), msg );
		buffer[0] = my_strlen( msg );
		Abort( buffer );
		DebugStr( buffer );
	}
}

//	-------------------------------------------------------------------
//	A copy of the 'DITL' resouce #136, to be able to display fatal errors even if we cannot access resources
//	-------------------------------------------------------------------

char sDITLData[] = {
	0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45,
	0x00, 0xB6, 0x00, 0x59, 0x00, 0xF0, 0x04, 0x02,
	0x4F, 0x6B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
	0x00, 0x0B, 0x00, 0x3E, 0x01, 0xBC, 0x88, 0x02,
	0x5E, 0x30 };

//	-------------------------------------------------------------------
//	Reserve memory at the start to deal with low memory conditions
//	-------------------------------------------------------------------

Handle sDITLHdl;
DialogRecord theDialog;
DialogPtr sDialogFatalError;

void InitUtilities( void )
{
	Rect theRect;

//	sDITLHdl = GetResource( 'DITL', 136 );
	sDITLHdl = NewHandle( sizeof( sDITLData ) );
	if (sDITLHdl==NULL)
	{
		DebugStr( "\pCannot allocate memory for abort dialog" );
		ExitToShell();
	}

	BlockMove( sDITLData, *sDITLHdl, sizeof( sDITLData ) );

	SetRect( &theRect, 10, 80, 502, 180 );
		//	altDBoxProc has a more 'vintage" look.
	sDialogFatalError = NewDialog( &theDialog, &theRect, "\pFatal Error", FALSE, altDBoxProc, (WindowPtr)-1, FALSE, 0, sDITLHdl );

		//	XCMD precludes us of accessing screenBits
	GetPort( &sGrafPtr );
}

//	-------------------------------------------------------------------

void DisposUtilities( void )
{
	if (sDialogFatalError)
		DisposDialog( sDialogFatalError);
	if (sDITLHdl)
		DisposHandle( sDITLHdl );
}

//	-------------------------------------------------------------------

void BreakHere( void )
{
	//	Useful place to place a debugger breakpoint
}

//	-------------------------------------------------------------------

void MessageStr( Str255 s )
{
	short item;
	ParamText( s, "", "", "" );
	ShowCursor();		//	For fullscreen crashes
	UtilPlaceWindow( sDialogFatalError, .05 );
	BringToFront( sDialogFatalError );		//	Needs to be done, if new windows appeared, ModalDialog will fail
	ShowWindow( sDialogFatalError );
	DrawDialog( sDialogFatalError );
	ModalDialog( NULL, &item );
	HideWindow( sDialogFatalError );
}

//	-------------------------------------------------------------------

void MessageLong( long l )
{
	char str[12];
	NumToString( l, str );
	MessageStr( (void *)str );
}

//	-------------------------------------------------------------------

void Abort( Str255 s )
{
	BreakHere();

	if (!sDialogFatalError)
	{
		DebugStr( s );
		ExitToShell();
	}
	MessageStr( s );
	ExitToShell();
}

//	-------------------------------------------------------------------

#define noDBGMEM

#ifdef DBGMEM
struct PtrHeader
{
	Size size;
};

static unsigned long totalAllocated = 0;

Ptr MyNewPtr( Size aSize )
{
	struct PtrHeader *p = (struct PtrHeader *)NewPtr( sizeof( struct PtrHeader ) + aSize );

//if (aSize==108)
//	Debugger();

	p->size = aSize;
	totalAllocated += aSize;

//printf( "+%lld=%lld ", p->size, totalAllocated );
//fflush( stdout );

	return (Ptr)(p+1);
}

//	-------------------------------------------------------------------

void MyDisposPtr( void *aPtr )
{
	struct PtrHeader *p = aPtr;

	totalAllocated -= p[-1].size;

//printf( "-%lld=%lld ", p[-1].size, totalAllocated );
//fflush( stdout );

	DisposPtr( p-1 );
}

//	-------------------------------------------------------------------

Size MyGetPtrSize( void *aPtr )
{
	struct PtrHeader *p = aPtr;
	return GetPtrSize( p-1 ) - sizeof( struct PtrHeader );
}

//	-------------------------------------------------------------------

void MySetPtrSize( void *aPtr, Size aSize )
{
	struct PtrHeader *p = aPtr;
	SetPtrSize( p-1, sizeof( struct PtrHeader ) + aSize );
}

//	-------------------------------------------------------------------

void DebugMem( void )
{
	DebugLong( totalAllocated );
}

#else

Ptr MyNewPtr( Size aSize ) { return NewPtr( aSize ); }
void MyDisposPtr( void *aPtr ) { DisposPtr( aPtr ); }
Size MyGetPtrSize( void *aPtr ) { return GetPtrSize( aPtr ); }
void MySetPtrSize( void *aPtr, Size aSize ) { SetPtrSize( aPtr, aSize ); }
void DebugMem( void ) {}

#endif

//	-------------------------------------------------------------------

Ptr NewPtrNoFail( Size aSize )
{
	Ptr theResult;

#ifdef VERBOSE
	printf( "ALLOCATING %ld BYTES\n", aSize );
#endif

	theResult = MyNewPtr( aSize );
	if (theResult==NULL)
		Abort( "\pFailed to allocate memory. Maybe you need to increase the memory allocated to MacFlim in the Finder" );

	{
		Size i;
		for (i=0;i!=aSize;i++)
			theResult[i] = 0xaa;
	}

	return theResult;
}

//	-------------------------------------------------------------------

Handle NewHandleNoFail( Size aSize )
{
	Handle theResult = NewHandle( aSize );
	if (theResult==NULL)
		Abort( "\pFailed to allocate memory. Maybe you need to increase the memory allocated to MacFlim in the Finder or reduce the buffersize in preferences" );
	return theResult;
}

//	-------------------------------------------------------------------

void CheckErr( short err, const char *msg )
{
#ifdef VERBOSE
	printf( "CHECK: %s => %d\n", msg, err );
#endif
	if (err!=noErr)
		assert( false, msg );
}

//	-------------------------------------------------------------------

#ifndef MINI_PLAYER

void Error( Str255 s, short err )
{
	Str255 errStr;
	
	NumToString( err, errStr );
	ParamText( s, errStr, "", "" );
	UtilModalDialog( kDLOGErrorNonFatal );
}

void CheckPtr( void *p, const char *msg )
{
	if (!p)
	{
#ifdef VERBOSE
		printf( "UNEXPECTED NULL PTR @ %s\n", msg );
#endif
		assert( false, msg );
	}
}

#endif

//	-------------------------------------------------------------------

void StrCpyPP( Str255 p, const Str255 q )
{
	my_memcpy( p, q, q[0]+1 );
}

void StrCatPC( Str255 p, const char *q )
{
	my_memcpy( p+p[0]+1, q, my_strlen( q ) );
	p[0] += my_strlen( q );
}

void StrCatPP( Str255 p, Str255 q )
{
	my_memcpy( p+p[0]+1, q+1, q[0]+1 );
	p[0] += q[0];
}

Boolean StrEquPP( Str255 p, Str255 q )
{
	if (p[0]!=q[0])
		return false;

	return !my_memcmp( p+1, q+1, p[0] );
}

//	-------------------------------------------------------------------

Boolean TestKey( unsigned char *keys, char k )
{
	return !!((keys[k>>3]>>(k&7))&1);
}
	
//	-------------------------------------------------------------------

void UtilPlaceWindow( WindowPtr window, float percentTop )
{
#define MBAR	19
#define TBAR	19

	short wl = window->portRect.left;
	short wr = window->portRect.right;
	short sl = sGrafPtr->portBits.bounds.left;
	short sr = sGrafPtr->portBits.bounds.right;

	short wt = window->portRect.top-TBAR;
	short wb = window->portRect.bottom;
	short st = sGrafPtr->portBits.bounds.top+MBAR;
	short sb = sGrafPtr->portBits.bounds.bottom;
	
	MoveWindow( window, ((sr-sl)-(wr-wl))/2, ((sb-st)-(wb-wt))*percentTop+MBAR+TBAR, FALSE );
}

//	-------------------------------------------------------------------

#define RectHeight(r)		((r).bottom-(r).top)

Size GetScreenSaveSize()
{
	return ((Size)sGrafPtr->portBits.rowBytes)*RectHeight(sGrafPtr->portBits.bounds);
}

void SaveScreen( Ptr *ptr )
{
	Size count = GetScreenSaveSize();

	*ptr = MyNewPtr( count );

	if (*ptr)
		BlockMove( sGrafPtr->portBits.baseAddr, *ptr, count );
}

//	-------------------------------------------------------------------

void RestoreScreen( Ptr *ptr )
{
	if (*ptr)
	{
		long count = ((long)sGrafPtr->portBits.rowBytes)*RectHeight(sGrafPtr->portBits.bounds);
		BlockMove( *ptr, sGrafPtr->portBits.baseAddr, count );
		MyDisposPtr( *ptr );
		*ptr = NULL;
	}
	else
	{	//	We hack it with a screen-wide window
		//	(#### Note: prob doesn't redraw the menu bar)
		//	(but the case of not enough memory to save screen, but enough memory to run the UI seems weird)
		WindowPtr w = NewWindow( NULL, &sGrafPtr->portBits.bounds, "\p", TRUE, plainDBox, (WindowPtr)-1, FALSE, 0);
		if (w)
		{
			DisposeWindow( w );
		}
	}
}

//	-------------------------------------------------------------------

OSErr UtilGetFileTypeCreator( Str255 fName, short vRefNum, long dirID, OSType *type, OSType *creator )
{
	HParamBlockRec pb;
	OSErr err;

	pb.fileParam.ioCompletion = NULL;
	pb.fileParam.ioVRefNum = vRefNum;
	pb.fileParam.ioFVersNum = 0;
	pb.fileParam.ioFDirIndex = 0;
	pb.fileParam.ioNamePtr = fName;
	pb.fileParam.ioDirID = dirID;

	err = PBHGetFInfo( &pb, FALSE );
	if (err)
		return err;

	if (type)
		*type = pb.fileParam.ioFlFndrInfo.fdType;
		
	if (*creator)
		*creator = pb.fileParam.ioFlFndrInfo.fdCreator;

	return noErr;
}

//	-------------------------------------------------------------------

#ifndef MINI_PLAYER

OSErr UtilSetFileTypeCreator( Str255 fName, short vRefNum, long dirID, OSType type, OSType creator )
{
	HParamBlockRec pb;
	OSErr err;

	pb.fileParam.ioCompletion = NULL;
	pb.fileParam.ioVRefNum = vRefNum;
	pb.fileParam.ioFVersNum = 0;
	pb.fileParam.ioFDirIndex = 0;
	pb.fileParam.ioNamePtr = fName;
	pb.fileParam.ioDirID = dirID;

	err = PBHGetFInfo( &pb, FALSE );
	if (err)
		return err;

	pb.fileParam.ioDirID = dirID;			//	This is actually mandatory
	pb.fileParam.ioFlFndrInfo.fdType = type;
	pb.fileParam.ioFlFndrInfo.fdCreator = creator;

	err = PBHSetFInfo( &pb, FALSE );

	return err;
}

//	-------------------------------------------------------------------

void UtilModalDialog( short dlogID )
{
	DialogPtr dialog = UtilPlaceDialog( dlogID );
	short itemHit;

	ModalDialog( NULL, &itemHit );
	DisposDialog( dialog );
}

//	-------------------------------------------------------------------

DialogPtr UtilPlaceDialog( short dlogID )
{
	DialogPtr dialog = GetNewDialog( dlogID, NULL, (WindowPtr)-1 );

	assert( dialog!=NULL, "GetNewDialog" );

	UtilPlaceWindow( dialog, 0.1 );
	ShowWindow( dialog );
	DrawDialog( dialog );
	ShowCursor();
	
	return dialog;
}

//	-------------------------------------------------------------------

void DebugStrLong( Str255 s, long l )
{
	char lStr[12];
	Str255 buffer;
	int i;

	lStr[0] = 0;
	NumToString( l, lStr );

	StrCpyPP( buffer, s );
	StrCatPP( buffer, (void *)lStr );

	DebugStr( lStr );
}

//	-------------------------------------------------------------------

static void *UtilStringFromErr( OSErr err )
{
	return "\p(Unknown error)";
}

//	-------------------------------------------------------------------

void UtilErrToString( OSErr err, Str255 str )
{
	Str255 str2;
	str[0] = 0;
	str2[0] = 0;

	StrCatPP( str, UtilStringFromErr( err ) );
	NumToString( err, str2 );
	StrCatPP( str, "\p (" );
	StrCatPP( str, str2 );
	StrCatPP( str, "\p)" );
}

#else

void UtilModalDialog( short dlogID ) {}

#endif



