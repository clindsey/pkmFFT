/*
 *  pkmAudioFeatures.h
 *
 *  Created by Parag K. Mital - http://pkmital.com 
 *  Contact: parag@pkmital.com
 *
 *  Copyright 2011 Parag K. Mital. All rights reserved.
 * 
 *	Permission is hereby granted, free of charge, to any person
 *	obtaining a copy of this software and associated documentation
 *	files (the "Software"), to deal in the Software without
 *	restriction, including without limitation the rights to use,
 *	copy, modify, merge, publish, distribute, sublicense, and/or sell
 *	copies of the Software, and to permit persons to whom the
 *	Software is furnished to do so, subject to the following
 *	conditions:
 *	
 *	The above copyright notice and this permission notice shall be
 *	included in all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,	
 *	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *	OTHER DEALINGS IN THE SOFTWARE.
 *
 *  Usage:
 *
 *  pkmAudioFeatures *af;
 *
 *  void setup()
 *  {
 *		af = new pkmAudioFeatures();
 *		mfccs = (float *)malloc(sizeof(float) * af->getNumCoefficients());
 *  }
 *
 *  void audioReceived(float *input, int bufferSize, int nChannels)
 *  {
 *		af->computeMFCC(input, mfccs);
 *  }
 *
 */

#pragma once

#include <Accelerate/Accelerate.h>
#include "pkmMatrix.h"
#include "pkmFFT.h"
#include "stdio.h"
#include "string.h"

#define CQ_ENV_THRESH 0.001   // Sparse matrix threshold (for efficient matrix multiplicaton)	

class pkmAudioFeatures
{
public:
	pkmAudioFeatures(int sample_rate = 44100, int fft_size = 512)
	{
		sampleRate = sample_rate;
		fftN = fft_size;
		
		setup();
	}
	
	~pkmAudioFeatures()
	{
		free(CQT);
		free(cqStart);
		free(cqStop);
		
		free(DCT);
		
		free(fft);
		free(fft_magnitudes);
		free(fft_phases);
	}
	
	void setup()
	{
		bpoN = 12;
		
		fft = new pkmFFT(fftN);
		fftOutN = fft->fftSizeOver2;
		fft_magnitudes = (float *)malloc(sizeof(float) * fftOutN);
		fft_phases = (float *)malloc(sizeof(float) * fftOutN);
		
		// low C minus quater tone
		loEdge = 55.0 * pow(2.0, 2.5/12.0);
		hiEdge = 8000.0;
		
		// Constant-Q bandwidth
		fratio = pow(2.0, 1.0/bpoN);				
		cqtN = (int) floor(log(hiEdge/loEdge)/log(fratio));
		
		if(cqtN<1)
			printf("warning: cqtN not positive definite\n");
		
		// The transformation matrix
		CQT = (float *)malloc(sizeof(float)*cqtN*fftOutN);			
		
		// Sparse matrix coding indices
		cqStart = (int *)malloc(sizeof(int)*cqtN);					
		cqStop = (int *)malloc(sizeof(int)*cqtN);			
		
		cqtVector = (float *)malloc(sizeof(float)*cqtN);			
		
		// Full spectrum DCT matrix
		dctN = cqtN; 
		DCT = (float *)malloc(sizeof(float)*cqtN*dctN);
		
		dctVector = (float *)malloc(sizeof(float)*dctN);	
		
		createLogFreqMap();
		createDCT();
	}
	
	void createLogFreqMap()
	{
		// loop variables
		int	i = 0,
			j = 0;
				
		float *fftfrqs = (float *)malloc(sizeof(float)*fftOutN);		// Actual number of real FFT coefficients
		float *logfrqs = (float *)malloc(sizeof(float)*cqtN);			// Number of constant-Q spectral bins
		float *logfbws = (float *)malloc(sizeof(float)*cqtN);			// Bandwidths of constant-Q bins
		float *mxnorm =  (float *)malloc(sizeof(float)*cqtN);			// CQ matrix normalization coefficients

		float N = (float)fftN;
		for(i = 0; i < fftOutN; i++)
			fftfrqs[i] = i * sampleRate / N;
		
		for(i = 0; i < cqtN; i++)
		{
			logfrqs[i] = loEdge * powf(2.0,(float)i/bpoN);
			logfbws[i] = MAX(logfrqs[i] * (fratio - 1.0), sampleRate / N);
		}
		
		float ovfctr = 0.5475;					// Norm constant so CQT'*CQT close to 1.0
		float tmp,tmp2;
		float *ptr;
		float cqEnvThresh = CQ_ENV_THRESH;		// Sparse matrix threshold (for efficient matrix multiplicaton)	
		
		assert(CQT);
		
		// Build the constant-Q transform (CQT)
		ptr = CQT;
		for(i = 0; i < cqtN; i++)
		{
			mxnorm[i] = 0.0;
			tmp2 = 1.0 / (ovfctr * logfbws[i]);
			for(j = 0; j < fftOutN; j++, ptr++)
			{
				tmp = (logfrqs[i] - fftfrqs[j])*tmp2;
				tmp = expf(-0.5 * tmp*tmp);
				*ptr = tmp;								// row major transform
				mxnorm[i] += tmp*tmp;
			}      
			mxnorm[i] = 2.0 * sqrtf(mxnorm[i]);
		}
		
		// Normalize transform matrix for identity inverse
		ptr = CQT;    
		for(i = 0; i < cqtN; i++)
		{
			cqStart[i] = 0;
			cqStop[i] = 0;
			tmp = 1.0/mxnorm[i];
			for(j = 0; j < fftOutN; j++, ptr++)
			{
				*ptr *= tmp;
				if( (!cqStart[i]) && 
					(cqEnvThresh < *ptr) )
				{
					cqStart[i] = j;
				}
				else if( (!cqStop[i]) && 
						 (cqStart[i]) && 
						 (*ptr < cqEnvThresh) )
				{
					cqStop[i] = j;
				}
			}
		}

		// cleanup local dynamic memory
		free(fftfrqs);
		free(logfrqs);
		free(logfbws);
		free(mxnorm);
	}
	
	void createDCT()
	{
		int i,j;
		float nm = 1 / sqrtf( cqtN / 2.0 );

		assert( DCT );
		
		for( i = 0 ; i < dctN ; i++ )
			for ( j = 0 ; j < cqtN ; j++ )
				DCT[ i * cqtN + j ] = nm * cosf( i * (2.0 * j + 1) * M_PI / 2.0 / (float)cqtN );
		for ( j = 0 ; j < cqtN ; j++ )
			DCT[ j ] *= sqrtf(2.0) / 2.0;
	}
	
	void computeMFCC(float *input, float* output)
	{
		
		// should window input buffer before FFT
		fft->forward(0, input, fft_magnitudes, fft_phases);
		
		// sparse matrix product of CQT * FFT
		/*
		int a = 0,b = 0;
		float *ptr1 = 0, *ptr2 = 0, *ptr3 = 0;
		float* mfccPtr = 0;
		
		for( a = 0; a < cqtN ; a++ )
		{
			ptr1 = cqtVector + a; // constant-Q transform vector
			*ptr1 = 0.0;
			ptr2 = CQT + a * fftOutN + cqStart[a];
			ptr3 = fft_magnitudes + cqStart[a];
			b = cqStop[a] - cqStart[a];
			while(b--){
				*ptr1 += *ptr2++ * *ptr3++;
			}
		}
		*/
		
		vDSP_mmul(fft_magnitudes, 1, CQT, 1, cqtVector, 1, 1, cqtN, fftOutN);

		// LFCC ( in-place )
		a = cqtN;
		ptr1 = cqtVector;
		while( a-- ){
			*ptr1 = log10f( *ptr1 );
			ptr1++;
		}
		
		/*
		a = dctN;
		ptr2 = DCT; // point to column of DCT
		mfccPtr = output;
		while( a-- )
		{
			ptr1 = cqtVector;  // point to cqt vector
			*mfccPtr = 0.0; 
			b = cqtN;
			while( b-- )
				*mfccPtr += (float)(*ptr1++ * *ptr2++);
			mfccPtr++;	
		}
		*/
		
		vDSP_mmul(cqtVector, 1, DCT, 1, output, 1, 1, dctN, cqtN);
		//float n = (float) dctN;
		//vDSP_vsdiv(output, 1, &n, output, 1, dctN);
		 
	}
	
	int getNumCoefficients()
	{
		return dctN;
	}
	
	
private:
	float			*sample_data,
					*powerSpectrum;
	
	float			*discreteCosineTransformStorage,		// storage
					*constantQTransformStorage;

	float			*DCT,									// coefficients
					*CQT;
	
	float			*cqtVector,
					*dctVector;
	
	int				*cqStart,								// sparse matrix indices
					*cqStop;
	
	float			loEdge, 
					hiEdge;
	
	float			fratio;
	
	pkmFFT			*fft;
	
	float			*fft_magnitudes,
					*fft_phases;
	
	int				bpoN,
					cqtN,
					dctN,
					fftN,
					fftOutN,
					sampleRate;
	
	
};

static float meanMagnitude(float *buf, int size)
{
	float mean;
	vDSP_svemg(buf, 1, &mean, size);
	mean /= size;
	return mean;
}