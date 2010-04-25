#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

bool filecmp(char*name1,char*name2) {
	FILE *fp1, *fp2;
	int ch1=1;
	int ch2=2;

	/* open first file */
	if((fp1 = fopen(name1, "rb"))!=NULL) {
		/* open second file */
		if((fp2 = fopen(name2, "rb"))!=NULL) {
			/* compare the files */
			do{
				ch1=fgetc(fp1);
				ch2=fgetc(fp2);
			}while( ch1==ch2 && ch1!=EOF);
			
			fclose(fp2);
		}
		fclose(fp1);
	}
	return (ch1==ch2);
}
