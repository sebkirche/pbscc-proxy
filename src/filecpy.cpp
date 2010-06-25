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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

BOOL CopyFileUTF8(const char*src,char*dst){
	BOOL b=false;
	return b;
}

TCHAR*DELIMHEXASCII	=_T("$$");
TCHAR*OPENHEXASCII	=_T("$$HEX");
TCHAR*CLOSEHEXASCII	=_T("$$ENDHEX$$");

BOOL HADecode(TCHAR * buf){
	TCHAR *p=buf;
	TCHAR *p2;
	int ch,b8;
	int b8shift[]={4,0,12,8};
	int count,i;
	while ((p = _tcsstr(p, OPENHEXASCII)) != NULL) {
		p2 = _tcsstr(p+_tcslen(OPENHEXASCII), DELIMHEXASCII);
		if(!p2)return false;
		p2[0]=0;
		count=_ttol(p+_tcslen(OPENHEXASCII));
		if(count==0)return false;
		p2[0]=DELIMHEXASCII[0];
		p2+=_tcslen(DELIMHEXASCII);
		while(count){
			ch=0;
			for(i=0;i<4;i++){
				switch(p2[0]){
					case '0':b8=0;break;
					case '1':b8=1;break;
					case '2':b8=2;break;
					case '3':b8=3;break;
					case '4':b8=4;break;
					case '5':b8=5;break;
					case '6':b8=6;break;
					case '7':b8=7;break;
					case '8':b8=8;break;
					case '9':b8=9;break;
					case 'a':
					case 'A':b8=10;break;
					case 'b':
					case 'B':b8=11;break;
					case 'c':
					case 'C':b8=12;break;
					case 'd':
					case 'D':b8=13;break;
					case 'e':
					case 'E':b8=14;break;
					case 'f':
					case 'F':b8=15;break;
					default: return false;
				}
				ch=ch | (b8<<b8shift[i]);
				p2++;
			}
			p[0]=ch;
			p++;
			count--;
		}
		if(_tcsncmp(CLOSEHEXASCII,p2,_tcslen(CLOSEHEXASCII)))return false;
		_tcscpy(p,p2+_tcslen(CLOSEHEXASCII));
	}
	
	return true;
}
