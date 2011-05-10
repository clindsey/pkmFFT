/*
 *  pkmSTFT.h
 *
 *  STFT implementation making use of Apple's Accelerate Framework (pkmFFT)
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
 *
 *  Usage:
 *
 *  // be sure to either use malloc or __attribute__ ((aligned (16))
 *  int buffer_size = 4096;
 *  float *sample_data = (float *) malloc (sizeof(float) * buffer_size);
 *  pkm::Mat magnitude_matrix, phase_matrix;
 *
 *  pkmSTFT *stft;
 *  stft = new pkmSTFT(512);
 *  stft.STFT(sample_data, buffer_size, magnitude_matrix, phase_matrix);
 *  fft.ISTFT(sample_data, buffer_size, magnitude_matrix, phase_matrix);
 *  delete stft;
 *
 */


#include "pkmFFT.h"
#include "pkmMatrix.h"

class pkmSTFT
{
public:

	pkmSTFT(int size)
	{
		fftSize = size;
		numFFTs = 0;
		fftBins = fftSize/2;
		hopSize = fftSize/4;
		windowSize = fftSize;
		bufferSize = 0;
		
		initializeFFTParameters(fftSize, windowSize, hopSize);
	}
	~pkmSTFT()
	{
		free(FFT);
	}
	
	void initializeFFTParameters(int _fftSize, int _windowSize, int _hopSize)
	{
		fftSize = _fftSize;
		hopSize = _hopSize;
		windowSize = _windowSize;
		
		// fft constructor
		FFT = new pkmFFT(fftSize);
	}
	
	void STFT(float *buf, int bufSize, pkm::Mat &M_magnitudes, pkm::Mat &M_phases)
	{	
		// pad input buffer
		int padding = ceilf((float)bufSize/(float)fftSize) * fftSize - bufSize;
		float *padBuf;
		if (padding) {
			printf("Padding %d sample buffer with %d samples\n", bufSize, padding);
			padBufferSize = bufSize + padding;
			padBuf = (float *)malloc(sizeof(float)*padBufferSize);
			// set padding to 0
			memset(&(padBuf[bufSize]), 0, sizeof(float)*padding);
			// copy original buffer into padded one
			memcpy(padBuf, buf, sizeof(float)*bufSize);	}
		else {
			padBuf = buf;
			padBufferSize = bufSize;
		}
		
		// create output fft matrix
		numWindows = (padBufferSize - fftSize)/hopSize + 1;
		
		if (M_magnitudes.rows != numWindows && M_magnitudes.cols != fftBins) {
			M_magnitudes.reset(numWindows, fftBins, true);
			M_phases.reset(numWindows, fftBins, true);
		}
		
		// stft
		for (int i = 0; i < numWindows; i++) {
			
			// get current col of freq mat
			float *magnitudes = M_magnitudes.row(i);
			float *phases = M_phases.row(i);
			float *buffer = padBuf + i*hopSize;
			
			FFT->forward(0, buffer, magnitudes, phases);	
			
			
		}
		// release padded buffer
		if (padding) {
			free(padBuf);
		}
	}
	
	
	void ISTFT(float *buf, int bufSize, pkm::Mat &M_magnitudes, pkm::Mat &M_phases)
	{
		int padding = ceilf((float)bufSize/(float)fftSize) * fftSize - bufSize;
		float *padBuf;
		if (padding) 
		{
			printf("Padding %d sample buffer with %d samples\n", bufSize, padding);
			padBufferSize = bufSize + padding;
			padBuf = (float *)calloc(padBufferSize, sizeof(float));
		}
		else {
			padBuf = buf;
			padBufferSize = bufSize;
		}
		
		pkm::Mat M_istft(padBufferSize, 1, padBuf, false);
		
		for(int i = 0; i < numWindows; i++)
		{
			float *buffer = padBuf + i*hopSize;
			float *magnitudes = M_magnitudes.row(i);
			float *phases = M_phases.row(i);
			
			FFT->inverse(0, buffer, magnitudes, phases);
		}

		memcpy(buf, padBuf, sizeof(float)*bufSize);
		// release padded buffer
		if (padding) {
			free(padBuf);
		}
	}
	
	pkmFFT				*FFT;
	
private:
	
	
	int				sampleRate,
						numFFTs,
						fftSize,
						fftBins,
						hopSize,
						bufferSize,
						padBufferSize,
						windowSize,
						numWindows;
};