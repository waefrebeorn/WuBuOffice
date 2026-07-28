/* test_crdt.c -- headless convergence + move/delete test for the CRDT. */
#include "crdt.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(c,msg) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(msg)); fails++; } } while(0)

static void dump(const Crdt *c, char *out, size_t n){
    out[0]=0;
    for (int i=0;i<crdt_count(c);i++){
        const char *v = crdt_get(c,i);
        strncat(out, v, n-strlen(out)-1);
    }
}

int main(void){
    /* two sites edit concurrently */
    Crdt *A = crdt_create("A"), *B = crdt_create("B");
    crdt_insert(A, 0, "a"); crdt_insert(A, 1, "b");
    crdt_insert(B, 0, "x"); crdt_insert(B, 1, "y");
    /* A deletes its 'a' */
    crdt_delete(A, 0);
    /* merge both ways */
    crdt_merge(A, B);
    crdt_merge(B, A);
    /* convergence: both hold the same live set */
    char sa[128], sb[128];
    dump(A, sa, sizeof sa); dump(B, sb, sizeof sb);
    CHECK(strcmp(sa, sb)==0, "convergence mismatch");
    /* live items: b (from A), x,y (from B) -> "xby" (clock order:
     * 1:A tomb, 1:B x, 2:A b, 2:B y -> live x,b,y) */
    CHECK(strcmp(sa, "xby")==0, "merged content");
    /* delete then insert convergence */
    Crdt *C = crdt_create("C"), *D = crdt_create("D");
    crdt_insert(C, 0, "m");
    crdt_delete(C, 0);          /* tombstone m */
    crdt_merge(D, C);
    crdt_insert(D, 0, "n");     /* concurrent insert */
    crdt_merge(C, D);
    char sc[64]; dump(C, sc, sizeof sc);
    CHECK(strcmp(sc, "n")==0, "tombstone + concurrent insert");
    crdt_destroy(A); crdt_destroy(B); crdt_destroy(C); crdt_destroy(D);

    /* serialize / deserialize round-trip */
    Crdt *E = crdt_create("E");
    crdt_insert(E,0,"one"); crdt_insert(E,1,"two"); crdt_delete(E,0);
    size_t len; char *buf = crdt_serialize(E, &len);
    CHECK(buf && len>0, "serialize");
    Crdt *F = crdt_create("F");
    CHECK(crdt_deserialize(F, buf, len)==1, "deserialize");
    char se[64]; dump(F, se, sizeof se);
    CHECK(strcmp(se, "two")==0, "deser content");
    free(buf); crdt_destroy(E); crdt_destroy(F);

    /* move */
    Crdt *G = crdt_create("G");
    crdt_insert(G,0,"a"); crdt_insert(G,1,"b"); crdt_insert(G,2,"c");
    crdt_move(G, 0, 3);   /* move a to end -> "bca" */
    char sg[64]; dump(G, sg, sizeof sg);
    CHECK(strcmp(sg, "bca")==0, "move");
    crdt_destroy(G);

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: crdt (concurrent convergence, tombstone+insert, serialize, move)\n");
    return 0;
}
