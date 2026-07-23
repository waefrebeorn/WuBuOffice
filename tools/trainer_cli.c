/* trainer_cli.c -- CLI for modular trainer */

#include "trainer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Unbuffered stdout so piped output is visible immediately */
static void init_buffering(void) { setbuf(stdout, NULL); setbuf(stderr, NULL); }

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <data_dir> [options]\n"
        "Options (env vars also work):\n"
        "  --train-stem <str>    Train file stem (default: emnist-letters-train)\n"
        "  --test-stem <str>     Test file stem (default: emnist-letters-test)\n"
        "  --nclass <int>        Number of classes (default: 26)\n"
        "  --label-off <int>     Label offset (default: 1)\n"
        "  --h1 <int>            MLP hidden 1 (default: 128)\n"
        "  --h2 <int>            MLP hidden 2 (default: 64)\n"
        "  --epochs <int>        Epochs (default: 20)\n"
        "  --batch <int>         Batch size (default: 256)\n"
        "  --traincap <int>      Cap training samples (default: 0=all)\n"
        "  --threads <int>       Worker threads (default: auto)\n"
        "  --lr <float>          Learning rate (default: 0.002 adam / 0.05 sgd)\n"
        "  --conv-fac <float>    Conv LR factor (default: 0.1)\n"
        "  --mom <float>         Momentum (default: 0.9)\n"
        "  --clip <float>        Grad clip L2 (default: 1.0)\n"
        "  --opt <sgd|adam|wubu> Optimizer (default: adam)\n"
        "  --leak <float>        Leaky ReLU slope (default: 0.1)\n"
        "  --inorm <0|1>         Instance norm (default: 1)\n"
        "  --cbam <0|1>          CBAM attention (default: 0)\n"
        "  --aug <float>         Rotation aug degrees (default: 0)\n"
        "  --jit <int>           Translation jitter px (default: 2)\n"
        "  --smooth <float>      Label smoothing (default: 0.1)\n"
        "  --norm <0|1>          Z-norm features (default: 1)\n"
        "  --freeze-conv <0|1>   Freeze conv (default: 0)\n"
        "  --freeze-mlp <0|1>    Freeze MLP (default: 0)\n"
        "  --phase2 <int>        MLP-only epochs (default: 0)\n"
        "  --warmup <int>        Warmup steps (default: 200)\n"
        "  --cos <0|1>           Cosine schedule (default: 1)\n"
        "  --save-conv <path>    Save conv weights\n"
        "  --save-mlp <path>     Save MLP weights\n"
        "  --arch <int>          Architecture: 0=MED_PAD(576), 1=WIDE(1152), 2=XL(2304) (default: 0)\n"
        "  --eval-only           Only evaluate (no training)\n"
        , prog);
}

int main(int argc, char **argv) {
    init_buffering();
    TrainConfig cfg = {
        .data_dir = argc>1 ? argv[1] : "data/emnist",
        .train_stem = "emnist-letters-train",
        .test_stem = "emnist-letters-test",
        .nclass = 26,
        .label_off = 1,
        .h1 = 128, .h2 = 64,
        .epochs = 20,
        .batch = 256,
        .traincap = 0,
        .nthreads = 0,
        .lr = 0,
        .conv_fac = 0.1f,
        .mom = 0.9f,
        .clip = 1.0f,
        .opt = 1,
        .leak = 0.1f,
        .inorm = 1,
        .cbam = 0,
        .aug_deg = 0.0f,
        .jit_px = 2,
        .smooth = 0.1f,
        .do_norm = 1,
        .freeze_conv = 0,
        .freeze_mlp = 0,
        .phase2 = 0,
        .warmup = 200,
        .cos = 1,
    };
    const char *save_conv = NULL, *save_mlp = NULL;
    int eval_only = 0;

    for(int i=2;i<argc;i++){
        if(!strcmp(argv[i],"--train-stem") && i+1<argc) cfg.train_stem=argv[++i];
        else if(!strcmp(argv[i],"--test-stem") && i+1<argc) cfg.test_stem=argv[++i];
        else if(!strcmp(argv[i],"--nclass") && i+1<argc) cfg.nclass=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--label-off") && i+1<argc) cfg.label_off=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--h1") && i+1<argc) cfg.h1=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--h2") && i+1<argc) cfg.h2=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--epochs") && i+1<argc) cfg.epochs=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--batch") && i+1<argc) cfg.batch=atol(argv[++i]);
        else if(!strcmp(argv[i],"--traincap") && i+1<argc) cfg.traincap=atol(argv[++i]);
        else if(!strcmp(argv[i],"--threads") && i+1<argc) cfg.nthreads=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--lr") && i+1<argc) cfg.lr=(float)atof(argv[++i]);
        else if(!strcmp(argv[i],"--conv-fac") && i+1<argc) cfg.conv_fac=(float)atof(argv[++i]);
        else if(!strcmp(argv[i],"--mom") && i+1<argc) cfg.mom=(float)atof(argv[++i]);
        else if(!strcmp(argv[i],"--clip") && i+1<argc) cfg.clip=(float)atof(argv[++i]);
        else if(!strcmp(argv[i],"--opt") && i+1<argc) {
            if(!strcmp(argv[++i],"sgd")) cfg.opt=0;
            else if(!strcmp(argv[i],"adam")) cfg.opt=1;
            else if(!strcmp(argv[i],"wubu")) cfg.opt=2;
        }
        else if(!strcmp(argv[i],"--leak") && i+1<argc) cfg.leak=(float)atof(argv[++i]);
        else if(!strcmp(argv[i],"--inorm") && i+1<argc) cfg.inorm=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--cbam") && i+1<argc) cfg.cbam=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--aug") && i+1<argc) cfg.aug_deg=(float)atof(argv[++i]);
        else if(!strcmp(argv[i],"--jit") && i+1<argc) cfg.jit_px=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--smooth") && i+1<argc) cfg.smooth=(float)atof(argv[++i]);
        else if(!strcmp(argv[i],"--norm") && i+1<argc) cfg.do_norm=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--freeze-conv") && i+1<argc) cfg.freeze_conv=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--freeze-mlp") && i+1<argc) cfg.freeze_mlp=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--phase2") && i+1<argc) cfg.phase2=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--warmup") && i+1<argc) cfg.warmup=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--cos") && i+1<argc) cfg.cos=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--save-conv") && i+1<argc) save_conv=argv[++i];
        else if(!strcmp(argv[i],"--save-mlp") && i+1<argc) save_mlp=argv[++i];
        else if(!strcmp(argv[i],"--arch") && i+1<argc) cfg.arch=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--eval-only")) eval_only=1;
        else if(!strcmp(argv[i],"-h")||!strcmp(argv[i],"--help")) { usage(argv[0]); return 0; }
        else { fprintf(stderr,"Unknown arg: %s\n",argv[i]); usage(argv[0]); return 1; }
    }

    /* Env overrides */
    if(getenv("CN_TRAIN")) cfg.train_stem=getenv("CN_TRAIN");
    if(getenv("CN_TEST")) cfg.test_stem=getenv("CN_TEST");
    if(getenv("CN_CLASS")) cfg.nclass=atoi(getenv("CN_CLASS"));
    if(getenv("CN_LABOFF")) cfg.label_off=atoi(getenv("CN_LABOFF"));
    if(getenv("CN_H1")) cfg.h1=atoi(getenv("CN_H1"));
    if(getenv("CN_H2")) cfg.h2=atoi(getenv("CN_H2"));
    if(getenv("CN_EPOCHS")) cfg.epochs=atoi(getenv("CN_EPOCHS"));
    if(getenv("CN_BATCH")) cfg.batch=atol(getenv("CN_BATCH"));
    if(getenv("CN_TRAINCAP")) cfg.traincap=atol(getenv("CN_TRAINCAP"));
    if(getenv("CN_THREADS")) cfg.nthreads=atoi(getenv("CN_THREADS"));
    if(getenv("CN_LR")) cfg.lr=(float)atof(getenv("CN_LR"));
    if(getenv("CN_CONVF")) cfg.conv_fac=(float)atof(getenv("CN_CONVF"));
    if(getenv("CN_MOM")) cfg.mom=(float)atof(getenv("CN_MOM"));
    if(getenv("CN_CLIP")) cfg.clip=(float)atof(getenv("CN_CLIP"));
    if(getenv("CN_OPT")) { if(!strcmp(getenv("CN_OPT"),"sgd")) cfg.opt=0; else if(!strcmp(getenv("CN_OPT"),"wubu")) cfg.opt=2; else cfg.opt=1; }
    if(getenv("CN_LEAK")) cfg.leak=(float)atof(getenv("CN_LEAK"));
    if(getenv("CN_INORM")) cfg.inorm=atoi(getenv("CN_INORM"));
    if(getenv("CBAM")) cfg.cbam=atoi(getenv("CBAM"));
    if(getenv("CN_AUG")) cfg.aug_deg=(float)atof(getenv("CN_AUG"));
    if(getenv("CN_JIT")) cfg.jit_px=atoi(getenv("CN_JIT"));
    if(getenv("CN_SMOOTH")) cfg.smooth=(float)atof(getenv("CN_SMOOTH"));
    if(getenv("CN_NORM")) cfg.do_norm=atoi(getenv("CN_NORM"));
    if(getenv("CN_FREEZE_CONV")) cfg.freeze_conv=atoi(getenv("CN_FREEZE_CONV"));
    if(getenv("CN_FREEZE_MLP")) cfg.freeze_mlp=atoi(getenv("CN_FREEZE_MLP"));
    if(getenv("CN_PHASE2")) cfg.phase2=atoi(getenv("CN_PHASE2"));
    if(getenv("CN_WARMUP")) cfg.warmup=atoi(getenv("CN_WARMUP"));
    if(getenv("CN_COS")) cfg.cos=atoi(getenv("CN_COS"));
    if(getenv("CN_ARCH")) cfg.arch=atoi(getenv("CN_ARCH"));

    if(cfg.lr==0) cfg.lr = (cfg.opt==0) ? 0.05f : 0.002f;

    Trainer *tr = trainer_create(&cfg);
    if(!tr){ fprintf(stderr,"trainer_create failed\n"); return 1; }

    if(eval_only){
        float acc = trainer_eval(tr);
        printf("Test accuracy: %.2f%%\n", acc);
    }else{
        float acc = trainer_run(tr);
        printf("\n=== FINAL RESULT ===\n");
        printf("Test accuracy: %.2f%%\n", acc);
        if(save_conv||save_mlp){
            if(trainer_save(tr, save_conv, save_mlp)==0) printf("Saved models.\n");
            else fprintf(stderr,"Save failed.\n");
        }
    }
    trainer_destroy(tr);
    return 0;
}