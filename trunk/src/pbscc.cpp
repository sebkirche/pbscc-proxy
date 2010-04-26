/*
 * Copyright 2010 Dmitry Y Lukyanov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "pbscc.h"
#include "res.h"
#include <shlobj.h>
#include <io.h>
#include <process.h>
#include <wininet.h>
#include "todo.h"
#include "easystr.h"
#include "entries.h"
#include "filecmp.h"
#include "conproc.h"
#include "tmp\version.h"
#include "FCNTL.H"

HINSTANCE	hInstance;
FILE		*logFile=NULL;
CHAR		*gpSccName="PBSCC Proxy";
CHAR		*gpSccVersion="version 2010-02-27";

HWND		consoleHwnd=NULL;
CHAR        filesubst[4000];
CHAR        _svn[11]=".svn"; //svn work directory

void log(const char* szFmt,...) {
	if(logFile){
		va_list args;
		va_start(args, szFmt);
		vfprintf(logFile, szFmt, args);
		fflush(logFile);
		va_end(args);
	}
}

bool ShowSysError(char*info,int err){
	LPVOID lpMsgBuf;
	if(err==0)err=GetLastError();
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		(LPTSTR) &lpMsgBuf, 0, NULL );
	log("System Error: %s : %s\n",info,lpMsgBuf);
	LocalFree( lpMsgBuf );
	return false;
}


BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD ul_reason_for_call, void* lpReserved) {
	switch (ul_reason_for_call){
		case DLL_PROCESS_ATTACH:{
			hInstance=hInst;
			HKEY rkey;
			DWORD type=REG_SZ;
			char buf[1024];
			DWORD buflen=sizeof(buf);
			if( RegOpenKey(PBSCC_REGKEY,PBSCC_REGPATH,&rkey)==ERROR_SUCCESS){
				if(RegQueryValueEx(rkey,"log.path",NULL,&type,(LPBYTE)buf,&buflen)==ERROR_SUCCESS){
					if(!logFile)logFile=fopen(buf,"at");
					log("\n\n---------------------------------------------------------\n"
						"DllMain DLL_PROCESS_ATTACH hInst=%X\n",hInst);
				}
				RegCloseKey(rkey);
			}
			
			break;
		}
		case DLL_PROCESS_DETACH:{
			if(logFile){
				log("DllMain DLL_PROCESS_DETACH\n");
				fclose(logFile);
				logFile=NULL;
			}
			break;
		}
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
	}
	return TRUE;
} 

//TODO: dangerous code
//substitutes the local file path by project path
//returns reference to the internal buffer with substituted path
char * _subst(THECONTEXT*ctx,char * file){
	strcpy(filesubst,ctx->lpProjName);
	strcpy(filesubst+ctx->cbProjName,file+ctx->cbProjPath);
	return filesubst;
}


//send to app message text
//splitting text by carrage return
void _msg(THECONTEXT*ctx,char * s){
	if(ctx->lpOutProc) {
		ctx->lpOutProc(s,strlen(s));
/*		
		char * ss=s;
		char * se=ss;
		
		while( ss[0] ){
			//find end of string
			while ( se[0]!=0 && se[0]!='\n' && se[0]!='\r' ) se++;
			//if it's diff from string start, send it to client
			if( se!=ss ){
				ctx->lpOutProc(ss,se - ss);
			}
			//find start of the next string
			while ( se[0]=='\n' || se[0]=='\r' ) se++;
			ss=se;
		}
*/
	}
}




BOOL GetProperty(char*fname,char*pname,char*pvalue,int pvlen){
	char c;
	int len;
	bool b=false;
	char buf[1000];
	FILE*f=fopen(fname,"rt");
	pvalue[0]=0;
	if(f){
		while(!b){
			if(fscanf(f,"%c %i\n",&c,&len)!=2)break;
			if(!fgets(buf,sizeof(buf),f))break;
			if(!strcmp(rtrim(buf),pname))b=true;
			if(fscanf(f,"%c %i\n",&c,&len)!=2)break;
			if(!fgets(buf,sizeof(buf),f))break;
		}
		if(b){
			strncpy(pvalue,buf,pvlen);
			pvalue[pvlen-1]=0;
			rtrim(pvalue);
		}
		fclose(f);
	}
	return b;
}

typedef struct {
	PASCALSTR*ps;
	long prevrev;
} BUILDCACHE;

bool _BuildCacheCallback(SVNENTRY*e,void*udata){
	BUILDCACHE *d=(BUILDCACHE *)udata;
	char lockby[SCC_USER_LEN+1]="";
	if(e->revision>0){
		if(d->prevrev < e->revision){
			sprintf(EOPS(d->ps),"\\%s\\prop-base\\%s.svn-base",_svn,e->name);
			GetProperty(d->ps->ptr,"lockby",lockby,sizeof(lockby));
			sprintf(EOPS(d->ps),"\\%s\\cache\\%s.cch",_svn,e->name);
			if( strcmp(e->scedule,"delete") ){
				FILE*fcch=fopen(d->ps->ptr,"wt");
				if(fcch){
					fprintf(fcch,"%d\n%s\n",e->revision, lockby);
					fflush(fcch);
					fclose(fcch);
				}
			}else{
				DeleteFile(d->ps->ptr);
			}
		}
	}
	return true;
}

BOOL BuildCache(THECONTEXT*ctx,PASCALSTR*ps){
	WIN32_FIND_DATA ffd;
	HANDLE ffh;
	FILE*fent;
	BUILDCACHE bc;
	char rev[PBSCC_REVLEN]="";
	
	//log("\tBuildCache %5d %s\n",ps->len, ps->ptr);
	sprintf(EOPS(ps),"\\%s",_svn);
	if(!access(ps->ptr,0)){
		strcat(EOPS(ps),"\\cache");
		if(access(ps->ptr,0)){
			if(!CreateDirectory(ps->ptr,NULL)){
				_msg(ctx,"can't create cache directory");
				_msg(ctx,ps->ptr);
				log("Error: can't create cache directory \"%s\"\n",ps->ptr);
				return false;
			}
		}
		//get the previous folder revision
		bc.prevrev=0;
		bc.ps=ps;
		strcat(EOPS(ps),"\\.cch");
		fent=fopen(ps->ptr,"rt");
		if(fent){
			fgets(rev,sizeof(rev),fent);
			rtrim(rev);
			bc.prevrev=atol(rev);
			fclose(fent);
		}
		sprintf(EOPS(ps),"\\%s\\entries",_svn);
		if(!entries_scan(ps->ptr, &_BuildCacheCallback, (void*) &bc )){
			_msg(ctx,"can't find entries file");
			_msg(ctx,ps->ptr);
			log("Error: can't find entries file \"%s\"\n",ps->ptr);
			return false;
		}
	}else log("\tnot found: %s\n",ps->ptr);
	
	strcpy(EOPS(ps),"\\*");
	ffh=FindFirstFile(ps->ptr,&ffd);
	if(ffh!=INVALID_HANDLE_VALUE){
		do{
			if(ffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY && strcmp(ffd.cFileName,_svn) && strcmp(ffd.cFileName,".") && strcmp(ffd.cFileName,"..")){
				strcpy(EOPS(ps)+1,ffd.cFileName);
				PASCALSTR ps2;
				ps2.ptr=ps->ptr;
				ps2.len=strlen(ps->ptr);
				BuildCache(ctx,&ps2);
			}
		}while(FindNextFile(ffh,&ffd));
		FindClose(ffh);
	}
	return true;
	
}

bool _files2list(THECONTEXT*ctx, LONG nFiles, LPCSTR* lpFileNames){
	FILE*f=fopen(ctx->lpTargetsTmp,"wt");
	if(f){
		for(int i=0;i<nFiles;i++){
			fputs( _subst( ctx, (char*)lpFileNames[i] ), f );
			fputs( "\n", f );
		}
		
		fflush(f);
		fclose(f);
		return true;
	}
	return false;
}

bool _msg2file(THECONTEXT*ctx, char*prefix){
	FILE*f=fopen(ctx->lpMsgTmp,"wt");
	if(f){
		if(prefix)fputs( prefix , f );
		fputs( ctx->lpComment , f );
		fflush(f);
		fclose(f);
		return true;
	}
	return false;
}

BOOL _scccommit(THECONTEXT*ctx,SCCCOMMAND icmd){
	char * cmd=NULL;
	switch(icmd){
		case SCC_COMMAND_CHECKOUT:{
			cmd="svn commit --non-interactive --targets \"%s\" -m \"scc check out\"";
			break;
		}
		case SCC_COMMAND_UNCHECKOUT:{
			cmd="svn commit --non-interactive --targets \"%s\" -m \"scc undo check out\"";
			break;
		}
		case SCC_COMMAND_CHECKIN:{
			_msg2file(ctx,"scc check in : ");
			cmd="svn commit --non-interactive --targets \"%s\" --file \"%s\"";
			ctx->dwLastCommitTime = GetTickCount();
			break;
		}
		case SCC_COMMAND_ADD:{
			_msg2file(ctx,"scc add : ");
			cmd="svn commit --non-interactive --targets \"%s\" --file \"%s\"";
			ctx->dwLastCommitTime = GetTickCount();
			ctx->isLastAddRemove = true;
			break;
		}
		case SCC_COMMAND_REMOVE:{
			_msg2file(ctx,"scc remove : ");
			cmd="svn commit --non-interactive --targets \"%s\" --file \"%s\"";
			ctx->dwLastCommitTime = GetTickCount();
			ctx->isLastAddRemove = true;
			break;
		}
		default : return false;
	}
	if(!_execscc(ctx, cmd, ctx->lpTargetsTmp, ctx->lpMsgTmp)){
		return false;
	}
	return true;
}


BOOL _sccupdate(THECONTEXT*ctx,BOOL force=false){
	
	if(GetTickCount() - ctx->dwLastUpdateTime > ctx->cacheTtlMs || force){
		log("Update repository.\n");
		if(!_execscc(ctx,"svn update --non-interactive \"%s\"",ctx->lpProjName))return false;
		if(ctx->pipeOut->match("^[CG] ")){
			_msg(ctx,"Conflict during updating repository.");
			_msg(ctx,"Please resolve all conflicts manually to continue work.");
		}
		
		PASCALSTR ps;
		char buf[5000]; //TODO: dangerous code
		strcpy(buf,ctx->lpProjName);
		ps.ptr=buf;
		ps.len=strlen(buf);
		BuildCache(ctx,&ps);
	}
	ctx->dwLastUpdateTime=GetTickCount();
	return true;
}



BOOL CALLBACK TheEnumWindowsProc(HWND hwnd,LPARAM lParam){
	
	DWORD pid;
	GetWindowThreadProcessId(hwnd,&pid);
	if(pid==GetCurrentProcessId()){
		char c[100]="";
		GetClassName(hwnd, c, sizeof(c));
		if(!strcmp(c,"ConsoleWindowClass")){
			log("console window = %X\n",hwnd);
			consoleHwnd=hwnd;
			ShowWindow(hwnd,SW_HIDE);
			return false;
		}
	}
	return true;
}


void addUniqueListItem(HWND hWnd,int ID,char*c){
	int i=SendDlgItemMessage(hWnd,ID,CB_FINDSTRINGEXACT,-1,(LPARAM)c);
	if(i==CB_ERR){
		SendDlgItemMessage(hWnd,ID,CB_ADDSTRING,0,(LPARAM)c);
	}
}

char*_unquote(char*c){
	int i=strlen(c);
	if(c[0]=='"' && c[i-1]=='"'){c[i-1]=0;c++;}
	return c;
}


BOOL CALLBACK DialogProcComment(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam){
	switch(msg){
		case WM_CLOSE:
			EndDialog(hwnd,0);
			break;
		case WM_MANAGEOK:{
				THECONTEXT*ctx;
				ctx=(THECONTEXT*)GetWindowLong(hwnd,GWL_USERDATA);
				BOOL enableOK;
				enableOK=TRUE;
				enableOK &= ( SendDlgItemMessage(hwnd,IDC_COMBO_MSG,WM_GETTEXTLENGTH,0,0) >0 );

				EnableWindow( GetDlgItem(hwnd,IDOK)  , enableOK );
				break;
			}
		case WM_COMMENTHST:{
			//(bool)wParam:  true=store list, false=loda list
			//lParam: if wParam=true, lParam = buffer with current text.
			HKEY rkey;
			int i;
			DWORD type=REG_SZ;
			char buf[PBSCC_MSGLEN+3];
			char key[10];
			DWORD buflen=sizeof(buf);
			if( RegOpenKey(PBSCC_REGKEY,PBSCC_REGPATH,&rkey)==ERROR_SUCCESS){
				if(wParam){
					//find/replace old value in a list.
					i=(int)SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_FINDSTRINGEXACT,-1,lParam);
					if(i!=CB_ERR)SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_DELETESTRING,i,0);
					SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_INSERTSTRING,0,lParam);
				}
				for(i=0;i<10;i++){
					sprintf(key,"cmt.%02i",i);
					if(wParam){
						if(i< SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_GETCOUNT,0,0) ){
							if( SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_GETLBTEXTLEN,i,0)<=PBSCC_MSGLEN ){
								SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_GETLBTEXT,i,(LPARAM) buf);
								RegSetValueEx(rkey,key,0,REG_SZ	,(LPBYTE)buf,(DWORD) strlen(buf)+1);
							}
						}
					}else{
						int err;
						buflen=PBSCC_MSGLEN+2;
						err=RegQueryValueEx(rkey,key,NULL,&type,(LPBYTE)buf,&buflen);
						if(err==ERROR_SUCCESS){
							buf[PBSCC_MSGLEN]=0;//just in case. to prevent longer rows.
							SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_ADDSTRING,0,(LPARAM)buf);
						}else{
							break;
						}
					}
				}
				RegCloseKey(rkey);
			}
			break;
		}
		case WM_SIZE:
			//no resize implemented
			break;
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDCANCEL:
					SendMessage(hwnd,WM_CLOSE,0,0);
					break;
				case IDOK:
					THECONTEXT*ctx;
					char buf[PBSCC_CMTLENALL];
					ctx=(THECONTEXT*)GetWindowLong(hwnd,GWL_USERDATA);
					SendDlgItemMessage(hwnd,IDC_COMBO_MSG,WM_GETTEXT,PBSCC_MSGLEN+1,(LPARAM) buf);
					strcpy(ctx->lpComment,buf);
					//store entered message
					SendMessage(hwnd,WM_COMMENTHST,true, (LPARAM)buf);
					//close window
					SendMessage(hwnd,WM_CLOSE,0,0);
					break;
				case IDC_COMBO_MSG:
					switch( HIWORD(wParam) ){
						case CBN_EDITCHANGE:
						case CBN_SELCHANGE:
						case CBN_CLOSEUP:
							PostMessage(hwnd,WM_MANAGEOK,0,0);
							break;
					};
					break;
			}
			return 1;   
		case WM_INITDIALOG:{
			THECONTEXT *ctx;
			LONG i;
			char *sCommand;
			ctx=(THECONTEXT*)lParam ;
			SetWindowLong(hwnd,	GWL_USERDATA,(LONG) lParam);
			SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_LIMITTEXT,PBSCC_MSGLEN,0);
			
			
			switch(ctx->eSCCCommand){
				case SCC_COMMAND_CHECKOUT: 
					sCommand="CheckOut";
					break;
				case SCC_COMMAND_CHECKIN:
					sCommand="CheckIn";
					break;
				case SCC_COMMAND_UNCHECKOUT:
					sCommand="UnCheckOut";
					break;
				case SCC_COMMAND_ADD:
					sCommand="Add";
					break;
				case SCC_COMMAND_REMOVE:
					sCommand="Remove";
					break;
				default :
					sCommand="other";
					break;
			}
			
			SendDlgItemMessage(hwnd,IDC_PATH,EM_REPLACESEL,0,(LPARAM)"command : ");
			SendDlgItemMessage(hwnd,IDC_PATH,EM_REPLACESEL,0,(LPARAM)sCommand);
			SendDlgItemMessage(hwnd,IDC_PATH,EM_REPLACESEL,0,(LPARAM)"\r\n");
			for(i=0;i<ctx->nFiles;i++){
				SendDlgItemMessage(hwnd,IDC_PATH,EM_REPLACESEL,0,(LPARAM)ctx->lpFileNames[i]);
				SendDlgItemMessage(hwnd,IDC_PATH,EM_REPLACESEL,0,(LPARAM)"\r\n");
			}

			PostMessage(hwnd,UM_SETFOCUS,IDC_COMBO_MSG,0);
			//restore message history
			SendMessage(hwnd,WM_COMMENTHST,false,0);
			//manage OK button (disable in this case)
			PostMessage(hwnd,WM_MANAGEOK,0,0);
			return 1;
		}
		case UM_SETFOCUS:
			SetFocus( GetDlgItem(hwnd,wParam) );
			break;
	}
	return 0;
}

char * _cachefile(char*prjpath){
	char*c=prjpath;
	int i,k,_svnlen;
	//search for the last slash
	for(i=0;prjpath[i];i++)
		if(prjpath[i]=='\\' || prjpath[i]=='/')
			c=prjpath+i;
	//		
	_svnlen=strlen(_svn);
	k=_svnlen+7;  //length of \.svn\cache
	//move filename to the right
	for(i=strlen(c);i>=0;i--)c[i+k]=c[i];
	
	strncpy(c,"\\",1);
	strncpy(c+1,_svn,_svnlen);
	strncpy(c+1+_svnlen,"\\cache ",6);
	strcat(c,".cch");
	return prjpath;
}


//gets file checkout and version info
//ver must be PBSCC_REVLEN+2+1
//user must be SCC_USER_LEN+2+1
//returns true if info found and ver+user are filled;
bool _getfileinfo(THECONTEXT*ctx,char*file,char ver[PBSCC_REVLEN+2+1],char user[SCC_USER_LEN+2+1]){
	ver[0]=0;
	user[0]=0;
	_subst(ctx,file);
	if(!access(filesubst,0)){
		FILE*fcch=fopen( _cachefile( filesubst ), "rt" );
		if(fcch){
			fgets(ver, PBSCC_REVLEN+2+1,fcch);
			fgets(user,SCC_USER_LEN+2+1,fcch);
			fclose(fcch);
			rtrim(ver);
			rtrim(user);
		}else return false;
	}else{
		//delete cache file if exists
		DeleteFile( _cachefile( filesubst ) );
		return false;
	}
	return true;
}

BOOL _copyfile(THECONTEXT *ctx,const char*src,char*dst){
	log("\tcopy \"%s\" \"%s\".\n",src,dst);
	SetFileAttributes(src,FILE_ATTRIBUTE_NORMAL);
	SetFileAttributes(dst,FILE_ATTRIBUTE_NORMAL);
	BOOL b=CopyFile(src,dst,false);
	if(!b){
		char *buf=new char[strlen(src)+strlen(dst)+100];
		sprintf(buf,"can't copy \"%s\" to \"%s\"",src,dst);
		log("%s.\n",buf);
		_msg(ctx,buf);
		delete []buf;
	}
	return b;
}

BOOL needComment(THECONTEXT*ctx){
	//a little bit stupid check, but...
	//if all files are *.pbg then no comment needed
	for(int i=0;i<ctx->nFiles;i++){
		long len=strlen(ctx->lpFileNames[i]);
		if( len>4 && !stricmp(".pbg",ctx->lpFileNames[i]+len-4) ){
			//this is a last file and all files are pbg
			if(i==ctx->nFiles-1){
				log("needComment: only pbg files. no comment needed.\n");
				return false;
			}
		}else break;
	}

//BAD CHECK. Better to add operation into ctx.
//and check if it's the first checkin operation after add/remove
/*	if (ctx->isLastAddRemove) {
		ctx->isLastAddRemove = FALSE;
		return FALSE; 
	}
*/
	
	return ((GetTickCount() - ctx->dwLastCommitTime) > DELAYFORNEWCOMMENT);
}

bool rememberTarget(THECONTEXT*ctx,LONG nFiles,LPCSTR* lpFileNames){
	for(LONG i=0;i<nFiles;i++){
		long len=strlen(lpFileNames[i]);
		if( len>4 && !stricmp(".pbt",lpFileNames[i]+len-4) && len<MAXFULLPATH ){
			strcpy(ctx->PBTarget,lpFileNames[i]);
			log("target: \"%s\"\n",ctx->PBTarget);
		}
	}
	return true;
}


//file just for information 
bool _getcomment(THECONTEXT*ctx,HWND hWnd,LONG nFiles, LPCSTR* lpFileNames,SCCCOMMAND icmd){
	int dlgID=IDD_DIALOG_COMMENT2;
	ctx->nFiles=nFiles;
	ctx->lpFileNames=lpFileNames;
	ctx->eSCCCommand=icmd;
	
	if( !needComment(ctx) ) return true;
	ctx->lpComment[0]=0;
	if( DialogBoxParam(hInstance,MAKEINTRESOURCE(dlgID),hWnd,&DialogProcComment,(LPARAM)ctx)==0 ){
		log("_getcomment: %s\n",ctx->lpComment);
		if(ctx->lpComment[0])return true;
	}
	return false;
}


extern "C"{
	
void PbSccVersion(){
	FILE*f=fopen("pbscc.ver","wt");
	fprintf(f,"version=%s\n",PROJECT_VER);
	fprintf(f,"date=%s\n",PROJECT_DATE);
	fflush(f);
	fclose(f);
}

	
SCCEXTERNC SCCRTN EXTFUN SccInitialize(LPVOID * ppContext, HWND hWnd, LPCSTR lpCallerName,LPSTR lpSccName, LPLONG lpSccCaps, LPSTR lpAuxPathLabel, LPLONG pnCheckoutCommentLen, LPLONG pnCommentLen){
	log("SccInitialize:\n");
	
	THECONTEXT * ctx=new THECONTEXT;
	memset( ctx, 0, sizeof(THECONTEXT) );
	strcpy(ctx->lpCallerName,lpCallerName);
	strcpy(lpSccName,gpSccName);
	lpSccCaps[0]=0x200828D;
	lpAuxPathLabel[0]=0;
	pnCheckoutCommentLen[0]=PBSCC_CMTLENALL+1;
	pnCommentLen[0]=PBSCC_CMTLENALL+1;
	ppContext[0]=ctx;
	
	
	if(!consoleHwnd){
		//set window topmost flag to prevent console visible
		SetWindowPos(hWnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOREDRAW|SWP_NOSENDCHANGING|SWP_NOSIZE);
		AllocConsole();
		EnumWindows(&TheEnumWindowsProc,0);
		//remove window topmost flag
		SetWindowPos(hWnd,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOREDRAW|SWP_NOSENDCHANGING|SWP_NOSIZE);
	}
	
	HKEY rkey;
	char buf[101];//1 or 0
	DWORD type=REG_SZ;
	DWORD buflen;
	
	if( RegOpenKey(PBSCC_REGKEY,PBSCC_REGPATH,&rkey)==ERROR_SUCCESS){
		
		buflen=sizeof(buf);
		if(RegQueryValueEx(rkey,"cache.ttl.seconds",NULL,&type,(LPBYTE)buf,&buflen)==ERROR_SUCCESS){
			ctx->cacheTtlMs=atol(buf);
			if(ctx->cacheTtlMs<20)	ctx->cacheTtlMs=20;
			//we need it in milliseconds
			ctx->cacheTtlMs=ctx->cacheTtlMs*1000;
		}
		
		buflen=sizeof(buf);
		if(RegQueryValueEx(rkey,"checkout.lock",NULL,&type,(LPBYTE)buf,&buflen)==ERROR_SUCCESS){
			ctx->doLock=(atol(buf)!=0);
		}
		RegCloseKey(rkey);
	}
	
	DWORD len=GetTempPath(MAXFULLPATH,ctx->lpTargetsTmp);
	strcpy(ctx->lpOutTmp,ctx->lpTargetsTmp);
	strcpy(ctx->lpErrTmp,ctx->lpTargetsTmp);
	strcpy(ctx->lpMsgTmp,ctx->lpTargetsTmp);
	
	GetTempFileName(ctx->lpTargetsTmp,"pbscc",0,ctx->lpTargetsTmp );
	GetTempFileName(ctx->lpOutTmp    ,"pbscc",0,ctx->lpOutTmp     );
	GetTempFileName(ctx->lpErrTmp    ,"pbscc",0,ctx->lpErrTmp     );
	GetTempFileName(ctx->lpMsgTmp    ,"pbscc",0,ctx->lpMsgTmp     );
	
	log("\t cache.ttl.milli: %i\n",ctx->cacheTtlMs);
	log("\t checkout.lock  : %i\n",ctx->doLock);
	log("\t svn out file   : %s \n",ctx->lpOutTmp);
	log("\t svn err file   : %s \n",ctx->lpErrTmp);
	log("\t svn msg file   : %s \n",ctx->lpMsgTmp);
	
	ctx->pipeOut=new mstring();
	ctx->pipeErr=new mstring();
	
	ctx->parent=hWnd;
	
	//the last command/ just to test
	_execscc(ctx,"%s","set");
	
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN SccUninitialize(LPVOID pContext){
	log("SccUninitialize:\n");
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	
	DeleteFile(ctx->lpTargetsTmp); //delete temp file
	DeleteFile(ctx->lpOutTmp); //delete temp file
	DeleteFile(ctx->lpErrTmp); //delete temp file
	DeleteFile(ctx->lpMsgTmp); //delete temp file
	
	delete ctx->pipeOut;
	delete ctx->pipeErr;


	delete pContext;
	if(consoleHwnd){
		FreeConsole();
		consoleHwnd=NULL;
	}
	return SCC_OK;
}


SCCEXTERNC SCCRTN EXTFUN SccGetProjPath(LPVOID pContext, HWND hWnd, LPSTR lpUser,LPSTR lpProjName, LPSTR lpLocalPath,LPSTR lpAuxProjPath,BOOL bAllowChangePath,LPBOOL pbNew){
	log("SccGetProjPath:\n");
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	BROWSEINFO bi;
	LPITEMIDLIST id;
	SCCRTN ret=SCC_E_OPNOTSUPPORTED;
	LPMALLOC lpMalloc;
	
	
	memset(&bi,0,sizeof(bi));
	bi.hwndOwner=hWnd;
	bi.pszDisplayName=lpProjName;
	bi.lpszTitle="Select the folder under source control";
	repeat:
	id=SHBrowseForFolder( &bi );
	if(id){
		SHGetPathFromIDList(id, lpProjName);
		if(SHGetMalloc(&lpMalloc)==NOERROR)lpMalloc->Free(id);
		if(!_execscc(ctx,"svn info --non-interactive \"%s\"",lpProjName)){
			char buf[2048];
			_snprintf(buf,sizeof(buf)-1,"The selected folder is not under source control.\n\n"  
				"The command line\n"
				"\tsvn.exe info --non-interactive \"%s\"\n"
				"\treturns no rows.\n\n"
				"Check that svn.exe installed on your computer.\n"
				"The directory where svn.exe is located must be in the PATH environment variable.\n"
				"Start your command shell and check that the command specified above returns one row like: \"Last Changed Rev: 559\".\n"
				,lpProjName);
			strcpy(buf+sizeof(buf)-6,"...");
			MessageBox(hWnd,buf, gpSccName, MB_OK|MB_ICONERROR);
			goto repeat;
		}
		
		DWORD user_len=SCC_USER_LEN;
		GetUserName(lpUser,&user_len);
		lpAuxProjPath[0]=0;
		pbNew[0]=false;
		ret=SCC_OK;
	}
	return ret;
}



SCCEXTERNC SCCRTN EXTFUN SccOpenProject(LPVOID pContext,HWND hWnd, LPSTR lpUser,LPSTR lpProjName,LPCSTR lpLocalProjPath,LPSTR lpAuxProjPath,LPCSTR lpComment,LPTEXTOUTPROC lpTextOutProc,LONG dwFlags){
	log("SccOpenProject:\n");
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	strcpy(ctx->lpProjName,lpProjName);
	strcpy(ctx->lpProjPath,lpLocalProjPath);
	strcpy(ctx->lpUser,lpUser);
	ctx->cbProjName=strlen(ctx->lpProjName);
	ctx->cbProjPath=strlen(ctx->lpProjPath);
	ctx->lpOutProc=lpTextOutProc;
	_msg(ctx,gpSccVersion);
	
	{
		HKEY rkey;
		DWORD type=REG_SZ;
		DWORD buflen=sizeof(_svn)-1;//11-1=10
		if( RegOpenKey(PBSCC_REGKEY,PBSCC_REGPATH,&rkey)==ERROR_SUCCESS){
			if(RegQueryValueEx(rkey,"svn.work",NULL,&type,(LPBYTE)_svn,&buflen)==ERROR_SUCCESS){
				_msg(ctx,_svn);  //just debug
			}
			RegCloseKey(rkey);
		}
	}
	
	if(!_sccupdate(ctx,true))return SCC_E_INITIALIZEFAILED;
	if(!PBGetVersion(ctx->PBVersion))ctx->PBVersion[0]=0;//get pb version
	
	return SCC_OK;
}


SCCEXTERNC SCCRTN EXTFUN SccGetCommandOptions(LPVOID pContext, HWND hWnd, enum SCCCOMMAND nCommand,LPCMDOPTS * ppvOptions){
	log("SccGetCommandOptions:\n");
	if(nCommand==SCC_COMMAND_OPTIONS)return SCC_OK;//SCC_I_ADV_SUPPORT; //no advanced button
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccCloseProject(LPVOID pContext){
	log("SccCloseProject:\n");
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN SccGet(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames, LONG dwFlags,LPCMDOPTS pvOptions){
	log("SccGet:\n");
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	if(!_sccupdate(ctx,true))return SCC_E_ACCESSFAILURE;
	for(int i=0;i<nFiles;i++){
		log("\t%s\n",lpFileNames[i]);
		if( !_copyfile(ctx,_subst(ctx,(char*)lpFileNames[i]),(char*)lpFileNames[i]) )return SCC_E_NONSPECIFICERROR;
	}
	return SCC_OK;
}

//common function for SccQueryInfoEx and SccQueryInfo 
//the difference only in cbFunc and cbParm parameters
SCCRTN _SccQueryInfo(LPVOID pContext, LONG nFiles, LPCSTR* lpFileNames,LPLONG lpStatus,INFOEXCALLBACK cbFunc,LPVOID cbParm){
	INFOEXCALLBACKPARM cbp;
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	ctx->lpComment[0]=0;
	DWORD t=GetTickCount();
	if(cbFunc){
		memset(&cbp,0,sizeof(cbp));
		cbp.cb=sizeof(cbp);
		cbp.status=1;
	}
	rememberTarget(ctx, nFiles, lpFileNames);
	if(!_sccupdate(ctx,false))return SCC_E_ACCESSFAILURE;

	for(int i=0;i<nFiles;i++){
		lpStatus[i]=SCC_STATUS_NOTCONTROLLED;

		char ver[PBSCC_REVLEN+2+1] ; //+2 for CRLF +1 for EOL
		char user[SCC_USER_LEN+2+1];
		
		if( _getfileinfo(ctx,(char*)lpFileNames[i], ver, user) ){
			lpStatus[i]=SCC_STATUS_CONTROLLED;
			if(user[0]){
				if(!stricmp(user,ctx->lpUser))lpStatus[i]=SCC_STATUS_OUTBYUSER|SCC_STATUS_CONTROLLED;
				else lpStatus[i]=SCC_STATUS_OUTOTHER|SCC_STATUS_CONTROLLED;
			}
			if(cbFunc){
				cbp.object=(char*)lpFileNames[i];
				cbp.version=ver;
				cbFunc(cbParm,&cbp);
			}
		}else{
			if(cbFunc){
				cbp.object=(char*)lpFileNames[i];
				cbp.version="";
				cbFunc(cbParm,&cbp);
			}
		}
		log("\tstat=%04X ver=\"%s\" user=\"%s\" \"%s\"\n",lpStatus[i],ver,user,lpFileNames[i]);
	}
	log("_SccQueryInfo: ms=%i\n",GetTickCount()-t);
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN SccQueryInfoEx(LPVOID pContext, LONG nFiles, LPCSTR* lpFileNames,LPLONG lpStatus,INFOEXCALLBACK cbFunc,LPVOID cbParm){
	log("SccQueryInfoEx:\n");
	return _SccQueryInfo(pContext, nFiles, lpFileNames, lpStatus, cbFunc, cbParm);
}

SCCEXTERNC SCCRTN EXTFUN SccQueryInfo(LPVOID pContext, LONG nFiles, LPCSTR* lpFileNames, LPLONG lpStatus){
	log("SccQueryInfo:\n");
	return _SccQueryInfo(pContext, nFiles, lpFileNames, lpStatus, NULL, NULL);
}

SCCEXTERNC SCCRTN EXTFUN SccCheckout(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames, LPCSTR lpComment, LONG dwFlags,LPCMDOPTS pvOptions){
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	int i;
	log("SccCheckout:\n");
	if(!_sccupdate(ctx,true))return SCC_E_ACCESSFAILURE;
	//check if all files are not locked
	for(i=0;i<nFiles;i++){
		char ver[PBSCC_REVLEN+2+1] ; //+2 for CRLF +1 for EOL
		char user[SCC_USER_LEN+2+1];
		
		if( _getfileinfo(ctx,(char*)lpFileNames[i], ver, user) ){
			if(user[0])return SCC_E_ALREADYCHECKEDOUT;
		}else return SCC_E_FILENOTCONTROLLED;
	}
	//do svn operations
	if (!_files2list(ctx, nFiles, lpFileNames ))return SCC_E_ACCESSFAILURE;
	if(!_execscc(ctx,"svn propset --non-interactive lockby \"%s\" --targets \"%s\"",ctx->lpUser,ctx->lpTargetsTmp)  )return SCC_E_NONSPECIFICERROR;
	if(!_scccommit(ctx,SCC_COMMAND_CHECKOUT )){
		_execscc(ctx,"svn revert --targets \"%s\"",ctx->lpTargetsTmp);
		return SCC_E_ACCESSFAILURE;
	}else if(ctx->doLock){
		_execscc(ctx,"svn lock --non-interactive --targets \"%s\" -m CheckOut",ctx->lpTargetsTmp);
	}
	//finish operations
	for( i=0;i<nFiles;i++){
		_copyfile(ctx,_subst(ctx,(char*)lpFileNames[i]),(char*)lpFileNames[i]);
		//add link into todo list
		ToDoSetLine("n", ctx->PBTarget,  (char*)lpFileNames[i], ctx->PBVersion);
	}
	if(!_sccupdate(ctx,true))return SCC_E_ACCESSFAILURE;
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN SccUncheckout(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames, LONG dwFlags,LPCMDOPTS pvOptions){
	char ver[PBSCC_REVLEN+2+1] ; //+2 for CRLF +1 for EOL
	char user[SCC_USER_LEN+2+1];
	int i;
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	log("SccUncheckout:\n");
	if(!_sccupdate(ctx,true))return SCC_E_ACCESSFAILURE;
	if (!_files2list(ctx, nFiles, lpFileNames ))return SCC_E_ACCESSFAILURE;
	for(i=0;i<nFiles;i++){
		if( _getfileinfo(ctx,(char*)lpFileNames[i], ver, user) ){
			if( strcmp(user,ctx->lpUser) ) return SCC_E_NOTCHECKEDOUT;
		}else return SCC_E_FILENOTCONTROLLED;
		if(!_execscc(ctx,"svn propdel --non-interactive lockby \"%s\"",_subst(ctx,(char*)lpFileNames[i]))  )return SCC_E_NONSPECIFICERROR;
	}
	if(!_scccommit(ctx,SCC_COMMAND_UNCHECKOUT )){
		_execscc(ctx,"svn revert --targets \"%s\"",ctx->lpTargetsTmp);
		return SCC_E_ACCESSFAILURE;
	}
	for(i=0;i<nFiles;i++){
		_copyfile(ctx,_subst(ctx,(char*)lpFileNames[i]),(char*)lpFileNames[i]);
		//cross the link in the todo list
		ToDoSetLine("y", ctx->PBTarget,  (char*)lpFileNames[i], ctx->PBVersion);
	}
	if(!_sccupdate(ctx,true))return SCC_E_ACCESSFAILURE;
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN SccCheckin(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames, LPCSTR lpComment, LONG dwFlags,LPCMDOPTS pvOptions){
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	char ver[PBSCC_REVLEN+2+1] ; //+2 for CRLF +1 for EOL
	char user[SCC_USER_LEN+2+1];
	int i;
	log("SccCheckin: \n");
	
	if( !_getcomment( ctx, hWnd,nFiles,lpFileNames,SCC_COMMAND_CHECKIN) )return SCC_I_OPERATIONCANCELED;
	
	if(!_sccupdate(ctx,true))return SCC_E_ACCESSFAILURE;
	if (!_files2list(ctx, nFiles, lpFileNames ))return SCC_E_ACCESSFAILURE;
	
	for(i=0;i<nFiles;i++){
		if( _getfileinfo(ctx,(char*)lpFileNames[i], ver, user) ){
			if( strcmp(user,ctx->lpUser) ) return SCC_E_NOTCHECKEDOUT;
		}else return SCC_E_FILENOTCONTROLLED;
		if( !_copyfile(ctx,lpFileNames[i],_subst(ctx, (char*)lpFileNames[i])) )goto error;
		if(!_execscc(ctx,"svn propdel --non-interactive lockby \"%s\"",_subst(ctx,(char*)lpFileNames[i]))  )return SCC_E_NONSPECIFICERROR;
	}
	if(!_scccommit(ctx,SCC_COMMAND_CHECKIN ))goto error;
	
	for(i=0;i<nFiles;i++){
		//cross the link in the todo list
		ToDoSetLine("y", ctx->PBTarget,  (char*)lpFileNames[i], ctx->PBVersion);
	}
	
	if(!_sccupdate(ctx,true))return SCC_E_ACCESSFAILURE;
	return SCC_OK;
	error:
	_execscc(ctx,"svn revert --targets \"%s\"",ctx->lpTargetsTmp);
	return SCC_E_ACCESSFAILURE;
	
}

SCCEXTERNC SCCRTN EXTFUN SccAdd(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames, LPCSTR lpComment, LONG * pdwFlags,LPCMDOPTS pvOptions){
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	int i;
	log("SccAdd:\n");
	
	if( !_getcomment( ctx, hWnd,nFiles,lpFileNames,SCC_COMMAND_ADD) )return SCC_I_OPERATIONCANCELED;
	
	if(!_sccupdate(ctx,false))return SCC_E_ACCESSFAILURE;
	if (!_files2list(ctx, nFiles, lpFileNames ))return SCC_E_ACCESSFAILURE;
	
	for(i=0;i<nFiles;i++){
		if( !_copyfile(ctx,lpFileNames[i],_subst(ctx, (char*)lpFileNames[i])) )return SCC_E_FILENOTEXIST;
	}
	if(!_execscc(ctx,"svn add --targets \"%s\"",ctx->lpTargetsTmp)  )goto error;
	if(!_scccommit(ctx,SCC_COMMAND_ADD ))goto error;
	
	if(!_sccupdate(ctx,true))return SCC_E_ACCESSFAILURE;
	return SCC_OK;
	error:
	_execscc(ctx,"svn revert --targets \"%s\"",ctx->lpTargetsTmp);
	for(i=0;i<nFiles;i++){
		DeleteFile(_subst(ctx, (char*)lpFileNames[i]));
	}
	return SCC_E_ACCESSFAILURE;
	
}

SCCEXTERNC SCCRTN EXTFUN SccRemove(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames,LPCSTR lpComment,LONG dwFlags,LPCMDOPTS pvOptions){
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	log("SccRemove:\n");
	
	if( !_getcomment( ctx, hWnd,nFiles,lpFileNames,SCC_COMMAND_REMOVE) )return SCC_I_OPERATIONCANCELED;
	
	if(!_sccupdate(ctx,false))return SCC_E_ACCESSFAILURE;
	if (!_files2list(ctx, nFiles, lpFileNames ))return SCC_E_ACCESSFAILURE;

	for(int i=0;i<nFiles;i++){
		SetFileAttributes(_subst(ctx, (char*)lpFileNames[i]),FILE_ATTRIBUTE_NORMAL);
	}
	
	if(!_execscc(ctx,"svn del --non-interactive --targets \"%s\"",ctx->lpTargetsTmp)  )goto error;
	if(!_scccommit(ctx,SCC_COMMAND_REMOVE ))goto error;
	
	if(!_sccupdate(ctx,true))return SCC_E_ACCESSFAILURE;
	return SCC_OK;
	error:
	_execscc(ctx,"svn revert --targets \"%s\"",ctx->lpTargetsTmp);
	return SCC_E_ACCESSFAILURE;
}

SCCEXTERNC SCCRTN EXTFUN SccDiff(LPVOID pContext, HWND hWnd, LPCSTR lpFileName, LONG dwFlags,LPCMDOPTS pvOptions){
	log("SccDiff: %s, %X :\n",lpFileName,dwFlags);
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	if(!_sccupdate(ctx,false))return SCC_E_ACCESSFAILURE;
	if( access( _subst(ctx, (char*)lpFileName), 0 /*R_OK*/ ) )return SCC_E_FILENOTCONTROLLED;
	
	if(dwFlags&SCC_DIFF_QUICK_DIFF){
		//do quick diff
		if( filecmp( (char*)lpFileName, _subst(ctx, (char*)lpFileName))  ){
			return SCC_OK;
		}
		return SCC_I_FILEDIFFERS;
	}else{
		mstring buf;
		char ver[PBSCC_REVLEN+2+1] ; //+2 for CRLF +1 for EOL
		char user[SCC_USER_LEN+2+1];
		
		if( _getfileinfo(ctx,(char*)lpFileName, ver, user) ){
			if(!stricmp(user,ctx->lpUser)){
				_copyfile(ctx,lpFileName,_subst(ctx, (char*)lpFileName));
				buf.sprintf("TortoiseProc.exe /command:diff /path:\"%s\"", filesubst);
				WinExec(buf.c_str(), SW_SHOW);
				_execscc(ctx,"svn revert \"%s\"",filesubst);
				return SCC_OK;
			}
		}
		MessageBox(hWnd,"No differences encountered.",gpSccName,MB_ICONINFORMATION);
	}
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN SccHistory(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames, LONG dwFlags,LPCMDOPTS pvOptions){
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	mstring buf;

	if (nFiles != 1) return SCC_E_NONSPECIFICERROR;
	buf.sprintf("TortoiseProc.exe /command:log /path:\"%s\"", _subst(ctx, (char*)lpFileNames[0]));
	WinExec(buf.c_str(), SW_SHOW);
	
	return SCC_OK;
}


SCCEXTERNC LONG EXTFUN SccGetVersion(){
	log("SccGetVersion %i\n",SCC_VER_NUM);
	return SCC_VER_NUM;
}






//---------------------------------------------------------------------------------------------


SCCEXTERNC SCCRTN EXTFUN SccRename(LPVOID pContext, HWND hWnd, LPCSTR lpFileName,LPCSTR lpNewName){
	log("SccRename: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccProperties(LPVOID pContext, HWND hWnd, LPCSTR lpFileName){
	log("SccProperties: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccPopulateList(LPVOID pContext, enum SCCCOMMAND nCommand, LONG nFiles, LPCSTR* lpFileNames, POPLISTFUNC pfnPopulate, LPVOID pvCallerData,LPLONG lpStatus, LONG dwFlags){
	log("SccPopulateList: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccGetEvents(LPVOID pContext, LPSTR lpFileName,LPLONG lpStatus,LPLONG pnEventsRemaining){
	log("SccGetEvents: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccRunScc(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames){
	log("SccRunScc: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccAddFromScc(LPVOID pContext, HWND hWnd, LPLONG pnFiles,LPCSTR** lplpFileNames){
	log("SccAddFromScc: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccSetOption(LPVOID pContext,LONG nOption,LONG dwVal){
	log("SccSetOption: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

           
} //extern "C"
