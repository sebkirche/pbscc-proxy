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

/**
 * this class defines simple class to hold svn object list
 */

#ifndef __SVNINFO_H__
#define __SVNINFO_H__
//initial size
#define __SVNINFO_H__INITSIZE	5000

typedef struct {
	int        hash;      //simple hash
	char       *path;     //relative path to file
	char       *rev;      //revision 
	char       *owner;    //object owner (locker)
}SVNINFOITEM;


class svninfo {
	private:
		SVNINFOITEM* ptr;
		long size;         
		long count;
		
		char*scpy(char*c){
			if(c==NULL)return NULL;
			char*cpy=new char[strlen(c)+1];
			return strcpy(cpy,c);
		}
		int hash(char*c){
			int h=0;
			char*ch="_";
			for (int i = 0; c[i]; i++) {
				ch[0]=c[i];
				CharLowerBuff(ch,1);
				h = 31*h + ch[0];
			}
			return h;
		}
	public:
		
		svninfo(){
			size  = __SVNINFO_H__INITSIZE;
			count = 0;
			ptr   = new SVNINFOITEM[size];
			memset( ptr, 0, size*sizeof(SVNINFOITEM) );
		}
		
		~svninfo(){
			reset();
			delete []ptr;
			ptr=NULL;
			count=0;
			size=0;
		}
		/**
		 * adds to the end of the list an item
		 * will not check if item already exists
		 */
		void add(char*_path,char*_rev,char*_owner){
			if(count+1>=size){
				//reallocate
				SVNINFOITEM *ptr_old=ptr;
				long size_old=size;
				size=size+size/2;
				ptr=new SVNINFOITEM[size];
				memset( ptr, 0, size*sizeof(SVNINFOITEM) );
				memcpy(ptr, ptr_old, size_old*sizeof(SVNINFOITEM));
				delete []ptr_old;
			}
			ptr[count].path=scpy(_path);
			ptr[count].rev=scpy(_rev);
			ptr[count].owner=scpy(_owner);
			ptr[count].hash=hash(ptr[count].path);
			count++;
		}
		
		/** reset all the values */
		void reset() {
			for(int i=0;i<count;i++){
				if(ptr[i].path)  delete []ptr[i].path;
				ptr[i].path=NULL;
				if(ptr[i].rev)  delete []ptr[i].rev;
				ptr[i].rev=NULL;
				if(ptr[i].owner) delete []ptr[i].owner;
				ptr[i].owner=NULL;
			}
			count=0;
		}
		
		/** returns element count */
		int getCount(){
			return count;
		}
		
		/** returns element by index */
		SVNINFOITEM* get(int i){
			return &ptr[i];
		}
		
		/** returns svn element by path element */
		SVNINFOITEM* get(char*_path){
			int h=hash(_path);
			for(int i=0;i<count;i++) {
				if( ptr[i].hash==h ){
					if ( !lstrcmpi( _path, ptr[i].path ) )return &ptr[i];
				}
			}
			return NULL;
		}
		
	
};	


#endif
