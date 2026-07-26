/* ocl_gru.c -- OpenCL fallback (#98) for the GRU recurrent forward.
 *
 * Mirrors gru_fwd_dir() in gru.c EXACTLY (same weight layout via gru_layout.h,
 * same SIG/tanh math, same recurrence h_t = (1-z)*h_{t-1} + z*n). The recurrent
 * dependency (h_t needs h_{t-1}) is honoured by running one kernel dispatch per
 * timestep: each dispatch parallelises the H units of that timestep across the
 * GPU, reading the previous hidden state and writing the new one. This is a
 * drop-in accelerator; if OpenCL is unavailable or any dispatch fails it
 * returns 0 and the caller (gru_forward) falls back to the scalar CPU path.
 *
 * OPENCL IS OPTIONAL AT RUNTIME: symbols are resolved via dlopen/dlsym, so the
 * binary links with NO OpenCL dependency (works with -static). If libOpenCL.so
 * is absent on the host, ocl_gru_dir simply returns 0 and the CPU path runs.
 */
#include "gru.h"
#include "gru_layout.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <dlfcn.h>
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

/* ---- resolved OpenCL function pointers (lazy, via dlsym) ---- */
typedef struct {
    void           *handle;   /* dlopen handle (NULL if not loaded) */
    cl_platform_id  plat;
    cl_device_id    dev;
    cl_context      ctx;
    cl_command_queue q;
    cl_program      prog;
    cl_kernel       kern;
    int             ok;
    /* resolved OpenCL function pointers */
    cl_int  (*pGetPlatformIDs)(cl_uint, cl_platform_id*, cl_uint*);
    cl_int  (*pGetDeviceIDs)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*);
    cl_context (*pCreateContext)(const cl_context_properties*, cl_uint, const cl_device_id*,
                                 void(*)(const char*,const void*,size_t,void*), void*, cl_int*);
    cl_command_queue (*pCreateCommandQueue)(cl_context, cl_device_id, cl_command_queue_properties, cl_int*);
    cl_program (*pCreateProgramWithSource)(cl_context, cl_uint, const char**, const size_t*, cl_int*);
    cl_int  (*pBuildProgram)(cl_program, cl_uint, const cl_device_id*, const char*,
                             void(*)(cl_program,void*), void*);
    cl_kernel (*pCreateKernel)(cl_program, const char*, cl_int*);
    cl_mem  (*pCreateBuffer)(cl_context, cl_mem_flags, size_t, void*, cl_int*);
    cl_int  (*pEnqueueWriteBuffer)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, const void*, cl_uint, const cl_event*, cl_event*);
    cl_int  (*pSetKernelArg)(cl_kernel, cl_uint, size_t, const void*);
    cl_int  (*pEnqueueNDRangeKernel)(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*, cl_uint, const cl_event*, cl_event*);
    cl_int  (*pEnqueueReadBuffer)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void*, cl_uint, const cl_event*, cl_event*);
    void    (*pReleaseMemObject)(cl_mem);
    void    (*pReleaseKernel)(cl_kernel);
    void    (*pReleaseProgram)(cl_program);
    void    (*pReleaseCommandQueue)(cl_command_queue);
    void    (*pReleaseContext)(cl_context);
    cl_int  (*pFinish)(cl_command_queue);
} ocl_state_t;
static ocl_state_t g;

#define DLOOP(sym, type, name) \
    do { g.name = (type)dlsym(g.handle, #sym); if (!g.name) { g.ok=0; return 0; } } while(0)

static int ocl_try_init(void){
    if (g.ok) return 1;
    if (g.ok < 0) { /* -1 = not tried, 0 = failed permanently */ }
    memset(&g, 0, sizeof g);
    g.ok = -1;
    /* try common OpenCL shared-object names */
    const char *names[] = { "libOpenCL.so", "libOpenCL.so.1",
                             "libOpenCL.so.1.0.0", "OpenCL" };
    void *h = NULL;
    for (size_t i=0;i<sizeof(names)/sizeof(names[0]);i++){
        h = dlopen(names[i], RTLD_NOW|RTLD_LOCAL);
        if (h) break;
    }
    if (!h) return 0;
    g.handle = h;

    cl_int (*pGetPlatformIDs)(cl_uint, cl_platform_id*, cl_uint*);
    cl_int (*pGetDeviceIDs)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*);
    cl_context (*pCreateContext)(const cl_context_properties*, cl_uint, const cl_device_id*,
                                 void(*)(const char*,const void*,size_t,void*), void*, cl_int*);
    cl_command_queue (*pCreateCommandQueue)(cl_context, cl_device_id, cl_command_queue_properties, cl_int*);
    cl_program (*pCreateProgramWithSource)(cl_context, cl_uint, const char**, const size_t*, cl_int*);
    cl_int (*pBuildProgram)(cl_program, cl_uint, const cl_device_id*, const char*,
                            void(*)(cl_program,void*), void*);
    cl_kernel (*pCreateKernel)(cl_program, const char*, cl_int*);
    cl_mem (*pCreateBuffer)(cl_context, cl_mem_flags, size_t, void*, cl_int*);
    cl_int (*pEnqueueWriteBuffer)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, const void*,
                                   cl_uint, const cl_event*, cl_event*);
    cl_int (*pSetKernelArg)(cl_kernel, cl_uint, size_t, const void*);
    cl_int (*pEnqueueNDRangeKernel)(cl_command_queue, cl_kernel, cl_uint, const size_t*,
                                     const size_t*, const size_t*, cl_uint, const cl_event*, cl_event*);
    cl_int (*pEnqueueReadBuffer)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void*,
                                  cl_uint, const cl_event*, cl_event*);
    void (*pReleaseMemObject)(cl_mem);
    void (*pReleaseKernel)(cl_kernel);
    void (*pReleaseProgram)(cl_program);
    void (*pReleaseCommandQueue)(cl_command_queue);
    void (*pReleaseContext)(cl_context);
    cl_int (*pFinish)(cl_command_queue);

    #define R(sym, field) do { g.field=(void*)dlsym(h, #sym); if(!g.field){ g.ok=0; return 0; } } while(0)
    R(clGetPlatformIDs, pGetPlatformIDs);
    R(clGetDeviceIDs, pGetDeviceIDs);
    R(clCreateContext, pCreateContext);
    R(clCreateCommandQueue, pCreateCommandQueue);
    R(clCreateProgramWithSource, pCreateProgramWithSource);
    R(clBuildProgram, pBuildProgram);
    R(clCreateKernel, pCreateKernel);
    R(clCreateBuffer, pCreateBuffer);
    R(clEnqueueWriteBuffer, pEnqueueWriteBuffer);
    R(clSetKernelArg, pSetKernelArg);
    R(clEnqueueNDRangeKernel, pEnqueueNDRangeKernel);
    R(clEnqueueReadBuffer, pEnqueueReadBuffer);
    R(clReleaseMemObject, pReleaseMemObject);
    R(clReleaseKernel, pReleaseKernel);
    R(clReleaseProgram, pReleaseProgram);
    R(clReleaseCommandQueue, pReleaseCommandQueue);
    R(clReleaseContext, pReleaseContext);
    R(clFinish, pFinish);
    #undef R

    cl_uint np = 0;
    if ((*(cl_int(*)(cl_uint,cl_platform_id*,cl_uint*))g.pGetPlatformIDs)(1,&g.plat,&np)!=CL_SUCCESS || np==0) return 0;
    cl_uint nd = 0;
    if (g.pGetDeviceIDs(g.plat, CL_DEVICE_TYPE_ALL, 1, &g.dev, &nd)!=CL_SUCCESS || nd==0) return 0;
    cl_int err;
    g.ctx = g.pCreateContext(NULL,1,&g.dev,NULL,NULL,&err);
    if (!g.ctx || err!=CL_SUCCESS) return 0;
    g.q = g.pCreateCommandQueue(g.ctx, g.dev, 0, &err);
    if (!g.q || err!=CL_SUCCESS) return 0;

    static const char *S =
    "__kernel void gru_fwd(\n"
    "    __global const float *W, int H, int D,\n"
    "    __global const float *x, __global const float *hprev,\n"
    "    __global float *zout, __global float *rout, __global float *hout){\n"
    "  int j = get_global_id(0);\n"
    "  if (j >= H) return;\n"
    "  int Wz=0, Wr=H*D, Wh=2*H*D;\n"
    "  int Uz=3*H*D, Ur=Uz+H*H, Uh=Ur+H*H;\n"
    "  int Bz=Uh+H*H, Br=Bz+H, Bh=Br+H;\n"
    "  float az=0.0f, ar=0.0f;\n"
    "  for (int i=0;i<D;i++){ az += W[Wz+j*D+i]*x[i]; ar += W[Wr+j*D+i]*x[i]; }\n"
    "  for (int k=0;k<H;k++){ float pv=hprev[k]; az += W[Uz+j*H+k]*pv; ar += W[Ur+j*H+k]*pv; }\n"
    "  az += W[Bz+j]; ar += W[Br+j];\n"
    "  float zv = 1.0f/(1.0f+exp(-az));\n"
    "  float rv = 1.0f/(1.0f+exp(-ar));\n"
    "  float ac=0.0f;\n"
    "  for (int i=0;i<D;i++) ac += W[Wh+j*D+i]*x[i];\n"
    "  for (int k=0;k<H;k++) ac += W[Uh+j*H+k]*(rv*hprev[k]);\n"
    "  ac += W[Bh+j];\n"
    "  float nv = tanh(ac);\n"
    "  float hv = (1.0f - zv)*hprev[j] + zv*nv;\n"
    "  zout[j]=zv; rout[j]=rv; hout[j]=hv;\n"
    "}\n";
    size_t slen = strlen(S);
    g.prog = g.pCreateProgramWithSource(g.ctx, 1, &S, &slen, &err);
    if (!g.prog || err!=CL_SUCCESS) return 0;
    if (g.pBuildProgram(g.prog, 1, &g.dev, NULL, NULL, NULL)!=CL_SUCCESS) return 0;
    g.kern = g.pCreateKernel(g.prog, "gru_fwd", &err);
    if (!g.kern || err!=CL_SUCCESS) return 0;
    g.ok = 1;
    return 1;
}

int ocl_gru_dir(const float *P, int H, int D, int T, const float *x, int dir,
                float *zbuf, float *rbuf, float *hbuf){
    if (H <= 0 || D <= 0 || T <= 0 || !P || !x || !zbuf || !rbuf || !hbuf) return 0;
    if (!ocl_try_init()) return 0;

    GRUOffs o = gru_offs(H, D);
    const float *Pd = P + (dir ? o.block : 0);

    cl_int err;
    cl_mem wbuf = g.pCreateBuffer(g.ctx, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,
                                  (size_t)o.block*sizeof(float), (void*)Pd, &err);
    cl_mem xbuf = g.pCreateBuffer(g.ctx, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,
                                  (size_t)D*sizeof(float), NULL, &err);
    cl_mem hpb  = g.pCreateBuffer(g.ctx, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,
                                  (size_t)H*sizeof(float), NULL, &err);
    cl_mem zb   = g.pCreateBuffer(g.ctx, CL_MEM_WRITE_ONLY, (size_t)H*sizeof(float), NULL, &err);
    cl_mem rb   = g.pCreateBuffer(g.ctx, CL_MEM_WRITE_ONLY, (size_t)H*sizeof(float), NULL, &err);
    cl_mem hb   = g.pCreateBuffer(g.ctx, CL_MEM_WRITE_ONLY, (size_t)H*sizeof(float), NULL, &err);
    float *zero = (float*)calloc((size_t)H, sizeof(float));
    if (!wbuf||!xbuf||!hpb||!zb||!rb||!hb||!zero){
        if(wbuf)g.pReleaseMemObject(wbuf); if(xbuf)g.pReleaseMemObject(xbuf);
        if(hpb)g.pReleaseMemObject(hpb); if(zb)g.pReleaseMemObject(zb);
        if(rb)g.pReleaseMemObject(rb); if(hb)g.pReleaseMemObject(hb);
        free(zero); return 0;
    }

    size_t gsz = (size_t)((H+63)/64)*64;
    int ok = 1;
    for (int t=0; t<T; t++){
        int ti = dir ? (T-1-t) : t;
        const float *xt = x + (size_t)ti*D;
        const float *hp = (t==0) ? zero : (hbuf + (size_t)(t-1)*H);
        if (g.pEnqueueWriteBuffer(g.q,xbuf,CL_TRUE,0,(size_t)D*sizeof(float),xt,0,NULL,NULL)!=CL_SUCCESS){ok=0;break;}
        if (g.pEnqueueWriteBuffer(g.q,hpb,CL_TRUE,0,(size_t)H*sizeof(float),hp,0,NULL,NULL)!=CL_SUCCESS){ok=0;break;}
        g.pSetKernelArg(g.kern,0,sizeof(cl_mem),&wbuf);
        g.pSetKernelArg(g.kern,1,sizeof(int),&H);
        g.pSetKernelArg(g.kern,2,sizeof(int),&D);
        g.pSetKernelArg(g.kern,3,sizeof(cl_mem),&xbuf);
        g.pSetKernelArg(g.kern,4,sizeof(cl_mem),&hpb);
        g.pSetKernelArg(g.kern,5,sizeof(cl_mem),&zb);
        g.pSetKernelArg(g.kern,6,sizeof(cl_mem),&rb);
        g.pSetKernelArg(g.kern,7,sizeof(cl_mem),&hb);
        if (g.pEnqueueNDRangeKernel(g.q,g.kern,1,NULL,&gsz,NULL,0,NULL,NULL)!=CL_SUCCESS){ok=0;break;}
        if (g.pEnqueueReadBuffer(g.q,zb,CL_TRUE,0,(size_t)H*sizeof(float),zbuf+(size_t)t*H,0,NULL,NULL)!=CL_SUCCESS){ok=0;break;}
        if (g.pEnqueueReadBuffer(g.q,rb,CL_TRUE,0,(size_t)H*sizeof(float),rbuf+(size_t)t*H,0,NULL,NULL)!=CL_SUCCESS){ok=0;break;}
        if (g.pEnqueueReadBuffer(g.q,hb,CL_TRUE,0,(size_t)H*sizeof(float),hbuf+(size_t)t*H,0,NULL,NULL)!=CL_SUCCESS){ok=0;break;}
    }
    g.pFinish(g.q);

    g.pReleaseMemObject(wbuf); g.pReleaseMemObject(xbuf); g.pReleaseMemObject(hpb);
    g.pReleaseMemObject(zb);   g.pReleaseMemObject(rb);   g.pReleaseMemObject(hb);
    free(zero);
    return ok;
}
