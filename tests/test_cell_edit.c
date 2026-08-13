#include "../apps/wubucell/cell.h"
#include "../apps/wubucell/cell_internal.h"
#include <stdio.h>
#include <string.h>
/* Verify that editing a value recomputes dependent formulas immediately. */
int main(void){
  wubucell_book *b = wubucell_create();
  int sh = wubucell_sheet(b, "S");
  wubucell_cell_n(b, sh, 1, 1, 10);          /* A1=10 */
  wubucell_cell_f(b, sh, 2, 1, "A1+1", 0);   /* B1=A1+1 */
  cell_eval_all(b);
  int ok=1;
  {int k;char t[64];double n,c;
   wubucell_get(b,sh,2,1,&k,&t,&n,&c);
   printf("B1 after eval = %.0f\n", c);
   if (c!=11){ printf("FAIL B1!=11\n"); ok=0; }}
  wubucell_cell_n(b, sh, 1, 1, 20);          /* edit A1 -> 20 */
  cell_eval_all(b);
  {int k;char t[64];double n,c;
   wubucell_get(b,sh,2,1,&k,&t,&n,&c);
   printf("B1 after A1=20 = %.0f\n", c);
   if (c!=21){ printf("FAIL B1!=21\n"); ok=0; }}
  wubucell_free(b);
  printf(ok ? "CELL_EDIT PASS\n" : "CELL_EDIT FAIL\n");
  return ok ? 0 : 1;
}
