

#define ES_SIMPLE_LEN 25
#define ES_MAX_LEN 3000


typedef struct {
	char name[MAX_PATH+1];
	char kind[ES_SIMPLE_LEN+1]; //file,dir
	char url[ES_MAX_LEN+1];// (usually valid only for owner)
	char repository[ES_MAX_LEN+1]; // (usually valid only for owner)
	char scedule[ES_SIMPLE_LEN+1]; // (delete,add,replace)
	long revision; //(usually valid for files)
}SVNENTRY;


//callback for the function entries_scan
//should return false to stop scan or true to continue
//udata - userdata passed to entries_scan function
typedef bool  (*ENTRYSCANCALLBACK)(SVNENTRY*e,void*udata);


//scans the entries file and calls callback function for each entry
bool entries_scan(char*entries,ENTRYSCANCALLBACK callback,void*udata);
//returns repository from the entries file
bool entries_repository(char*entries,char*buf,int buflen);
