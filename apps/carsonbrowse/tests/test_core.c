#include "carsonbrowse.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int fake(const cb_url*u,char*b,size_t cap,size_t*n,void*x){
 const char*r="HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<title>Test Page</title><h1>Hello</h1><script>bad()</script><p>A &amp; B</p>";(void)x;
 assert(!strcmp(u->host,"example.com")&&!strcmp(u->path,"/"));if(strlen(r)>cap)return 0;memcpy(b,r,strlen(r));*n=strlen(r);return 1;
}
int main(void){
 cb_url u,v;cb_http_response r;cb_document d;cb_history h={0};unsigned status=0;
 const char*raw="HTTP/1.1 200 OK\r\nX-Test: yes\r\n\r\nhello";
 const char*html="<title>Demo</title><h1>Hello</h1><script>x</script><p>A &amp; B</p>";
 assert(cb_url_parse("http://example.com",&u)&&u.port==80&&!strcmp(u.path,"/"));
 assert(cb_url_parse("http://example.com:8080/path?q=1",&u)&&u.port==8080&&!strcmp(u.path,"/path?q=1"));
 assert(!cb_url_parse("https://example.com",&u));assert(!cb_url_parse("http://user@example.com",&u));
 assert(cb_http_parse(raw,strlen(raw),&r)&&r.status==200&&r.body_length==5&&!memcmp(r.body,"hello",5));
 assert(cb_html_to_text(html,strlen(html),&d)&&!strcmp(d.title,"Demo"));
 assert(strstr(d.text,"Hello")&&strstr(d.text,"A & B")&&!strstr(d.text,"x"));
 assert(cb_url_parse("http://one.test/",&u)&&cb_url_parse("http://two.test/",&v));
 assert(cb_history_visit(&h,&u)&&cb_history_visit(&h,&v));
 assert(cb_history_back(&h,&u)&&!strcmp(u.host,"one.test"));
 assert(cb_history_forward(&h,&u)&&!strcmp(u.host,"two.test"));
 assert(cb_load("http://example.com",fake,NULL,&d,&status)&&status==200&&!strcmp(d.title,"Test Page"));
 puts("CarsonBrowse core tests passed");return 0;
}
