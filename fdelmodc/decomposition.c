/*
 *  decomposition.c
 *
 *  Kees Wapenaar "Reciprocity properties of one-way propagators"
 *   GEOPHYSICS, VOL. 63, NO. 4 (JULY-AUGUST 1998); P. 1795–1798
 *
 *  Created by Jan Thorbecke on 17/03/2014.
 *  Copyright 2014 TU Delft. All rights reserved.
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define NINT(x) ((int)((x)>0.0?(x)+0.5:(x)-0.5))

#ifndef COMPLEX
typedef struct _complexStruct { /* complex number */
    float r,i;
} complex;
typedef struct _dcomplexStruct { /* complex number */
    double r,i;
} dcomplex;
#endif/* complex */

complex firoot(float x, float stab);
complex ciroot(complex x, float stab);
complex cwp_csqrt(complex z);

void decud(float om, float rho, float cp, float dx, int nkx, float fangle, float alpha, float eps, complex *pu);
int writesufile(char *filename, float *data, int n1, int n2, float f1, float f2, float d1, float d2);

void kxwfilter(complex *data, float k, float dx, int nkx, 
			   float alfa1, float alfa2, float perc);

void kxwdecomp(complex *rp, complex *rvz, complex *up, complex *down,
               int nkx, float dx, int nt, float dt, float fmin, float fmax,
               float cp, float rho, int verbose)
{
	int      iom, iomin, iomax, ikx, nfreq, a, av;
	float    omin, omax, deltom, om, kp, df, dkx;
	float    alpha, eps, *angle, *mav, avrp, avrvz, maxrp, maxrvz, fangle;
	complex  *pu, w;
	complex  ax, az;
	
	df     = 1.0/((float)nt*dt);
	dkx    = 2.0*M_PI/(nkx*dx);
	deltom = 2.*M_PI*df;
	omin   = 2.*M_PI*fmin;
	omax   = 2.*M_PI*fmax;
	nfreq  = nt/2+1;
	eps    = 0.01;
    alpha  = 0.1;
	
	iomin  = (int)MIN((omin/deltom), (nfreq-1));
	iomin  = MAX(iomin, 1);
	iomax  = MIN((int)(omax/deltom), (nfreq-1));

	pu     = (complex *)malloc(nkx*sizeof(complex));
	angle  = (float *)calloc(2*90,sizeof(float));
	mav  = (float *)calloc(2*90,sizeof(float));

	/* estimate maximum propagation angle in wavefields P and Vz */
	for (a=1; a<90; a++) {
		for (iom = iomin; iom <= iomax; iom++) {
			om  = iom*deltom;
			ikx = MIN(NINT( ((om/cp)*sin(a*M_PI/180.0))/dkx ), nkx/2);
			if (ikx < nkx/2 && ikx != 0) {
				ax.r = rp[iom*nkx+ikx].r + rp[iom*nkx+nkx-1-ikx].r;
				ax.i = rp[iom*nkx+ikx].i + rp[iom*nkx+nkx-1-ikx].i;
				angle[a] += sqrt(ax.r*ax.r + ax.i*ax.i);
				ax.r = rvz[iom*nkx+ikx].r + rvz[iom*nkx+nkx-1-ikx].r;
				ax.i = rvz[iom*nkx+ikx].i + rvz[iom*nkx+nkx-1-ikx].i;
				angle[90+a] += sqrt(ax.r*ax.r + ax.i*ax.i);
			}
		}
	}

	avrp  =0.0;
	avrvz =0.0;
	maxrp =0.0;
	maxrvz=0.0;
	for (a=1; a<90; a++) {
		avrp += angle[a];
		maxrp = MAX(angle[a], maxrp);
		avrvz += angle[90+a];
		maxrvz = MAX(angle[90+a], maxrvz);
	}
	avrp  = avrp/89.0;
	avrvz = avrvz/89.0;
	if (verbose>=4)  {
		writesufile("anglerp.su", angle, 90, 1, 0, 0, 1, 1);
		writesufile("anglervz.su", &angle[90], 90, 1, 0, 0, 1, 1);
	}
	for (av=0; av<90; av++) {
		 if (angle[av] < avrp) angle[av] = 0.0;
		 if (angle[90+av] < avrvz) angle[90+av] = 0.0;
	}
	if (verbose>=4)  {
		writesufile("anglerp0.su", angle, 90, 1, 0, 0, 1, 1);
		writesufile("anglervz0.su", &angle[90], 90, 1, 0, 0, 1, 1);
	}
	av=89;
	while (angle[av] == 0.0 && av > 0 ) av--;
	fangle = 1.0*av;
	if (verbose>=2) vmess("Up-down going: P max=%e average=%e => angle at average %f", maxrp, avrp, fangle);
//	av=179;
//	while (angle[av] == 0.0 && av > 91) av--;
//	fprintf(stderr,"vz max=%e average=%e => angle at average %f \n", maxrvz, avrvz, 1.0*(av-90));

	for (iom = iomin; iom <= iomax; iom++) {
		om  = iom*deltom;

		decud(om, rho, cp, dx, nkx, fangle, alpha, eps, pu);
/*
		kxwfilter(dpux, kp, dx, nkx, alfa1, alfa2, perc); 
		kxwfilter(dpuz, kp, dx, nkx, alfa1, alfa2, perc); 
*/
		for (ikx = 0; ikx < nkx; ikx++) {
			ax.r = 0.5*rp[iom*nkx+ikx].r;
			ax.i = 0.5*rp[iom*nkx+ikx].i;
			az.r = 0.5*(rvz[iom*nkx+ikx].r*pu[ikx].r - rvz[iom*nkx+ikx].i*pu[ikx].i);
			az.i = 0.5*(rvz[iom*nkx+ikx].i*pu[ikx].r + rvz[iom*nkx+ikx].r*pu[ikx].i);

			down[iom*nkx+ikx].r = ax.r + az.r;
			down[iom*nkx+ikx].i = ax.i + az.i;
			up[iom*nkx+ikx].r   = ax.r - az.r;
			up[iom*nkx+ikx].i   = ax.i - az.i;
		}

	}

	free(pu);

	return;
}

void decud(float om, float rho, float cp, float dx, int nkx, float fangle, float alpha, float eps, complex *pu)
{
	int 	 ikx, ikxmax1, ikxmax2, filterpoints, filterppos;
	float 	 mu, kp, kp2, ks, ks2, ksk;
	float 	 kx, kx2, kzp2, kzs2, dkx, stab;
	float 	kxfmax, kxnyq, kpos, kneg, kfilt, perc, band, *filter;
	complex kzp, kzs, cste, ckp, ckp2, ckzp2;
	
/* with complex frequency
	wom.r=om; 
	wom.i=alpha;

	ckp.r  = wom.r/cp;
 	ckp.i  = wom.i/cp;
 	ckp2.r = ckp.r*ckp.r-ckp.i*ckp.i;
 	ckp2.i = 2.0*ckp.r*ckp.i;
 	stab  = eps*eps*(ckp.r*ckp.r+ckp.i*ckp.i);
*/

	kp  = om/cp;
	kp2 = kp*kp;
	dkx = 2.0*M_PI/(nkx*dx);
 	stab  = eps*eps*kp*kp;

	/* make kw filter at maximum angle alfa */
	perc = 0.15; /* percentage of band to use for smooth filter */
	filter = (float *)malloc(nkx*sizeof(float));
	kpos = kp*sin(M_PI*fangle/180.0);
	kneg = -kpos;
	kxnyq  = M_PI/dx;
	if (kpos > kxnyq)  kpos = kxnyq;
	band = kpos;
	filterpoints = (int)fabs((int)(perc*band/dkx));
	kfilt = fabs(dkx*filterpoints);
	if (kpos+kfilt < kxnyq) {
		kxfmax = kpos+kfilt;
		filterppos = filterpoints;
	}
	else {
		kxfmax = kxnyq;
		filterppos = (int)(0.15*nkx/2);
	}
	ikxmax1 = (int) (kxfmax/dkx);
	ikxmax2 = ikxmax1 - filterppos;
	// fprintf(stderr,"ikxmax1=%d ikxmax2=%d nkp=%d nkx=%d\n", ikxmax1, ikxmax2, (int)(kp/dkx), nkx);

	for (ikx = 0; ikx < ikxmax2; ikx++) 
		filter[ikx]=1.0;
	for (ikx = ikxmax2; ikx < ikxmax1; ikx++)
		filter[ikx] =(cos(M_PI*(ikx-ikxmax2)/(ikxmax1-ikxmax2))+1)/2.0;
	for (ikx = ikxmax1; ikx <= nkx/2; ikx++)
		filter[ikx] = 0.0;
	/* end of kxfilter */

	for (ikx = 0; ikx <= (nkx/2); ikx++) {
		kx   = ikx*dkx;
		kx2  = kx*kx;
		kzp2 = kp2 - kx2;
		kzp  = firoot(kzp2, stab);

/* with complex frequency 
 		kzp2.r = kp2.r - kx2;
 		kzp2.i = kp2.i;
 		kzp  = ciroot(kzp2, stab);
*/
		if (kzp2 != 0) {
			pu[ikx].r = filter[ikx]*om*rho*kzp.r;
			pu[ikx].i = filter[ikx]*om*rho*kzp.i;
//			pu[ikx].r = om*rho*kzp.r;
//			pu[ikx].i = om*rho*kzp.i;
		}
		else {
			pu[ikx].r = 0.0;
			pu[ikx].i = 0.0;
		}
	}
	
/* operators are symmetric in kx-w domain */
	for (ikx = (nkx/2+1); ikx < nkx; ikx++) {
		pu[ikx] = pu[nkx-ikx];
	}
	free(filter);
	
	return;
}

/* compute 1/x */
complex firoot(float x, float stab)
{
    complex z;
    if (x > 0.0) {
        z.r = 1.0/sqrt(x+stab);
        z.i = 0.0;
    }
    else if (x == 0.0) {
        z.r = 0.0;
        z.i = 0.0;
    }
    else {
        z.r = 0.0;
        z.i = 1.0/sqrt(-x+stab);
    }
    return z;
}

complex ciroot(complex x, float stab)
{
    complex z, kz, kzz;
	float kd;

    if (x.r == 0.0) {
        z.r = 0.0;
        z.i = 0.0;
    }
    else {
		kzz = cwp_csqrt(x);
 		kz.r = kzz.r;
 		kz.i = -abs(kzz.i);
 		kd = kz.r*kz.r+kz.i*kz.i+stab;
        z.r = kz.r/kd;
        z.i = -kz.i/kd;
 	}
     return z;
}
 
complex cwp_csqrt(complex z)
{
   complex c;
    float x,y,w,r;
    if (z.r==0.0 && z.i==0.0) {
		c.r = c.i = 0.0;
        return c;
    } else {
        x = fabs(z.r);
        y = fabs(z.i);
        if (x>=y) {
            r = y/x;
            w = sqrt(x)*sqrt(0.5*(1.0+sqrt(1.0+r*r)));
        } else {
            r = x/y;
            w = sqrt(y)*sqrt(0.5*(r+sqrt(1.0+r*r)));
        }
        if (z.r>=0.0) {
            c.r = w;
            c.i = z.i/(2.0*w);
        } else {
            c.i = (z.i>=0.0) ? w : -w;
            c.r = z.i/(2.0*c.i);
        }
        return c;
    }
}

