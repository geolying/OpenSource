/*
 * Copyright (c) 2017 by the Society of Exploration Geophysicists.
 * For more information, go to http://software.seg.org/2017/00XX .
 * You must read and accept usage terms at:
 * http://software.seg.org/disclaimer.txt before use.
 */

#include "par.h"
#include "segy.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <genfft.h>

int omp_get_max_threads(void);
int omp_get_num_threads(void);
void omp_set_num_threads(int num_threads);

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif
#define NINT(x) ((int)((x)>0.0?(x)+0.5:(x)-0.5))
int compareInt(const void *a, const void *b) 
{ return (*(int *)a-*(int *)b); }


#ifndef COMPLEX
typedef struct _complexStruct { /* complex number */
    float r,i;
} complex;
#endif/* complex */

int readShotData(char *filename, float *xrcv, float *xsrc, float *zsrc, int *xnx, complex *cdata, int nw, int nw_low, int nshots,
int nx, int nxs, float fxsb, float dxs, int ntfft, int mode, float scale, float tsq, float Q, float f0, int reci, int *nshots_r, int *isxcount, int *reci_xsrc,  int *reci_xrcv, float *ixmask, int verbose);
int readTinvData(char *filename, float *xrcv, float *xsrc, float *zsrc, int *xnx, int Nfoc, int nx, int ntfft, int mode, int *maxval, float *tinv, int hw, int verbose);
int writeDataIter(char *file_iter, float *data, segy *hdrs, int n1, int n2, float d2, float f2, int n2out, int Nfoc, float *xsyn,
float *zsyn, int *ixpos, int npos, int iter);

void name_ext(char *filename, char *extension);

int getFileInfo(char *filename, int *n1, int *n2, int *ngath, float *d1, float *d2, float *f1, float *f2, float *xmin, float *xmax, float *sclsxgx, int *ntraces);
int readData(FILE *fp, float *data, segy *hdrs, int n1);
int writeData(FILE *fp, float *data, segy *hdrs, int n1, int n2);
int disp_fileinfo(char *file, int n1, int n2, float f1, float f2, float d1, float d2, segy *hdrs);
double wallclock_time(void);

void synthesis(complex *Refl, complex *Fop, float *Top, float *iRN, int nx, int nt, int nxs, int nts, float dt, float *xsyn, int
Nfoc, float *xrcv, float *xsrc, int *xnx, float fxse, float fxsb, float dxs, float dxsrc, float dx, int ntfft, int
nw, int nw_low, int nw_high,  int mode, int reci, int nshots, int *ixpos, int npos, double *tfft, int *isxcount, int
*reci_xsrc,  int *reci_xrcv, float *ixmask, int verbose);

void synthesisPosistions(int nx, int nt, int nxs, int nts, float dt, float *xsyn, int Nfoc, float *xrcv, float *xsrc, int *xnx,
float fxse, float fxsb, float dxs, float dxsrc, float dx, int nshots, int *ixpos, int *npos, int *isxcount, int countmin, int reci, int verbose);

int linearsearch(int *array, size_t N, int value);

/*********************** self documentation **********************/
char *sdoc[] = {
" ",
" MARCHENKO_primaries - Iterative primary reflections retrieval",
" ",
" marchenko_primaries file_tinv= file_shot= [optional parameters]",
" ",
" Required parameters: ",
" ",
"   file_shot= ............... Reflection response: R",
" ",
" Optional parameters: ",
" ",
" INTEGRATION ",
"   ishot=nshots/2 ........... shot position(s) to remove internal multiples ",
"   file_src=spike ........... convolve ishot(s) with source wavelet",
"   file_tinv= ............... use file_tinv to remove internal multiples",
" COMPUTATION",
"   cgemm=0 .................. 1: use BLAS's cgemm to compute R*ishot",
"   tap=0 .................... lateral taper focusing(1), shot(2) or both(3)",
"   ntap=0 ................... number of taper points at boundaries",
"   fmin=0 ................... minimum frequency in the Fourier transform",
"   fmax=70 .................. maximum frequency in the Fourier transform",
"   file_src= ................ optional source wavelet to convolve selected ishot's",
" MARCHENKO ITERATIONS ",
"   niter=10 ................. number of iterations",
"   istart=20 ................ start sample of iterations for primaries",
"   iend=nt .................. end sample of iterations for primaries",
" MUTE-WINDOW ",
"   shift=20 ................. number of points to account for wavelet (epsilon in papers)",
"   smooth=5 ................. number of points to smooth mute with cosine window",
" REFLECTION RESPONSE CORRECTION ",
"   tsq=0.0 .................. scale factor n for t^n for true amplitude recovery",
"   Q=0.0 .......,............ Q correction factor",
"   f0=0.0 ................... ... for Q correction factor",
"   scale=2 .................. scale factor of R for summation of Ni with G_d",
"   pad=0 .................... amount of samples to pad the reflection series",
"   reci=0 ................... 1; add receivers as shots 2; only use receivers as shot positions",
"   countmin=0 ............... 0.3*nxrcv; minumum number of reciprocal traces for a contribution",
" OUTPUT DEFINITION ",
"   file_rr= ................. output file with primary only shot record",
"   T=0 ...................... :1 compute transmission-losses compensated primaries ",
"   verbose=0 ................ silent option; >0 displays info",
" ",
" ",
" author  : Lele Zhang & Jan Thorbecke : 2019 ",
" ",
NULL};
/**************** end self doc ***********************************/

int main (int argc, char **argv)
{
    FILE    *fp_out, *fp_rr, *fp_w;
    size_t nread;
    int     i, j, l, ret, nshots, Nfoc, nt, nx, nts, nxs, ngath;
    int     size, n1, n2, ntap, tap, di, ntraces, pad;
    int     nw, nw_low, nw_high, nfreq, *xnx, *xnxsyn;
    int     reci, countmin, mode, n2out, verbose, ntfft;
    int     iter, niter, tracf, *muteW, *tsynW;
	int		hw, ii, ishot, istart, iend, k, m;
    int     smooth, shift, *ixpos, npos, ix, cgemm;
    int     nshots_r, *isxcount, *reci_xsrc, *reci_xrcv;
    float   fmin, fmax, *tapersh, *tapersy, fxf, dxf, *xsrc, *xrcv, *zsyn, *zsrc, *xrcvsyn;
    double  t0, t1, t2, t3, tsyn, tread, tfft, tcopy, tii, energyNi, energyN0;
    float   d1, d2, f1, f2, fxsb, fxse, ft, fx, *xsyn, dxsrc;
    float   *G_d, *DD, *RR, dt, dx, dxs, scl, mem, T;
    float   *f1min, *iRN, *Ni, *rtrace, *tmpdata;
    float   xmin, xmax, scale, tsq, Q, f0;
    float   *ixmask;
    float   grad2rad, p, src_angle, src_velo;
    complex *Refl, *Fop, *ctrace, *cwave;
    char    *file_tinv, *file_shot, *file_rr, *file_src;
    segy    *hdrs_out, hdr;

    initargs(argc, argv);
    requestdoc(1);

    tsyn = tread = tfft = tcopy = tii = 0.0;
    t0   = wallclock_time();

    if (!getparstring("file_shot", &file_shot)) file_shot = NULL;
    if (!getparstring("file_tinv", &file_tinv)) file_tinv = NULL;
    if(!getparstring("file_src", &file_src)) file_src = NULL;
    if (!getparstring("file_rr", &file_rr)) file_rr = NULL;
    if (!getparint("verbose", &verbose)) verbose = 0;
    if (file_tinv == NULL && file_shot == NULL) 
        verr("file_tinv and file_shot cannot be both input pipe");
    if (!getparstring("file_rr", &file_rr)) {
        if (verbose) vwarn("parameter file_rr not found, assume pipe");
        file_rr = NULL;
    }
    if (!getparfloat("fmin", &fmin)) fmin = 0.0;
    if (!getparfloat("fmax", &fmax)) fmax = 70.0;
    if (!getparint("reci", &reci)) reci = 0;
    if (!getparfloat("scale", &scale)) scale = 2.0;
    if (!getparfloat("tsq", &tsq)) tsq = 0.0;
    if (!getparfloat("Q", &Q)) Q = 0.0;
    if (!getparfloat("f0", &f0)) f0 = 0.0;
    if (!getparint("tap", &tap)) tap = 0;
    if (!getparint("ntap", &ntap)) ntap = 0;
    if (!getparint("pad", &pad)) pad = 0;
    if (!getparfloat("T", &T)) T = 1.0;
	if (T>=0) T=1.0;
	else T=-1.0;

    if(!getparint("niter", &niter)) niter = 10;
    if(!getparint("hw", &hw)) hw = 15;
    if(!getparint("smooth", &smooth)) smooth = 5;
    if(!getparint("shift", &shift)) shift=20;
    if(!getparint("cgemm", &cgemm)) cgemm=0;

    if (reci && ntap) vwarn("tapering influences the reciprocal result");

/*================ Reading info about shot and initial operator sizes ================*/

    ngath = 0; /* setting ngath=0 scans all traces; nx contains maximum traces/gather */
    ret = getFileInfo(file_shot, &nt, &nx, &ngath, &d1, &dx, &ft, &fx, &xmin, &xmax, &scl, &ntraces);
    nshots = ngath;
//    assert (nxs >= nshots);

    if (!getparfloat("dt", &dt)) dt = d1;
    if(!getparint("istart", &istart)) istart=20;
    if(!getparint("iend", &iend)) iend=nt;

	if (file_tinv != NULL) {
    	ngath = 0; /* setting ngath=0 scans all traces; n2 contains maximum traces/gather */
    	ret = getFileInfo(file_tinv, &n1, &n2, &ngath, &d1, &d2, &f1, &f2, &xmin, &xmax, &scl, &ntraces);
        Nfoc = ngath;
        nxs  = n2;
        nts  = n1;
        dxs  = d2;
        fxsb = f2;
	}
	else {
		/* 'G_d' is one of the shot records */
    	if(!getparint("ishot", &ishot)) ishot=(nshots)/2;
    	Nfoc = 1;
    	nxs  = nx; 
    	nts  = nt;
    	dxs  = dx; 
    	fxsb = fx; 
	}

    ntfft = optncr(MAX(nt+pad, nts+pad)); 
    nfreq = ntfft/2+1;
    nw_low = (int)MIN((fmin*ntfft*dt), nfreq-1);
    nw_low = MAX(nw_low, 1);
    nw_high = MIN((int)(fmax*ntfft*dt), nfreq-1);
    nw  = nw_high - nw_low + 1;
    if (!getparint("countmin", &countmin)) countmin = 0.3*nx;

/* ========================= Opening wavelet file ====================== */

    cwave = (complex *)calloc(ntfft,sizeof(complex));
    if (file_src != NULL){
        if (verbose) vmess("Reading wavelet from file %s.", file_src);

        fp_w = fopen(file_src, "r");
        if (fp_w == NULL) verr("error on opening input file_src=%s", file_src);
    	nread = fread(&hdr, 1, TRCBYTES, fp_w);
        assert (nread == TRCBYTES);
        tmpdata = (float *)malloc(hdr.ns*sizeof(float));
        nread = fread(tmpdata, sizeof(float), hdr.ns, fp_w);
        assert (nread == hdr.ns);
        fclose(fp_w);

        dt = d1; // To Do check dt wavelet is the same as reflection data
    	rtrace = (float *)calloc(ntfft,sizeof(float));

		/* To Do add samples in middle */
        if (hdr.ns <= ntfft) {
            for (i = 0; i < hdr.ns; i++) rtrace[i] = tmpdata[i];
            for (i = hdr.ns; i < ntfft; i++) rtrace[i] = 0.0;
        }
        else {
            vwarn("file_src has more samples than reflection data: truncated to ntfft = %d", ntfft);
            for (i = 0; i < ntfft; i++) rtrace[i] = tmpdata[i];
        }
        rc1fft(rtrace, cwave, ntfft, -1);
        free(tmpdata);
        free(rtrace);
    }
	else {
        for (i = 0; i < nfreq; i++) cwave[i].r = 1.0;
	}
    
/*================ Allocating all data arrays ================*/

    Fop     = (complex *)calloc(nxs*nw*Nfoc,sizeof(complex));
    f1min   = (float *)calloc(Nfoc*nxs*ntfft,sizeof(float));
    iRN     = (float *)calloc(Nfoc*nxs*ntfft,sizeof(float));
    Ni      = (float *)calloc(Nfoc*nxs*ntfft,sizeof(float));
    G_d     = (float *)calloc(Nfoc*nxs*ntfft,sizeof(float));
    DD      = (float *)calloc(Nfoc*nxs*ntfft,sizeof(float));
    RR      = (float *)calloc(Nfoc*nxs*ntfft,sizeof(float));
    muteW   = (int *)calloc(Nfoc*nxs,sizeof(int));
    tsynW   = (int *)malloc(Nfoc*nxs*sizeof(int)); // time-shift for Giovanni's plane-wave on non-zero times
    tapersy = (float *)malloc(nxs*sizeof(float));
    xrcvsyn = (float *)calloc(Nfoc*nxs,sizeof(float)); // x-rcv postions of focal points
    xsyn    = (float *)malloc(Nfoc*sizeof(float)); // x-src position of focal points
    zsyn    = (float *)malloc(Nfoc*sizeof(float)); // z-src position of focal points
    xnxsyn  = (int *)calloc(Nfoc,sizeof(int)); // number of traces per focal point
    ixpos   = (int *)calloc(nxs,sizeof(int)); // x-position of source of shot in G_d domain (nxs with dxs)

    Refl    = (complex *)malloc(nw*nx*nshots*sizeof(complex));
    tapersh = (float *)malloc(nx*sizeof(float));
    xrcv    = (float *)calloc(nshots*nx,sizeof(float)); // x-rcv postions of shots
    xsrc    = (float *)calloc(nshots,sizeof(float)); //x-src position of shots
    zsrc    = (float *)calloc(nshots,sizeof(float)); // z-src position of shots
    xnx     = (int *)calloc(nshots,sizeof(int)); // number of traces per shot

	if (reci!=0) {
        reci_xsrc = (int *)malloc((nxs*nxs)*sizeof(int));
        reci_xrcv = (int *)malloc((nxs*nxs)*sizeof(int));
        isxcount  = (int *)calloc(nxs,sizeof(int));
        ixmask  = (float *)calloc(nxs,sizeof(float));
    }

/*================ Reading shot records ================*/

    mode=1;
    readShotData(file_shot, xrcv, xsrc, zsrc, xnx, Refl, nw, nw_low, nshots, nx, nxs, fxsb, dxs, ntfft, 
         mode, scale, tsq, Q, f0, reci, &nshots_r, isxcount, reci_xsrc, reci_xrcv, ixmask, verbose);

    tapersh = (float *)malloc(nx*sizeof(float));
    if (tap == 2 || tap == 3) {
        for (j = 0; j < ntap; j++)
            tapersh[j] = (cos(PI*(j-ntap)/ntap)+1)/2.0;
        for (j = ntap; j < nx-ntap; j++)
            tapersh[j] = 1.0;
        for (j = nx-ntap; j < nx; j++)
            tapersh[j] =(cos(PI*(j-(nx-ntap))/ntap)+1)/2.0;
    }
    else {
        for (j = 0; j < nx; j++) tapersh[j] = 1.0;
    }
    if (tap == 2 || tap == 3) {
        if (verbose) vmess("Taper for shots applied ntap=%d", ntap);
        for (l = 0; l < nshots; l++) {
            for (j = 1; j < nw; j++) {
                for (i = 0; i < nx; i++) {
                    Refl[l*nx*nw+j*nx+i].r *= tapersh[i];
                    Refl[l*nx*nw+j*nx+i].i *= tapersh[i];
                }   
            }   
        }
    }
    free(tapersh);

    /* check consistency of header values */
    fxf = xsrc[0];
    if (nx > 1) dxf = (xrcv[nx-1] - xrcv[0])/(float)(nx-1);
    else dxf = d2;
    if (NINT(dx*1e3) != NINT(fabs(dxf)*1e3)) {
        vmess("dx in hdr.d1 (%.3f) and hdr.gx (%.3f) not equal",dx, dxf);
        if (dxf != 0) dx = fabs(dxf);
        else verr("gx hdrs not set");
        vmess("dx used => %f", dx);
    }
    
    dxsrc = (float)xsrc[1] - xsrc[0];
    if (dxsrc == 0) {
        vwarn("sx hdrs are not filled in!!");
        dxsrc = dx;
    }

/*================ Define focusing operator(s) ================*/
/* G_d = -R(ishot,-t)*/

/* select ishot from R */
//        for (k=0; k<nFoc; k++) {

/*================ Read and define mute window based on focusing operator(s) ================*/
/* G_d = p_0^+ = G_d (-t) ~ Tinv */

	if (file_tinv != NULL) {
    	mode=-1; /* apply complex conjugate to read in data */
    	readTinvData(file_tinv, xrcvsyn, xsyn, zsyn, xnxsyn, Nfoc, nxs, ntfft,
         	mode, muteW, G_d, hw, verbose);
    	/* reading data added zero's to the number of time samples to be the same as ntfft */
    	nts   = ntfft;
        /* copy G_d to DD */
        for (l = 0; l < Nfoc; l++) {
            for (i = 0; i < nxs; i++) { 
				for (j = 0; j < nts; j++) { 
					DD[l*nxs*nts+i*nts+j]  = G_d[l*nxs*nts+i*nts+j]; 
					G_d[l*nxs*nts+i*nts+j] = 0.0; 
				}
			} 
		}
        /* check consistency of header values */
        if (xrcvsyn[0] != 0 || xrcvsyn[1] != 0 ) fxsb = xrcvsyn[0];
        fxse = fxsb + (float)(nxs-1)*dxs;
        dxf = (xrcvsyn[nxs-1] - xrcvsyn[0])/(float)(nxs-1);
        if (NINT(dxs*1e3) != NINT(fabs(dxf)*1e3)) {
            vmess("dx in hdr.d1 (%.3f) and hdr.gx (%.3f) not equal",d2, dxf);
            if (dxf != 0) dxs = fabs(dxf);
            vmess("dx in operator => %f", dxs);
        }
	}
	else { /* use ishot from Refl and optionally convolve with wavelet */
        /* reading data added zero's to the number of time samples to be the same as ntfft */
        nts   = ntfft;

        l = 0;
        k = ishot;    
        scl   = 1.0/((float)ntfft);
        rtrace = (float *)calloc(ntfft,sizeof(float));
        ctrace = (complex *)calloc(ntfft,sizeof(complex));
        memset(&ctrace[0].r,0,nfreq*2*sizeof(float));
        for (i = 0; i < xnx[k]; i++) {
            for (j = nw_low, m = 0; j <= nw_high; j++, m++) {
                ctrace[j].r =  Refl[k*nw*nx+m*nx+i].r*cwave[j].r + Refl[k*nw*nx+m*nx+i].i*cwave[j].i;
                ctrace[j].i = -Refl[k*nw*nx+m*nx+i].i*cwave[j].r + Refl[k*nw*nx+m*nx+i].r*cwave[j].i;;
            }
    	    /* transfrom result back to time domain */
            cr1fft(ctrace, rtrace, ntfft, 1);
            for (j = 0; j < nts; j++) {
                DD[l*nxs*nts+i*nts+j] = -1.0*scl*rtrace[j];
            }
        }
	    free(ctrace);
	    free(rtrace);

	    fxse = fxsb = xrcv[0];
        /* check consistency of header values */
        for (k=0; k<nshots; k++) {
            for (i = 0; i < nx; i++) {
                fxsb = MIN(xrcv[k*nx+i],fxsb);
                fxse = MAX(xrcv[k*nx+i],fxse);
            }
        }
        fxse = fxsb + (float)(nxs-1)*dxs;
        dxf = dx;
	}

	/* just fill with zero's */
	for (i=0; i<nxs*Nfoc; i++) {
		tsynW[i] = 0;
	}

    /* define tapers to taper edges of acquisition */
    if (tap == 1 || tap == 3) {
        for (j = 0; j < ntap; j++)
            tapersy[j] = (cos(PI*(j-ntap)/ntap)+1)/2.0;
        for (j = ntap; j < nxs-ntap; j++)
            tapersy[j] = 1.0;
        for (j = nxs-ntap; j < nxs; j++)
            tapersy[j] =(cos(PI*(j-(nxs-ntap))/ntap)+1)/2.0;
    }
    else {
        for (j = 0; j < nxs; j++) tapersy[j] = 1.0;
    }
    if (tap == 1 || tap == 3) {
        if (verbose) vmess("Taper for operator applied ntap=%d", ntap);
        for (l = 0; l < Nfoc; l++) {
            for (i = 0; i < nxs; i++) {
                for (j = 0; j < nts; j++) {
                    DD[l*nxs*nts+i*nts+j] *= tapersy[i];
                }   
            }   
        }   
    }

/*================ Check the size of the files ================*/

    if (NINT(dxsrc/dx)*dx != NINT(dxsrc)) {
        vwarn("source (%.2f) and receiver step (%.2f) don't match",dxsrc,dx);
        if (reci == 2) vwarn("step used from operator (%.2f) ",dxs);
    }
    di = NINT(dxf/dxs);
    if ((NINT(di*dxs) != NINT(dxf)) && verbose) 
        vwarn("dx in receiver (%.2f) and operator (%.2f) don't match",dx,dxs);
    if (nt != nts) 
        vmess("Time samples in shot (%d) and focusing operator (%d) are not equal",nt, nts);
    if (verbose) {
        vmess("Number of focusing operators   = %d", Nfoc);
        vmess("Number of receivers in focusop = %d", nxs);
        vmess("number of shots                = %d", nshots);
        vmess("number of receiver/shot        = %d", nx);
        vmess("first model position           = %.2f", fxsb);
        vmess("last model position            = %.2f", fxse);
        vmess("first source position fxf      = %.2f", fxf);
        vmess("source distance dxsrc          = %.2f", dxsrc);
        vmess("last source position           = %.2f", fxf+(nshots-1)*dxsrc);
        vmess("receiver distance     dxf      = %.2f", dxf);
        vmess("direction of increasing traces = %d", di);
        vmess("number of time samples (nt,nts) = %d (%d,%d)", ntfft, nt, nts);
        vmess("time sampling                  = %e ", dt);
        if (file_rr != NULL)    vmess("RR output file                 = %s ", file_rr);
    }

/*================ initializations ================*/

    if (reci) n2out = nxs;
    else n2out = nshots;
    mem = Nfoc*n2out*ntfft*sizeof(float)/1048576.0;
    if (verbose) {
        vmess("number of output traces        = %d", n2out);
        vmess("number of output samples       = %d", ntfft);
        vmess("Size of output data/file       = %.1f MB", mem);
    }


    /* dry-run of synthesis to get all x-positions calcalated by the integration */
    synthesisPosistions(nx, nt, nxs, nts, dt, xsyn, Nfoc, xrcv, xsrc, xnx, fxse, fxsb, 
        dxs, dxsrc, dx, nshots, ixpos, &npos, isxcount, countmin, reci, verbose);
    if (verbose) {
        vmess("synthesisPosistions: nshots=%d npos=%d", nshots, npos);
    }

/*================ set variables for output data ================*/

    n1 = nts; n2 = n2out;
    f1 = ft; f2 = fxsb+dxs*ixpos[0];
    d1 = dt;
    if (reci == 0) d2 = dxsrc; // distance between sources in R
    else if (reci == 1) d2 = dxs; // distance between traces in G_d 
    else if (reci == 2) d2 = dx; // distance between receivers in R

    hdrs_out = (segy *) calloc(n2,sizeof(segy));
    if (hdrs_out == NULL) verr("allocation for hdrs_out");
    size  = nxs*nts;

    for (i = 0; i < n2; i++) {
        hdrs_out[i].ns     = n1;
        hdrs_out[i].trid   = 1;
        hdrs_out[i].dt     = dt*1000000;
        hdrs_out[i].f1     = f1;
        hdrs_out[i].f2     = f2;
        hdrs_out[i].d1     = d1;
        hdrs_out[i].d2     = d2;
        hdrs_out[i].trwf   = n2out;
        hdrs_out[i].scalco = -1000;
        hdrs_out[i].gx = NINT(1000*(f2+i*d2));
        hdrs_out[i].scalel = -1000;
        hdrs_out[i].tracl = i+1;
    }
    t1    = wallclock_time();
    tread = t1-t0;


/*================ start loop over number of time-samples ================*/

    for (ii=istart; ii<iend; ii++) {

/*================ initialization ================*/

		/* To Do copy to G_d is not needed */

        for (l = 0; l < Nfoc; l++) {
            for (i = 0; i < nxs; i++) {
                for (j = 0; j < nts; j++) {
                    G_d[l*nxs*nts+i*nts+j] = DD[l*nxs*nts+i*nts+j];
                }
                for (j = 0; j < nts-ii+shift; j++) { 
                    G_d[l*nxs*nts+i*nts+j] = 0.0;
                }
            }
            for (i = 0; i < npos; i++) {
                    j = 0;
                    ix = ixpos[i];
                    f1min[l*nxs*nts+i*nts+j] = -DD[l*nxs*nts+ix*nts+j];
                    for (j = 1; j < nts; j++) {
                       f1min[l*nxs*nts+i*nts+j] = -DD[l*nxs*nts+ix*nts+nts-j];
                  }
            }
        }
    	memcpy(Ni, G_d, Nfoc*nxs*ntfft*sizeof(float));

/*================ start Marchenko iterations ================*/

        for (iter=0; iter<niter; iter++) {

            t2    = wallclock_time();
    
/*================ construction of Ni(-t) = - \int R(x,t) Ni(t)  ================*/

            synthesis(Refl, Fop, Ni, iRN, nx, nt, nxs, nts, dt, xsyn, Nfoc, 
                xrcv, xsrc, xnx, fxse, fxsb, dxs, dxsrc, dx, ntfft, nw, nw_low, nw_high, mode,
                reci, nshots, ixpos, npos, &tfft, isxcount, reci_xsrc, reci_xrcv, ixmask, verbose);

            t3 = wallclock_time();
            tsyn +=  t3 - t2;

            for (l = 0; l < Nfoc; l++) {
                for (i = 0; i < npos; i++) {
                    j = 0;
                    ix = ixpos[i]; 
                    Ni[l*nxs*nts+i*nts+j]    = -iRN[l*nxs*nts+ix*nts+j];
                    for (j = 1; j < nts; j++) {
                        Ni[l*nxs*nts+i*nts+j]    = -iRN[l*nxs*nts+ix*nts+nts-j];
                    }
                }
            }

            /* primaries only scheme */
            if (iter % 2 == 0) { /* even iterations: => f_1^-(t) */
            	/* apply muting for the acausal part */
                for (l = 0; l < Nfoc; l++) {
                    for (i = 0; i < npos; i++) {
                        for (j = ii-shift; j < nts; j++) {
                            Ni[l*nxs*nts+i*nts+j] = 0.0;
                        }
                        for (j = 0; j < shift; j++) {
                            Ni[l*nxs*nts+i*nts+j] = 0.0;
                        }
                    }
                }
            }
            else {/* odd iterations: => f_1^+(t)  */
                for (l = 0; l < Nfoc; l++) {
                    for (i = 0; i < npos; i++) {
                        j = 0;
                        f1min[l*nxs*nts+i*nts+j] -= Ni[l*nxs*nts+i*nts+j];
                        for (j = 1; j < nts; j++) {
                            f1min[l*nxs*nts+i*nts+j] -= Ni[l*nxs*nts+i*nts+nts-j];
                        }
                    }
                }
                for (l = 0; l < Nfoc; l++) {
                    for (i = 0; i < npos; i++) {
                        for (j = nts-shift; j < nts; j++) {
                            Ni[l*nxs*nts+i*nts+j] = 0.0;
                        }
                        for (j = 0; j < nts-ii+shift; j++) {
                            Ni[l*nxs*nts+i*nts+j] = 0.0;
                        }
                    }
                }
            } /* end else (iter) branch */

            t2 = wallclock_time();
            tcopy +=  t2 - t3;
    
            if (verbose>2) vmess("*** Iteration %d finished ***", iter);
    
        } /* end of iterations */

        for (l = 0; l < Nfoc; l++) {
            for (i = 0; i < nxs; i++) {
                 RR[l*nxs*nts+i*nts+ii] = f1min[l*nxs*nts+i*nts+ii];
            }
        }

		/* To Do optional write intermediate RR results to file */

        if (verbose) {
            t3=wallclock_time();
            tii=(t3-t1)*((float)(iend-istart)/(ii-istart+1.0))-(t3-t1);
            vmess("Remaining compute time at time-sample %d = %.2f s.",ii, tii);
        }

	} /* end of time iterations ii */

    free(Ni);
    free(G_d);

    free(muteW);
    free(tsynW);

    t2 = wallclock_time();
    if (verbose) {
        vmess("Total CPU-time marchenko = %.3f", t2-t0);
        vmess("with CPU-time synthesis  = %.3f", tsyn);
        vmess("with CPU-time copy array = %.3f", tcopy);
        vmess("     CPU-time fft data   = %.3f", tfft);
        vmess("and CPU-time read data   = %.3f", tread);
    }

/*================ write output files ================*/


    fp_rr = fopen(file_rr, "w+");
    if (fp_rr==NULL) verr("error on creating output file %s", file_rr);

    tracf = 1;
    for (l = 0; l < Nfoc; l++) {
        if (reci) f2 = fxsb;
        else f2 = fxf;

        for (i = 0; i < n2; i++) {
            hdrs_out[i].fldr   = l+1;
            hdrs_out[i].sx     = NINT(xsyn[l]*1000);
            hdrs_out[i].offset = (long)NINT((f2+i*d2) - xsyn[l]);
            hdrs_out[i].tracf  = tracf++;
            hdrs_out[i].selev  = NINT(zsyn[l]*1000);
            hdrs_out[i].sdepth = NINT(-zsyn[l]*1000);
            hdrs_out[i].f1     = f1;
        }

        ret = writeData(fp_rr, (float *)&RR[l*size], hdrs_out, n1, n2);
        if (ret < 0 ) verr("error on writing output file.");
    }
    ret = fclose(fp_rr);
    if (ret < 0) verr("err %d on closing output file",ret);

    if (verbose) {
        t1 = wallclock_time();
        vmess("and CPU-time write data  = %.3f", t1-t2);
    }

/*================ free memory ================*/

    free(hdrs_out);
    free(tapersy);

    exit(0);
}


/*================ Convolution and Integration ================*/

void synthesis(complex *Refl, complex *Fop, float *Top, float *iRN, int nx, int nt, int nxs, int nts, float dt, float *xsyn, int
Nfoc, float *xrcv, float *xsrc, int *xnx, float fxse, float fxsb, float dxs, float dxsrc, float dx, int ntfft, int
nw, int nw_low, int nw_high,  int mode, int reci, int nshots, int *ixpos, int npos, double *tfft, int *isxcount, int
*reci_xsrc,  int *reci_xrcv, float *ixmask, int verbose)
{
    int     nfreq, size, inx;
    float   scl;
    int     i, j, l, m, iw, ix, k, ixsrc, il, ik;
    float   *rtrace, idxs;
    complex *sum, *ctrace;
    int     npe;
    static int first=1, *ixrcv;
    static double t0, t1, t;

    size  = nxs*nts;
    nfreq = ntfft/2+1;
    /* scale factor 1/N for backward FFT,
     * scale dt for correlation/convolution along time, 
     * scale dx (or dxsrc) for integration over receiver (or shot) coordinates */
    scl   = 1.0*dt/((float)ntfft);

#ifdef _OPENMP
    npe   = omp_get_max_threads();
    /* parallelisation is over number of virtual source positions (Nfoc) */
    if (npe > nshots) {
        vmess("Number of OpenMP threads set to %d (was %d)", Nfoc, npe);
        omp_set_num_threads(Nfoc);
    }
#endif

    t0 = wallclock_time();

    /* reset output data to zero */
    memset(&iRN[0], 0, Nfoc*nxs*nts*sizeof(float));
    ctrace = (complex *)calloc(ntfft,sizeof(complex));

    if (!first) {
    /* transform muted Ni (Top) to frequency domain, input for next iteration  */
        for (l = 0; l < Nfoc; l++) {
            /* set Fop to zero, so new operator can be defined within ixpos points */
            memset(&Fop[l*nxs*nw].r, 0, nxs*nw*2*sizeof(float));
            for (i = 0; i < npos; i++) {
                   rc1fft(&Top[l*size+i*nts],ctrace,ntfft,-1);
                   ix = ixpos[i];
                   for (iw=0; iw<nw; iw++) {
                       Fop[l*nxs*nw+iw*nxs+ix].r = ctrace[nw_low+iw].r;
                       Fop[l*nxs*nw+iw*nxs+ix].i = mode*ctrace[nw_low+iw].i;
                   }
            }
        }
    }
    else { /* only for first call to synthesis using all nxs traces in G_d */
    /* transform G_d to frequency domain, over all nxs traces */
        first=0;
        for (l = 0; l < Nfoc; l++) {
            /* set Fop to zero, so new operator can be defined within all ix points */
            memset(&Fop[l*nxs*nw].r, 0, nxs*nw*2*sizeof(float));
            for (i = 0; i < nxs; i++) {
                   rc1fft(&Top[l*size+i*nts],ctrace,ntfft,-1);
                   for (iw=0; iw<nw; iw++) {
                       Fop[l*nxs*nw+iw*nxs+i].r = ctrace[nw_low+iw].r;
                       Fop[l*nxs*nw+iw*nxs+i].i = mode*ctrace[nw_low+iw].i;
                   }
            }
        }
        idxs = 1.0/dxs;
        ixrcv = (int *)malloc(nshots*nx*sizeof(int));
        for (k=0; k<nshots; k++) {
            for (i = 0; i < nx; i++) {
                ixrcv[k*nx+i] = NINT((xrcv[k*nx+i]-fxsb)*idxs);
            }
        }
    }
    free(ctrace);
    t1 = wallclock_time();
    *tfft += t1 - t0;

/* Loop over total number of shots */
    if (reci == 0 || reci == 1) {
#pragma omp parallel default(none) \
 shared(iRN, dx, npe, nw, verbose, nshots, xnx) \
 shared(Refl, Nfoc, reci, xrcv, xsrc, xsyn, fxsb, fxse, nxs, dxs) \
 shared(nx, dxsrc, nfreq, nw_low, nw_high) \
 shared(Fop, size, nts, ntfft, scl, ixrcv) \
 private(l, ix, j, m, i, sum, rtrace, k, ixsrc, inx)
{ /* start of parallel region */
        sum   = (complex *)malloc(nfreq*sizeof(complex));
        rtrace = (float *)calloc(ntfft,sizeof(float));

#pragma omp for schedule(guided,1)
        for (k=0; k<nshots; k++) {
            if ((xsrc[k] < 0.999*fxsb) || (xsrc[k] > 1.001*fxse)) continue;
            ixsrc = NINT((xsrc[k] - fxsb)/dxs);
            inx = xnx[k]; /* number of traces per shot */

/*================ SYNTHESIS ================*/

            for (l = 0; l < Nfoc; l++) {
		        /* compute integral over receiver positions */
                /* multiply R with Fop and sum over nx */
                memset(&sum[0].r,0,nfreq*2*sizeof(float));
                for (j = nw_low, m = 0; j <= nw_high; j++, m++) {
                    for (i = 0; i < inx; i++) {
                        ix = ixrcv[k*nx+i];
                        sum[j].r += Refl[k*nw*nx+m*nx+i].r*Fop[l*nw*nxs+m*nxs+ix].r -
                                    Refl[k*nw*nx+m*nx+i].i*Fop[l*nw*nxs+m*nxs+ix].i;
                        sum[j].i += Refl[k*nw*nx+m*nx+i].i*Fop[l*nw*nxs+m*nxs+ix].r +
                                    Refl[k*nw*nx+m*nx+i].r*Fop[l*nw*nxs+m*nxs+ix].i;
                    }
                }

                /* transfrom result back to time domain */
                cr1fft(sum, rtrace, ntfft, 1);

                /* place result at source position ixsrc; dx = receiver distance */
                for (j = 0; j < nts; j++) 
                    iRN[l*size+ixsrc*nts+j] += rtrace[j]*scl*dx;
            
            } /* end of parallel Nfoc loop */


        if (verbose>4) vmess("*** Shot gather %d processed ***", k);

        } /* end of nshots (k) loop */

        free(sum);
        free(rtrace);
#ifdef _OPENMP
#pragma omp single 
            npe   = omp_get_num_threads();
#endif
} /* end of parallel region */



    }     /* end of if reci */

/* if reciprocal traces are enabled start a new loop over reciprocal shot positions */
    if (reci != 0) {
        for (k=0; k<nxs; k++) {
            if (isxcount[k] == 0) continue;
            ixsrc = k;
            inx = isxcount[ixsrc]; /* number of traces per reciprocal shot */

#pragma omp parallel default(none) \
 shared(iRN, dx, nw, verbose) \
 shared(Refl, Nfoc, reci, xrcv, xsrc, xsyn, fxsb, fxse, nxs, dxs) \
 shared(nx, dxsrc, inx, k, nfreq, nw_low, nw_high) \
 shared(reci_xrcv, reci_xsrc, ixmask) \
 shared(Fop, size, nts, ntfft, scl, ixrcv, ixsrc) \
 private(l, ix, j, m, i, sum, rtrace, ik, il)
{ /* start of parallel region */
            sum   = (complex *)malloc(nfreq*sizeof(complex));
            rtrace = (float *)calloc(ntfft,sizeof(float));

#pragma omp for schedule(guided,1)
            for (l = 0; l < Nfoc; l++) {

		        /* compute integral over (reciprocal) source positions */
                /* multiply R with Fop and sum over nx */
                memset(&sum[0].r,0,nfreq*2*sizeof(float));
                for (j = nw_low, m = 0; j <= nw_high; j++, m++) {
                    for (i = 0; i < inx; i++) {
                        il = reci_xrcv[ixsrc*nxs+i];
                        ik = reci_xsrc[ixsrc*nxs+i];
            			ix = NINT((xsrc[il] - fxsb)/dxs);
                        sum[j].r += Refl[il*nw*nx+m*nx+ik].r*Fop[l*nw*nxs+m*nxs+ix].r -
                                    Refl[il*nw*nx+m*nx+ik].i*Fop[l*nw*nxs+m*nxs+ix].i;
                        sum[j].i += Refl[il*nw*nx+m*nx+ik].i*Fop[l*nw*nxs+m*nxs+ix].r +
                                    Refl[il*nw*nx+m*nx+ik].r*Fop[l*nw*nxs+m*nxs+ix].i;
                    }
                }

                /* transfrom result back to time domain */
                cr1fft(sum, rtrace, ntfft, 1);

                /* place result at source position ixsrc; dxsrc = shot distance */
                for (j = 0; j < nts; j++) 
                    iRN[l*size+ixsrc*nts+j] = ixmask[ixsrc]*(iRN[l*size+ixsrc*nts+j]+rtrace[j]*scl*dxsrc);
                
            } /* end of parallel Nfoc loop */
        
            free(sum);
            free(rtrace);
        
 } /* end of parallel region */

        } /* end of reciprocal shots (k) loop */
	} /* end of if reci */

    t = wallclock_time() - t0;
    if (verbose>3) {
        vmess("OMP: parallel region = %f seconds (%d threads)", t, npe);
    }

    return;
}

void synthesisPosistions(int nx, int nt, int nxs, int nts, float dt, float *xsyn, int Nfoc, float *xrcv, float *xsrc, int *xnx,
float fxse, float fxsb, float dxs, float dxsrc, float dx, int nshots, int *ixpos, int *npos, int *isxcount, int countmin, int reci, int verbose)
{
    int     i, j, l, ixsrc, ixrcv, dosrc, k, *count;
    float   x0, x1;

    count   = (int *)calloc(nxs,sizeof(int)); // number of traces that contribute to the integration over x

/*================ SYNTHESIS ================*/

    for (l = 0; l < 1; l++) { /* assuming all focal operators cover the same lateral area */
//    for (l = 0; l < Nfoc; l++) {
        *npos=0;

        if (reci == 0 || reci == 1) {
            for (k=0; k<nshots; k++) {

                ixsrc = NINT((xsrc[k] - fxsb)/dxs);
                if (verbose>=3) {
                    vmess("source position:     %.2f in operator %d", xsrc[k], ixsrc);
                    vmess("receiver positions:  %.2f <--> %.2f", xrcv[k*nx+0], xrcv[k*nx+nx-1]);
                    vmess("focal point positions:  %.2f <--> %.2f", fxsb, fxse);
                }
        
                if ((NINT(xsrc[k]-fxse) > 0) || (NINT(xrcv[k*nx+nx-1]-fxse) > 0) ||
                    (NINT(xrcv[k*nx+nx-1]-fxsb) < 0) || (NINT(xsrc[k]-fxsb) < 0) || 
                    (NINT(xrcv[k*nx+0]-fxsb) < 0) || (NINT(xrcv[k*nx+0]-fxse) > 0) ) {
                    vwarn("source/receiver positions are outside synthesis aperture");
                    vmess("xsrc = %.2f xrcv_1 = %.2f xrvc_N = %.2f", xsrc[k], xrcv[k*nx+0], xrcv[k*nx+nx-1]);
                    vmess("source position:     %.2f in operator %d", xsrc[k], ixsrc);
                    vmess("receiver positions:  %.2f <--> %.2f", xrcv[k*nx+0], xrcv[k*nx+nx-1]);
                    vmess("focal point positions:  %.2f <--> %.2f", fxsb, fxse);
                }
        
                if ( (xsrc[k] >= 0.999*fxsb) && (xsrc[k] <= 1.001*fxse) ) {
				    j = linearsearch(ixpos, *npos, ixsrc);
				    if (j < *npos) { /* the position (at j) is already included */
					    count[j] += xnx[k];
				    }
				    else { /* add new postion */
            		    ixpos[*npos]=ixsrc;
					    count[*npos] += xnx[k];
                   	    *npos += 1;
				    }
    //                vmess("source position %d is inside synthesis model %f *npos=%d count=%d", k, xsrc[k], *npos, count[*npos]);
			    }
    
    	    } /* end of nshots (k) loop */
   	    } /* end of reci branch */

        /* if reci=1 or reci=2 source-receive reciprocity is used and new (reciprocal-)sources are added */
        if (reci != 0) {
            for (k=0; k<nxs; k++) { /* check count in total number of shots added by reciprocity */
                if (isxcount[k] >= countmin) {
				    j = linearsearch(ixpos, *npos, k);
				    if (j < *npos) { /* the position (at j) is already included */
					    count[j] += isxcount[k];
				    }
				    else { /* add new postion */
            		    ixpos[*npos]=k;
					    count[*npos] += isxcount[k];
                   	    *npos += 1;
				    }
                }
                else {
                    isxcount[k] = 0;
                }
            }
   	    } /* end of reci branch */
    } /* end of Nfoc loop */

    if (verbose>=4) {
	    for (j=0; j < *npos; j++) { 
            vmess("ixpos[%d] = %d count=%d", j, ixpos[j], count[j]);
		}
    }
    free(count);

/* sort ixpos into increasing values */
    qsort(ixpos, *npos, sizeof(int), compareInt);


    return;
}

int linearsearch(int *array, size_t N, int value)
{
	int j;
/* Check is position is already in array */
    j = 0;
    while (j < N && value != array[j]) {
        j++;
    }
	return j;
}

/*
void update(float *field, float *term, int Nfoc, int nx, int nt, int reverse, int ixpos)
{
    int   i, j, l, ix;

    if (reverse) {
        for (l = 0; l < Nfoc; l++) {
            for (i = 0; i < npos; i++) {
                j = 0;
                Ni[l*nxs*nts+i*nts+j] = -iRN[l*nxs*nts+i*nts+j];
                for (j = 1; j < nts; j++) {
                    Ni[l*nxs*nts+i*nts+j] = -iRN[l*nxs*nts+i*nts+nts-j];
                }
            }
        }
    }
    else {
        for (l = 0; l < Nfoc; l++) {
            for (i = 0; i < npos; i++) {
                j = 0;
                Ni[l*nxs*nts+i*nts+j] = -iRN[l*nxs*nts+i*nts+j];
                for (j = 1; j < nts; j++) {
                    Ni[l*nxs*nts+i*nts+j] = -iRN[l*nxs*nts+i*nts+nts-j];
                }
            }
        }
    }
    return;
}
*/
