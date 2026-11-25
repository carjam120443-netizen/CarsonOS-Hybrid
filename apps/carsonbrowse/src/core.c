#include "carsonbrowse.h"
#include <string.h>
#include <ctype.h>
static int cp(char *d,size_t cap,const char *s,size_t n){if(!d||!s||n>=cap)return 0;memcpy(d,s,n);d[n]=0;return 1;}
int cb_url_parse(const char *s,cb_url *o){
 const char *p,*e,*colon;size_t n;unsigned port=80;
 if(!s||!o||strncmp(s,"http://",7))return 0;memset(o,0,sizeof(*o));p=s+7;e=p;
 while(*e&&*e!='/'&&*e!='?'&&*e!='#')e++;
 if(e==p||memchr(p,'@',(size_t)(e-p))||memchr(p,'[',(size_t)(e-p)))return 0;
 colon=memchr(p,':',(size_t)(e-p));n=colon?(size_t)(colon-p):(size_t)(e-p);
 if(!cp(o->host,sizeof(o->host),p,n))return 0;
 if(colon){const char*q=colon+1;if(q==e)return 0;port=0;while(q<e){if(!isdigit((unsigned char)*q))return 0;port=port*10u+(unsigned)(*q++-'0');if(port>65535)return 0;}if(!port)return 0;}
 if(*e=='?'){n=strlen(e);if(n+1>=sizeof(o->path))return 0;o->path[0]='/';memcpy(o->path+1,e,n+1);}
 else if(*e=='#'||!*e)strcpy(o->path,"/");
 else {n=strlen(e);if(n>=sizeof(o->path))return 0;memcpy(o->path,e,n+1);}
 strcpy(o->scheme,"http");o->port=port;return 1;
}
int cb_http_parse(const char*b,size_t n,cb_http_response*o){
 const char*e,*body=NULL;size_t pos;
 if(!b||!o||n<12||(e=memchr(b,'\n',n))==NULL||(size_t)(e-b)<12||memcmp(b,"HTTP/1.",7))return 0;
 if(!isdigit((unsigned char)b[9])||!isdigit((unsigned char)b[10])||!isdigit((unsigned char)b[11]))return 0;
 pos=(size_t)(e-b)+1;
 while(pos<n){size_t st=pos;while(pos<n&&b[pos]!='\n')pos++;
  if(pos==st||(pos==st+1&&b[st]=='\r')){body=b+(pos<n?pos+1:pos);break;}
  if(pos<n)pos++;
 }
 if(!body)return 0;o->status=(unsigned)(b[9]-'0')*100u+(unsigned)(b[10]-'0')*10u+(unsigned)(b[11]-'0');o->body=body;o->body_length=n-(size_t)(body-b);return 1;
}
static void put(cb_document*d,char c){if(d->text_length+1<sizeof(d->text)){d->text[d->text_length++]=c;d->text[d->text_length]=0;}}
static void decode(cb_document*d,const char*p,size_t n){
 if(n==3&&!memcmp(p,"amp",3))put(d,'&');else if(n==2&&!memcmp(p,"lt",2))put(d,'<');else if(n==2&&!memcmp(p,"gt",2))put(d,'>');else if(n==4&&!memcmp(p,"quot",4))put(d,'"');else if(n==4&&!memcmp(p,"apos",4))put(d,'\'');else if(n==4&&!memcmp(p,"nbsp",4))put(d,' ');else put(d,'&');
}
int cb_html_to_text(const char*h,size_t n,cb_document*d){
 size_t i=0;int hidden=0,title=0,space=0;
 if(!h||!d)return 0;memset(d,0,sizeof(*d));
 while(i<n){char c=h[i++];
  if(c=='<'){int closing=0;char tag[32];size_t t=0,j=i;while(j<n&&h[j]!='>'&&j-i<64)j++;
   while(i<j&&isspace((unsigned char)h[i]))i++;if(i<j&&h[i]=='/'){closing=1;i++;}
   while(i<j&&(isalnum((unsigned char)h[i])||h[i]=='-')&&t+1<sizeof(tag))tag[t++]=(char)tolower((unsigned char)h[i++]);tag[t]=0;
   if(!strcmp(tag,"script")||!strcmp(tag,"style"))hidden=!closing;
   if(!strcmp(tag,"title"))title=!closing;
   if(!hidden&&(!strcmp(tag,"p")||!strcmp(tag,"br")||!strcmp(tag,"div")||!strcmp(tag,"h1")||!strcmp(tag,"h2")||!strcmp(tag,"li")||!strcmp(tag,"tr")))space=1;
   i=j<n?j+1:j;continue;
  }
  if(hidden)continue;
  if(c=='&'){size_t st=i,j=i;while(j<n&&j-st<8&&isalnum((unsigned char)h[j]))j++;if(j<n&&h[j]==';'){decode(d,h+st,j-st);i=j+1;continue;}}
  if(isspace((unsigned char)c)){space=1;continue;}
  if(space&&(title||d->text_length))put(d,' ');space=0;
  if(title){size_t z=strlen(d->title);if(z+1<sizeof(d->title)){d->title[z]=c;d->title[z+1]=0;}}else put(d,c);
 }
 return 1;
}
int cb_history_visit(cb_history*h,const cb_url*u){size_t i;if(!h||!u)return 0;if(h->count&&h->cursor+1<h->count)h->count=h->cursor+1;if(h->count==CB_HISTORY_MAX){for(i=1;i<CB_HISTORY_MAX;i++)h->entries[i-1]=h->entries[i];h->count--;}h->entries[h->count]=*u;h->cursor=h->count++;return 1;}
int cb_history_back(cb_history*h,cb_url*o){if(!h||!o||!h->count||!h->cursor)return 0;*o=h->entries[--h->cursor];return 1;}
int cb_history_forward(cb_history*h,cb_url*o){if(!h||!o||!h->count||h->cursor+1>=h->count)return 0;*o=h->entries[++h->cursor];return 1;}
int cb_load(const char*a,cb_fetch_fn f,void*x,cb_document*d,unsigned*status){
 cb_url u;char response[CB_RESPONSE_MAX];size_t got=0;cb_http_response r;
 if(!f||!d||!cb_url_parse(a,&u)||!f(&u,response,sizeof(response),&got,x)||got>sizeof(response)||!cb_http_parse(response,got,&r))return 0;
 if(status)*status=r.status;return cb_html_to_text(r.body,r.body_length,d);
}
