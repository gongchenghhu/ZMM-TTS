/*                                                          15.Feb.2010 v1.3 */
/*=============================================================================

                          U    U   GGG    SSSS  TTTTT
                          U    U  G       S       T
                          U    U  G  GG   SSSS    T
                          U    U  G   G       S   T
                           UUU     GG     SSS     T

                   ========================================
                    ITU-T - USER'S GROUP ON SOFTWARE TOOLS
                   ========================================

       =============================================================
       COPYRIGHT NOTE: This source code, and all of its derivations,
       is subject to the "ITU-T General Public License". Please have
       it  read  in    the  distribution  disk,   or  in  the  ITU-T
       Recommendation G.191 on "SOFTWARE TOOLS FOR SPEECH AND  AUDIO
       CODING STANDARDS".
       =============================================================

  DESCRIPTION : 
	This file contains a demonstration program of a frequency response tool, using
  the functions in fft.h

  HISTORY :
	31.Mar.05	v1.0	First Beta version
	10.Mar.08 v1.1  New option:
	                -ov   : overlap between consecutive frames for decreasing the window effect.
	   Sep.08 v1.2  Use of split radix(4,2) fft algorithm instead of DFT
                  New option:
                  -nfft : indicates the number of points used in FFT.
  15.Feb.10 v1.3  Modified maximum string length for filename, and
	                removed some macros (OVERLAP, VAR_NFFT)

  AUTHORS :
	Cyril Guillaume & Stephane Ragot -- stephane.ragot@francetelecom.com
	Pierre Berthet (v1.1) for France Telecom
	Deming Zhang (v1.2) for Huawei Technologies
  yusuke hiwasaki (v1.3) NTT
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "fft.h"
#include "export.h"
#include "bmp_utils.h"

/* UGST modules */
#include "ugstdemo.h"
#include "ugst-utl.h"

#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif

static int is_little_endian()
{
  /* Hex version of the string ABCD */
  unsigned long tmp = 0x41424344; 
  
  /* compare the hex version of the 4 characters with the ascii version */
  return(strncmp("DCBA",(char *)&tmp,4));
}

#define P(x) printf x
static void display_usage()
{
  P(("FREQRESP.C - Version 1.3 of 15.Feb.2010 \n\n"));
 
  P((" Frequency response measure program\n"));
  P((" This program computes the average power spectrum \n"));
  P((" of two files (input and output of a codec).\n"));
  P(("\n"));
  P((" Usage:\n"));
  P((" $ freqresp   [-options] FileInpCodec FileOutCodec ASCIIout\n"));
  P((" where:\n"));
  P(("  FileInpCodec   is the input file of the codec;\n"));
  P(("  FileOutCodec   is the output file of the codec;\n"));
  P(("  ASCIIout       is an ASCII file containing the average power spectrum\n"));
  P(("                 of FileInpCodec and FileOutCodec;\n"));
  P(("\n"));
  P((" Options:\n"));
  P(("  -fs  fs ....... fs is the sampling frequency of the input files (default is\n"));
  P(("                  16000Hz);\n"));
  P(("  -bmp bmpFile... bmpFile is a bitmap file containing the average power\n"));
  P(("                  spectrum graphic representation;\n"));
  P(("  -fmin  fmin ... fmin is the minimum frequency to display in bitmap (default\n"));
  P(("                  is 0Hz);\n"));
  P(("  -fmax  fmax ... fmax is the maximum frequency to display in bitmap (default\n"));
  P(("                  is 8000Hz);\n"));
  P(("  -fstep fstep .. fstep is the frequency step to display in bitmap (default\n"));
  P(("                  is 1000Hz);\n"));
  P(("  -pmin  pmin ... pmin is the minimum power (dB) to display in bitmap (default\n"));
  P(("                  is -80dB);\n"));
  P(("  -pmax  pmax ... pmax is the maximumpower (dB) to display in bitmap (default\n"));
  P(("                  is 0dB);\n"));
  P(("  -pstep pstep .. pstep is the power step (dB) to display in bitmap (default\n"));
  P(("                  is 10dB);\n"));
  P(("  -ov    ov ..... ov is the overlap (%c) between two consecutive frames for\n", '%'));
  P(("                  computing the average power spectrum (default is 0%c);\n", '%'));
  P(("  -nfft  nfft ... nfft is the number of samples in each FFT (default is 2048).\n"));
  P(("\n"));
}
#undef P

int main(argc, argv)
int argc;
char *argv[];
{

	/* .... DECLARATIONS ..... */
	/* buffers */
	float frame[NFFT_MAX];			/* Frame of the input signal (float format) */
	short frame_sh[NFFT_MAX];		/* Frame of the input signal (short format) */
	float hanning[NFFT_MAX];		/* hanning window */
	float powSp[NFFT_MAX];			/* Power spectrum of a frame */
	float avg1PowSp[NFFT_MAX/2];	/* Average Power spectrum vector for the first input file */
	float avg2PowSp[NFFT_MAX/2];	/* Average Power spectrum vector for the second input file */

	/* file variables */
	FILE* fp;						/* file pointer */
	char  in1FileName[MAX_STRLEN]; /* name of the first input file (input of the codec) */
	char  in2FileName[MAX_STRLEN]; /* name of the second input file (output of the codec) */
	char  asciiFileName[MAX_STRLEN]; /* name of the output ASCII file */
	char  bmpFileName[MAX_STRLEN]; /* name of the output bitmap file */


	/* algorithm variables */
	int nfft = 2048;
	long fs=16000;					/* sampling frequency */
	int little_endian;				/* flag =1 if little-endian, else =0 */
	int i, j;
	int nbread;
	long nbFrame=0;
	int bmp_mode=0;
	int border=40;
	int im_wdth=nfft/2+border;
	int im_hght=(int) ((nfft/2+border)/1.25);
	int xstep, ystep;
	long fstep=1000;
	float pstep=10;
	float pmax=0, pmin=-80;
	long fmax=fs/2, fmin=0;
	char* image;
	float ov=0;
	int nb_samples_ov=0;



	/* ......... GET PARAMETERS ......... */

	/* Check options */
	if (argc < 4)
		display_usage();
	else
	{
		while (argc > 1 && argv[1][0] == '-')
			if (strcmp(argv[1],"-fs")==0)
			{
				/* Set the sampling frequency parameter */
				fs = atol(argv[2]);

				/* Move arg{c,v} over the option to the next argument */
				argc -=2;
				argv +=2;
			}
			else if (strcmp(argv[1],"-bmp")==0)
			{
				/* Set the name of the bitmap file */
				if (strlen(argv[2])<MAX_STRLEN) {
					strcpy(bmpFileName,argv[2]);
					bmp_mode=1;
				}
        else {
          fprintf(stderr, "Filename argument too long (%s)\n", argv[2]);
					exit(2);
        }

				/* Move arg{c,v} over the option to the next argument */
				argc -=2;
				argv +=2;
			}
			else if (strcmp(argv[1],"-fmax")==0)
			{
				/* Set the name of the bitmap file */
				fmax = atol(argv[2]);
				bmp_mode=1;

				/* Move arg{c,v} over the option to the next argument */
				argc -=2;
				argv +=2;
			}
			else if (strcmp(argv[1],"-fmin")==0)
			{
				/* Set the name of the bitmap file */
				fmin = atol(argv[2]);
				bmp_mode=1;

				/* Move arg{c,v} over the option to the next argument */
				argc -=2;
				argv +=2;
			}
			else if (strcmp(argv[1],"-pmax")==0)
			{
				/* Set the name of the bitmap file */
				pmax = (float) atof(argv[2]);
				bmp_mode=1;

				/* Move arg{c,v} over the option to the next argument */
				argc -=2;
				argv +=2;
			}
			else if (strcmp(argv[1],"-pmin")==0)
			{
				/* Set the name of the bitmap file */
				pmin = (float) atof(argv[2]);
				bmp_mode=1;

				/* Move arg{c,v} over the option to the next argument */
				argc -=2;
				argv +=2;
			}
			else if (strcmp(argv[1],"-pstep")==0)
			{
				/* Set the name of the bitmap file */
				pstep = (float) atof(argv[2]);
				bmp_mode=1;

				/* Move arg{c,v} over the option to the next argument */
				argc -=2;
				argv +=2;
			}
			else if (strcmp(argv[1],"-fstep")==0)
			{
				/* Set the name of the bitmap file */
				fstep = atol(argv[2]);
				bmp_mode=1;

				/* Move arg{c,v} over the option to the next argument */
				argc -=2;
				argv +=2;
			}
			else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-?") == 0)
			{
				/* Display help message */
				display_usage();
				exit(2);
			}
			else if (strcmp(argv[1],"-ov")==0)
			{
				/* Get the overlap percentage */
				ov = (float) atof(argv[2]);
				if ((ov < 0) || (ov >= 1))
				{
					fprintf(stderr, "ERROR! Bad overlap parameter.\n\n");
					exit(-1);
				}
				else
				{
					nb_samples_ov = (int) (ov * nfft);
				}

				/* Move arg{c,v} over the option to the next argument */
				argc -=2;
				argv +=2;
			}
			else if (strcmp(argv[1],"-nfft")==0)
			{
				/* Get the number of samples involved in each FFT */
				nfft = (int) atof(argv[2]);
				if ((nfft < 16) || (nfft > NFFT_MAX)
                    || (nfft!=16 && nfft!=32 && nfft!=64 && nfft!=128 && nfft!=256 && nfft!=512 && nfft!=1024 && nfft!=2048 && nfft!=4096 && nfft!=8192))
				{
					fprintf(stderr, "ERROR! Bad nfft parameter (must be a power of 2 inferior to %d).\n\n", NFFT_MAX);
					exit(-1);
				}
				/* 1064x851 was the original size (with nfft=2048), for lower nfft values, it is better
				   to keep these dimensions for better visual quality. */
				im_wdth = max(nfft/2+border , 1064);
				im_hght = (int)(max((nfft/2+border)/1.25 , 851));
				nb_samples_ov = (int) (ov * nfft);
				/* Move arg{c,v} over the option to the next argument */
				argc -=2;
				argv +=2;
			}
			else
			{
				fprintf(stderr, "ERROR! Invalid option \"%s\" in command line\n\n",argv[1]);
				display_usage();
				exit(-1);
			}
	}

	/* Read parameters for processing */
	GET_PAR_S(1, "_Input File of the codec :......... ", in1FileName);
	GET_PAR_S(2, "_Output File of the codec : ....... ", in2FileName);
	GET_PAR_S(3, "_Output ASCII File: ............... ", asciiFileName);



	/* ..... INITIALIZATIONS ..... */
	/* initialize the average power spectrum vector */
	for(i=0; i<nfft/2; i++) {
		avg1PowSp[i]=0;
		avg2PowSp[i]=0;
	}

	/* generate a hanning window with nfft coefficients */
	genHanning(nfft, hanning);


	/* ..... PROCESSING ..... */

	/* ..... First File ..... */
	/* open first input file */
	fp=fopen(in1FileName,"rb");
	if (fp == NULL) {
		fprintf(stderr,"Error: Can't open input file %s", in1FileName);
		exit(-1);
	}

	/* loop over first input file */
	while( (nbread = fread(frame_sh,sizeof(short),nfft,fp)) == nfft) {
		/* increment the number of processed frames */
		nbFrame++;

		/* convert short format input, into 16 bit float */
		sh2fl(nfft, frame_sh, frame, 16, 1);

		/* Hanning Windowing */
		for(i=0; i<nfft; i++) {
			frame[i]=frame[i]*hanning[i];
		}

#ifndef TUNED_FFT
		/* Real Discret Fourier Transform */
		rdft(nfft,frame,real,imag);
		/* Power spectrum computation */
		powSpect(real, imag, powSp, nfft);
#else
		powSpect(nfft,frame, powSp);
#endif

		/* average power spectrum computation */
		for(i=0; i<nfft/2; i++) {
			avg1PowSp[i] = avg1PowSp[i] + (powSp[i]-avg1PowSp[i])/nbFrame;
		}

		/* For overlapping, reposition the file pointer nb_samples_ov samples before its current position */
		fseek ( fp , -nb_samples_ov*2 , SEEK_CUR );
	}
	/* close input file */
	fclose(fp);


	/* ..... Second File ..... */

	/* open second input file */
	fp=fopen(in2FileName,"rb");
	if (fp == NULL) {
		fprintf(stderr,"Error: Can't open input file %s", in2FileName);
		exit(-1);
	}

	nbFrame=0;
	/* loop over first input file */
	while( (nbread = fread(frame_sh,sizeof(short),nfft,fp)) == nfft) {
		/* increment the number of processed frames */
		nbFrame++;

		/* convert short format input, into 16 bit float */
		sh2fl(nfft, frame_sh, frame, 16, 1);

		/* Hanning Windowing */
		for(i=0; i<nfft; i++) {
			frame[i]=frame[i]*hanning[i];
		}
#ifndef TUNED_FFT
		/* Real Discret Fourier Transform */
		rdft(nfft,frame,real,imag);
		/* Power spectrum computation */
		powSpect(real, imag, powSp, nfft);
#else
		powSpect(nfft,frame, powSp);
#endif

		/* average power spectrum computation */
		for(i=0; i<nfft/2; i++) {
			avg2PowSp[i] = avg2PowSp[i] + (powSp[i]-avg2PowSp[i])/nbFrame;
		}

		/* For overlapping, reposition the file pointer nb_samples_ov samples before its current position */
		fseek ( fp , -nb_samples_ov*2 , SEEK_CUR );
	}
	/* close input file */
	fclose(fp);


	/* .... Save Average Power Spectrum .... */

	/* export vectors in an ASCII file */
	exportASCII(avg1PowSp, avg2PowSp, fs, nfft, asciiFileName);

	if (bmp_mode==1) {
		/* allocate memory for the image and initialize to zero */
		image=calloc(im_wdth*im_hght,sizeof(char));
		/* draw axes */
		xstep = (int) (fstep*(im_wdth-border)/(fmax-fmin));
		ystep=  (int) (pstep*(im_hght-border)/(pmax-pmin));
		for(i=0;i<im_wdth; i++) {
			image[border*im_wdth/2 + i]=1;
			if(i>border/2)
				if(((i-border/2)%xstep)==0)
					for(j=0;j<5;j++)
						image[(border/2+j)*im_wdth + i]=1;
		}
		for(i=0;i<im_hght; i++) {
			image[i*im_wdth + border/2]=1;
			if(i>border/2)
				if(((i-border/2)%ystep)==0)
					for(j=0;j<5;j++)
						image[i*im_wdth + j+ border/2]=1;
		}
		/* draw avg1PowSp */
		draw_linesdB(image, avg1PowSp, nfft/2, im_wdth, im_hght, border, (float) (2*fmax*(nfft/2-1)/(double) fs), (float) (2*fmin*(nfft/2-1)/(double) fs), pmax, pmin,2);
		/* draw avg2PowSp */
		draw_linesdB(image, avg2PowSp, nfft/2, im_wdth, im_hght, border, (float) (2*fmax*(nfft/2-1)/(double) fs), (float) (2*fmin*(nfft/2-1)/(double) fs), pmax, pmin,3);
		/* save bitmap */
		little_endian= (is_little_endian() == 0) ? 1 : 0;
		sav_bmp(im_wdth, im_hght,image,bmpFileName, little_endian);
		printf(" >> Fmin  : %d Hz\n",(int)fmin);
		printf(" >> Fmax  : %d Hz\n",(int)fmax);
		printf(" >> Fstep : %d Hz\n",(int)fstep);
		printf(" >> Pmin  : %2.2f dB\n",pmin);
		printf(" >> Pmax  : %2.2f dB\n",pmax);
		printf(" >> Pstep : %2.2f dB\n",pstep);
	}



	return 0;
}
