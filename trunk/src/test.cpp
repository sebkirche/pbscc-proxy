/* This is simple demonstration of how to use expat. This program
   reads an XML document from standard input and writes a line with
   the name of each element to standard output indenting child
   elements by one tab stop more than their parent element.
   It must be used with Expat compiled for UTF-8 output.
*/

#include <windows.h>
#include <stdio.h>
//#include "expat.h"
#include "pbscc.h"
#include "mstring.h"
#include "entries.h"
#include "svninfo.h"

#define BUF_SIZE	512
#define XML_FMT_INT_MOD "l"

extern FILE	*logFile;

long timer(long t,char * msg){
	long i=GetTickCount();
	if(t==0)t=i;
	printf("*** %s timer = %i [%i] **\n",msg,i-t,t);
	return i;
}

long outproc (LPCSTR msg, DWORD len) {
	printf("*** outproc: ");
	for(DWORD i=0; i<len; i++){
		putchar(msg[i]);
	}
	putchar('\n');
	return 0;
}


bool _entries_scanwc_callback(SVNENTRY*e,void*udata) {
	THECONTEXT* ctx=(THECONTEXT*)udata;
	if( !strcmp(e->kind,"dir") && e->name[0] ){
		mstring s=mstring(e->wcpath);
		s.addPath(e->name);
		entries_scan(s.c_str(), &_entries_scanwc_callback, udata , ctx->svnwd);
	}else if( !strcmp(e->kind,"file") ){
		ctx->svni->add(ctx->lpProjName,e->wcpath,e->name,e->revision,e->lockowner);
	}
	return true;
}

/** Scans work copy and builds in-memory cache */
//svni could be a part of the context
bool ScanWC(THECONTEXT* ctx) {
	ctx->svni->reset();
	return entries_scan(ctx->lpProjName, &_entries_scanwc_callback, (void*) ctx , ctx->svnwd);
}




int main(int argc, char *argv[]) {
	long t=0;
	int i;
	LONG lpSccCaps,pnCheckoutCommentLen,pnCommentLen;
	logFile=stdout;
	t=timer(t,"start");
	if(argc==2){
		char*wc=argv[1];
		
		THECONTEXT*ctx;
		
		SccInitialize( (LPVOID *) &ctx, NULL, "caller","scc", &lpSccCaps, "aux", &pnCheckoutCommentLen, &pnCommentLen);
		SccOpenProject(ctx, NULL, "root", wc, "pb-workspace", "aux", "comment", outproc,0);
		
		/*
		char buf[5000]; //TODO: dangerous code
		PASCALSTR ps;
		
		t=timer(t,"start build cahce");
		for(i=0;i<5000;i++){
			strcpy(buf,ctx->lpProjName);
			ps.ptr=buf;
			ps.len=strlen(buf);
			BuildCache(ctx,&ps);
		}
		
		t=timer(t,"end build cahce");
		*/
		t=timer(t,"start scan");
		
		for(i=0;i<5000;i++){
			ScanWC(ctx);
		}
		t=timer(t,"end scan");
		for(i=0;i<ctx->svni->getCount();i++)ctx->svni->print(ctx->svni->get(i));
		printf("count=%i\n\n\n",ctx->svni->getCount());
		
		SVNINFOITEM*e=ctx->svni->get("d:\\xxx","d:\\xxx\\test2\\src\\libs\\lib1\\pbtestx.sra");
		ctx->svni->print(e);
		
		
		SccUninitialize(ctx);
		
		
	}else{
		printf("specify local work copy as a parameter\n");
	}
	t=timer(t,"end");
	return 0;
}
