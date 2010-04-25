#include <windows.h>

//simple string functions

//removes right spaces
char*rtrim(char*c){
	char*ns=c;
	char*space=" \t\r\n";
	for(int i=0;c[i];i++)
		if(!strchr(space,c[i]))
			ns=c+i+1;
	ns[0]=0;
	return c;
}

//returns pointer to the first non-space character of the string (char*)
char * ltrim(char*c){
	while(c[0] && (c[0]==' ' || c[0]=='\t'))c++;
	return c;
}


