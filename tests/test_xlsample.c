#include "../apps/wubucell/cell.h"
#include "../apps/wubucell/cell_read.h"
#include "../apps/wubucell/cell_internal.h"
#include <stdio.h>
int main(int argc,char**argv){
  const char *p = argc>1?argv[1]:"/tmp/wubes/sample.xlsx";
  wubucell_book *b=NULL;
  if(wubucell_read(p,&b)){ printf("load failed\n"); return 1; }
  int ok=1;
  for(int r=1;r<=4;r++) for(int c=1;c<=3;c++){
    int k=0; char t[64]=""; double num=0,cached=0;
    if(wubucell_get(b,1,c,r,&k,&t,&num,&cached)==0)
      printf("R%dc%d kind=%d num=%.1f cached=%.1f\n",r,c,k,num,cached);
    else printf("R%dc%d <empty>\n",r,c);
  }
  /* expect A1=14, B1=49, A3=7, B4=15 */
  double vals[4]; int got[4]={0};
  int refs[4][2]={{1,1},{2,1},{1,3},{2,4}}; char *exp[4]={"A1=14","B1=49","A3=7","B4=15"};
  for(int r=1;r<=4;r++)for(int c=1;c<=3;c++){
    int k;char t[64];double num,cached;
    if(wubucell_get(b,1,c,r,&k,&t,&num,&cached)==0){
      for(int i=0;i<4;i++) if(refs[i][0]==c&&refs[i][1]==r){
        vals[i] = (k==WUBUCELL_NUM)? num : cached; got[i]=1; }
    }
  }
  double expex[4]={14,49,7,15};
  for(int i=0;i<4;i++){
    if(!got[i]){printf("FAIL %s not found\n",exp[i]);ok=0;}
    else if(vals[i]!=expex[i]){printf("FAIL %s = %.1f (want %.1f)\n",exp[i],vals[i],expex[i]);ok=0;}
    else printf("OK %s = %.1f\n",exp[i],vals[i]);
  }
  wubucell_free(b);
  printf(ok?"XLSAMPLE PASS\n":"XLSAMPLE FAIL\n");
  return ok?0:1;
}
