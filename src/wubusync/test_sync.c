/* test_sync.c -- headless local-first CRDT sync + shared lock test. */
#include "sync.h"
#include "crdt.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } } while(0)

int main(void){
    char dir[] = "/tmp/wubu_sync_test.XXXXXX";
    mkdtemp(dir);
    Sync *s = sync_open(dir);
    CHECK(s != NULL, "sync_open");

    /* two editors build replicas, merge via the store */
    Crdt *A = crdt_create("A");
    crdt_insert(A,0,"hello"); crdt_insert(A,1,"world");
    size_t la; char *sa = crdt_serialize(A, &la);
    CHECK(sync_put(s, "doc1", sa, la, "A")==1, "put A");

    Crdt *B = crdt_create("B");
    crdt_insert(B,0,"foo");  /* concurrent insert on another site */
    size_t lb; char *sb = crdt_serialize(B, &lb);
    /* merge B into the store's doc1 */
    int added = sync_merge(s, "doc1", sb, lb, "B");
    CHECK(added >= 1, "merge added");

    /* reload merged replica from store */
    char *blob=NULL; size_t blen=0;
    CHECK(sync_get(s, "doc1", &blob, &blen)==1, "get");
    Crdt *M = crdt_create("M");
    CHECK(crdt_deserialize(M, blob, blen)==1, "deser merged");
    /* live set should contain hello, world, foo (3) */
    CHECK(crdt_count(M)==3, "merged count");
    free(blob); crdt_destroy(M);

    /* shared lock: only one live holder */
    CHECK(sync_lock(s, "doc1", getpid(), "A")==1, "lock acquire");
    CHECK(sync_lock(s, "doc1", getpid()+1, "B")==0, "lock denied for other pid");
    CHECK(sync_unlock(s, "doc1", getpid())==1, "unlock");
    CHECK(sync_lock(s, "doc1", getpid()+1, "B")==1, "lock acquire after release");

    free(sa); free(sb); crdt_destroy(A); crdt_destroy(B); sync_close(s);
    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: sync (CRDT store merge + shared lock)\n");
    return 0;
}
