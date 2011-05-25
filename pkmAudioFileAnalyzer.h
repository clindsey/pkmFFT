/*
 *  pkmAudioFileAnalyzer.h
 *  autoGrafittiMapKit
 *
 *  Created by Mr. Magoo on 5/12/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#pragma once
#include <vector>
using namespace std;
#include "pkmAudioFeatures.h"
#include "pkmMatrix.h"
#include "pkmAudioFile.h"

class pkmAudioFileAnalyzer
{
public:
	// 44100 / 512 * 93 
	pkmAudioFileAnalyzer(int sample_rate = 44100, 
						 int fft_size = 512, 
						 int hop_size = 0)			// not implemented
	{
		sampleRate = sample_rate;
		fftN = fft_size;
		mfccAnalyzer = new pkmAudioFeatures(sampleRate, fftN);
	}
	
	~pkmAudioFileAnalyzer()
	{
		delete mfccAnalyzer;
	}
	
	void analyzeFile(float *&buffer,					// in
					 int samples,						// in
					 vector<double *> &feature_matrix,	// out
					 vector<pkmAudioFile> &sound_lut,	// out
					 int &num_frames,					// out
					 int &num_features)					// out
	{
		num_frames = samples / fftN;
		num_features = mfccAnalyzer->getNumCoefficients();
		for (int i = 0; i < num_frames; i++) 
		{
			double *featureFrame = (double *)malloc(sizeof(double) * mfccAnalyzer->getNumCoefficients());
			mfccAnalyzer->computeMFCC(buffer + i*fftN, featureFrame);
			feature_matrix.push_back(featureFrame);
			sound_lut.push_back(pkmAudioFile(buffer, i*fftN, samples));
		}
	}
	
	int						sampleRate, 
							fftN;
	pkmAudioFeatures		*mfccAnalyzer;
};
