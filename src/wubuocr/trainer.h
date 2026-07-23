/* trainer.h -- training orchestration for conv3+MLP. Opaque structs, C11, no deps. */
#ifndef WUBUOCR_TRAINER_H
#define WUBUOCR_TRAINER_H

#include <stddef.h>
#include <stdint.h>

typedef struct Trainer Trainer;
typedef struct TrainConfig TrainConfig;
typedef struct TrainState TrainState;

/* Configuration for training */
struct TrainConfig {
    const char *data_dir;
    const char *train_stem;     /* e.g. "emnist-letters-train" */
    const char *test_stem;      /* e.g. "emnist-letters-test" */
    int nclass;                 /* number of classes (26 for letters) */
    int label_off;              /* label value offset (1 for EMNIST, 0 for MNIST) */

    int h1, h2;                 /* MLP hidden sizes */
    int epochs;
    long batch;
    long traincap;              /* cap training samples (0 = all) */
    int nthreads;               /* worker threads (0 = auto) */

    float lr;                   /* base learning rate */
    float conv_fac;             /* conv LR = lr * conv_fac */
    float mom;                  /* momentum (SGD) */
    float clip;                 /* grad norm clip (0 = off) */
    int opt;                    /* 0=sgd, 1=adam, 2=wubu */

    float leak;                 /* leaky ReLU slope */
    int inorm;                  /* instance norm (CN_INORM) */
    int cbam;                   /* CBAM attention */

    float aug_deg;              /* rotation augmentation degrees */
    int jit_px;                 /* translation jitter pixels */
    float smooth;               /* label smoothing */

    int do_norm;                /* z-norm features */
    int freeze_conv;
    int freeze_mlp;
    int phase2;                 /* MLP-only fine-tune epochs */
    int warmup;                 /* warmup steps */
    int cos;                    /* cosine schedule */
    int probe;                  /* debug probe */

    /* Architecture selection */
    int arch;                   /* 0=med_pad(32x32/576feats), 1=wide(28x28/1152feats), 2=xl(28x28/2304feats) */
};

/* Opaque training state (progress, metrics) */
struct TrainState {
    int epoch;
    long step;
    float lr;
    float train_acc;
    float test_acc;
    float loss;
};

/* Create trainer from config (allocates model, data, threads) */
Trainer *trainer_create(const TrainConfig *cfg);

/* Run one epoch; returns 0 on success, -1 on error */
int trainer_epoch(Trainer *tr, TrainState *out);

/* Run full training loop; returns final test accuracy */
float trainer_run(Trainer *tr);

/* Destroy trainer and free all resources */
void trainer_destroy(Trainer *tr);

/* Save model weights + norm stats */
int trainer_save(const Trainer *tr, const char *conv_path, const char *mlp_path);

/* Quick accuracy eval on test set (no training) */
float trainer_eval(const Trainer *tr);

#endif