#ifndef __MSTRING_H__
#define __MSTRING_H__
#define __MSTRING_H__ALLOC 2048

#include <tchar.h>
#include "deelx.h"

#define ASSERT(x) {if(!(x)) _asm{int 0x03}}
#define TCHAR_ARG   TCHAR
#define WCHAR_ARG   WCHAR
#define CHAR_ARG    char
#define DOUBLE_ARG  double

#define FORCE_ANSI      0x10000
#define FORCE_UNICODE   0x20000
#define FORCE_INT64     0x40000

class mstring {
	private:
		//--- member variables

		TCHAR* ptr;
		long allocated;
		long length;
		
		void init(){
			allocated=0;
			length=0;
			ptr = NULL;
			//printf("init\n");
		}
		//! Reallocate the array, make it bigger or smaler
		void realloc(long new_size) {
			//printf("realloc > %i\n",new_size);
			if(new_size>allocated){
				TCHAR* old_ptr = ptr;
				new_size/=__MSTRING_H__ALLOC;
				new_size++;
				new_size*=__MSTRING_H__ALLOC;
				ptr=new TCHAR[new_size];
				if(old_ptr)	{
					_tcscpy(ptr,old_ptr);
					delete [] old_ptr;
				}else{
					ptr[0]=0;
				}
				allocated=new_size;
				//printf("realloc >> %i\n",new_size);
			}
		}
	public:
		//! Default constructor
		mstring(){
			init();
		}

		//! Constructor
		mstring(TCHAR*c){
			init();
			append(c);
		}


		//! destructor
		~mstring() {
			if(ptr)delete [] ptr;
			//printf("destroy\n");
		}
		
		TCHAR * c_str(){
			return ptr;
		}
		
		//flag could be NORM_IGNORECASE
		bool startsWith(TCHAR *c, int flag=0){
			int ret;
			if(flag & NORM_IGNORECASE)
				ret=_tcsnicmp(ptr, c, _tcslen(c));
			else
				ret=_tcsnccmp(ptr, c, _tcslen(c));
				
			if(!ret)return true;
			return false;
		}
		
		TCHAR charAt(long i){
			if(i>length || i<0 || length==0)return 0;
			return ptr[i];
		}

		void set(TCHAR * c){
			if(c){
				long i=_tcslen(c);
				realloc(i+1);
				_tcscpy(ptr,c);
				length=i;
				//printf("set > %s<<\n",ptr);
			}else{
				if(ptr){
					delete [] ptr;
					ptr=NULL;
				}
				realloc(0);
				length=0;
				allocated=0;
				//printf("set > %s<<\n",ptr);
			}
		}
		
		bool getenv(TCHAR * name){
			int len=GetEnvironmentVariable(name,NULL,0);
			if(len>0){
				realloc(len+1);
				GetEnvironmentVariable(name,ptr,len+1);
				length=_tcslen(ptr);
			}else{
				set(NULL);
				return false;
			}
			return true;
		}
		
		void append(TCHAR * c){
			if(!c)return;
			long i=_tcslen(c);
			realloc(length+i+1);
			_tcscpy(ptr+length,c);
			length+=i;
			//printf("append > %s<<\n",ptr);
		}
		
		long len(){
			return length;
		}

		void rtrim(){
			if(length==0)return;
			int len=length;
			TCHAR*space=_T(" \t\r\n");
			for(int i=length-1; i>=0 && _tcschr(space,ptr[i]) ; i--)
				ptr[i]=0;
			length=_tcslen(ptr);
		}

		
		void vsprintf(LPCTSTR lpszFormat, va_list argList){

			va_list argListSave = argList;

			// make a guess at the maximum length of the resulting string
			int nMaxLen = 0;
			for (LPCTSTR lpsz = lpszFormat; *lpsz != '\0'; lpsz = _tcsinc(lpsz))
			{
				// handle '%' character, but watch out for '%%'
				if (*lpsz != '%' || *(lpsz = _tcsinc(lpsz)) == '%')
				{
					nMaxLen += _tclen(lpsz);
					continue;
				}

				int nItemLen = 0;

				// handle '%' character with format
				int nWidth = 0;
				for (; *lpsz != '\0'; lpsz = _tcsinc(lpsz))
				{
					// check for valid flags
					if (*lpsz == '#')
						nMaxLen += 2;   // for '0x'
					else if (*lpsz == '*')
						nWidth = va_arg(argList, int);
					else if (*lpsz == '-' || *lpsz == '+' || *lpsz == '0' ||
						*lpsz == ' ')
						;
					else // hit non-flag character
						break;
				}
				// get width and skip it
				if (nWidth == 0)
				{
					// width indicated by
					nWidth = _ttoi(lpsz);
					for (; *lpsz != '\0' && _istdigit(*lpsz); lpsz = _tcsinc(lpsz))
						;
				}
				ASSERT(nWidth >= 0);

				int nPrecision = 0;
				if (*lpsz == '.')
				{
					// skip past '.' separator (width.precision)
					lpsz = _tcsinc(lpsz);

					// get precision and skip it
					if (*lpsz == '*')
					{
						nPrecision = va_arg(argList, int);
						lpsz = _tcsinc(lpsz);
					}
					else
					{
						nPrecision = _ttoi(lpsz);
						for (; *lpsz != '\0' && _istdigit(*lpsz); lpsz = _tcsinc(lpsz))
							;
					}
					ASSERT(nPrecision >= 0);
				}

				// should be on type modifier or specifier
				int nModifier = 0;
				if (_tcsncmp(lpsz, _T("I64"), 3) == 0)
				{
					lpsz += 3;
					nModifier = FORCE_INT64;
#if !defined(_X86_) && !defined(_ALPHA_)
					// __int64 is only available on X86 and ALPHA platforms
					ASSERT(FALSE);
#endif
				}
				else
				{
					switch (*lpsz)
					{
					// modifiers that affect size
					case 'h':
						nModifier = FORCE_ANSI;
						lpsz = _tcsinc(lpsz);
						break;
					case 'l':
						nModifier = FORCE_UNICODE;
						lpsz = _tcsinc(lpsz);
						break;

					// modifiers that do not affect size
					case 'F':
					case 'N':
					case 'L':
						lpsz = _tcsinc(lpsz);
						break;
					}
				}

				// now should be on specifier
				switch (*lpsz | nModifier)
				{
				// single characters
				case 'c':
				case 'C':
					nItemLen = 2;
					va_arg(argList, TCHAR_ARG);
					break;
				case 'c'|FORCE_ANSI:
				case 'C'|FORCE_ANSI:
					nItemLen = 2;
					va_arg(argList, CHAR_ARG);
					break;
				case 'c'|FORCE_UNICODE:
				case 'C'|FORCE_UNICODE:
					nItemLen = 2;
					va_arg(argList, WCHAR_ARG);
					break;

				// strings
				case 's':
					{
						LPCTSTR pstrNextArg = va_arg(argList, LPCTSTR);
						if (pstrNextArg == NULL)
						   nItemLen = 6;  // "(null)"
						else
						{
						   nItemLen = lstrlen(pstrNextArg);
						   nItemLen = max(1, nItemLen);
						}
					}
					break;

				case 'S':
					{
#ifndef _UNICODE
						LPWSTR pstrNextArg = va_arg(argList, LPWSTR);
						if (pstrNextArg == NULL)
						   nItemLen = 6;  // "(null)"
						else
						{
						   nItemLen = wcslen(pstrNextArg);
						   nItemLen = max(1, nItemLen);
						}
#else
						LPCSTR pstrNextArg = va_arg(argList, LPCSTR);
						if (pstrNextArg == NULL)
						   nItemLen = 6; // "(null)"
						else
						{
						   nItemLen = lstrlenA(pstrNextArg);
						   nItemLen = max(1, nItemLen);
						}
#endif
					}
					break;

				case 's'|FORCE_ANSI:
				case 'S'|FORCE_ANSI:
					{
						LPCSTR pstrNextArg = va_arg(argList, LPCSTR);
						if (pstrNextArg == NULL)
						   nItemLen = 6; // "(null)"
						else
						{
						   nItemLen = lstrlenA(pstrNextArg);
						   nItemLen = max(1, nItemLen);
						}
					}
					break;

				case 's'|FORCE_UNICODE:
				case 'S'|FORCE_UNICODE:
					{
						LPWSTR pstrNextArg = va_arg(argList, LPWSTR);
						if (pstrNextArg == NULL)
						   nItemLen = 6; // "(null)"
						else
						{
						   nItemLen = wcslen(pstrNextArg);
						   nItemLen = max(1, nItemLen);
						}
					}
					break;
				}

				// adjust nItemLen for strings
				if (nItemLen != 0)
				{
					if (nPrecision != 0)
						nItemLen = min(nItemLen, nPrecision);
					nItemLen = max(nItemLen, nWidth);
				}
				else
				{
					switch (*lpsz)
					{
					// integers
					case 'd':
					case 'i':
					case 'u':
					case 'x':
					case 'X':
					case 'o':
						if (nModifier & FORCE_INT64)
							va_arg(argList, __int64);
						else
							va_arg(argList, int);
						nItemLen = 32;
						nItemLen = max(nItemLen, nWidth+nPrecision);
						break;

					case 'e':
					case 'g':
					case 'G':
						va_arg(argList, DOUBLE_ARG);
						nItemLen = 128;
						nItemLen = max(nItemLen, nWidth+nPrecision);
						break;

					case 'f':
						{
							// 312 == strlen("-1+(309 zeroes).")
							// 309 zeroes == max precision of a double
							// 6 == adjustment in case precision is not specified,
							//   which means that the precision defaults to 6
							va_arg(argList, double);
							nItemLen = max(nWidth, 312)+nPrecision+6;
						}
						break;

					case 'p':
						va_arg(argList, void*);
						nItemLen = 32;
						nItemLen = max(nItemLen, nWidth+nPrecision);
						break;

					// no output
					case 'n':
						va_arg(argList, int*);
						break;

					default:
						ASSERT(FALSE);  // unknown formatting option
					}
				}

				// adjust nMaxLen for output nItemLen
				nMaxLen += nItemLen;
			}

			realloc(length + nMaxLen +1 );
			::_vstprintf(ptr+length, lpszFormat, argListSave);

			va_end(argListSave);
		}
		
		
		void sprintf(TCHAR* format, ...){
			va_list argList;
			va_start(argList, format);
			vsprintf(format, argList);
			va_end(argList);
		}

		/* 
			enum REGEX_FLAGS {
				NO_FLAG        = 0,
				SINGLELINE     = 0x01,
				MULTILINE      = 0x02,
				GLOBAL         = 0x04,
				IGNORECASE     = 0x08,
				RIGHTTOLEFT    = 0x10,
				EXTENDED       = 0x20
			};
		*/
		bool match(TCHAR*pattern,REGEX_FLAGS flags=NO_FLAG){
		    // declare
            CRegexp regexp(pattern,flags);
            // test
            MatchResult result = regexp.Match(ptr);
            // matched or not
            return result.IsMatched()!=0;
		}

};



#endif

