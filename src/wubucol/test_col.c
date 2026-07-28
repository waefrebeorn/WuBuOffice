/* test_col.c -- headless comment-thread store test. */
#include "col.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } } while(0)

int main(void){
    Col *c = col_create();
    int t1 = col_add(c, "node:5", "alice", "typo here");
    int t2 = col_add(c, "node:9", "bob", "clarify?");
    CHECK(t1==1 && t2==2, "thread ids");
    CHECK(col_thread_count(c)==2, "count");
    CHECK(strcmp(col_anchor(c,t1),"node:5")==0, "anchor");
    CHECK(strcmp(col_text(c,t1),"typo here")==0, "root text");
    CHECK(col_reply(c, t1, "carol", "fixed")==1, "reply");
    CHECK(col_reply_count(c,t1)==1, "reply count");
    const Reply *r = col_reply_at(c, t1, 0);
    CHECK(r && strcmp(r->author,"carol")==0 && strcmp(r->text,"fixed")==0, "reply fields");
    CHECK(col_resolve(c, t1, 1)==1, "resolve");
    CHECK(col_resolved(c,t1)==1, "resolved flag");
    CHECK(col_remove(c, t2)==1, "remove");
    CHECK(col_thread_count(c)==1, "count after remove");
    CHECK(col_id_at(c,0)==t1, "id at");
    col_destroy(c);
    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: col (threads, replies, resolve, remove)\n");
    return 0;
}
