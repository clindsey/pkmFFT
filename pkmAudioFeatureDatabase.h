/*
 *  pkmAudioFeatureDatabase.h
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
#include "pkmAudioFileAnalyzer.h"
#include "pkmMatrix.h"
#include "pkmAudioFile.h"
#include "ANN.h"						// kd-tree
#include <Accelerate/Accelerate.h>

class pkmAudioFeatureDatabase
{
public:
	pkmAudioFeatureDatabase(int sample_rate = 44100, 
							int fft_size = 512)
	{
		sampleRate = sample_rate;
		fftN = fft_size;
		bBuiltIndex = false;
		analyzer = new pkmAudioFileAnalyzer(sampleRate, fftN);
		
		k				= 3;								// number of nearest neighbors
		nnIdx			= new ANNidx[k];					// allocate near neighbor indices
		dists			= new ANNdist[k];					// allocate near neighbor dists	
		
	}
	~pkmAudioFeatureDatabase()
	{
		delete analyzer;
		
		// we free here because in upper level the segmenter allocates this data
		// this is really stupid but a solution for now.
		for (int i = 0; i < unique_buffers.size(); i++) {
			free(unique_buffers[i]);
		}
	}
	
	void addSound(float *&buf, int size)
	{
		float					*buf_copy = (float *)malloc(sizeof(float) * size);
		vector<double * >		feature_matrix;
		vector<pkmAudioFile>	sound_lut;
		int						num_frames, num_features;
		
		// store buffer
		cblas_scopy(size, buf, 1, buf_copy, 1);
		
		// get the features for this buffer for every frame
		analyzer->analyzeFile(buf_copy, 
							  size, 
							  feature_matrix,				// every audio frames mfccs
							  sound_lut,					// every audio frame has a reference to the buffer, the offset, and the length of the buffer
							  num_frames,					// total number of frames for this audio file
							  num_features);				// number of coefficients
		
		// concatenate features to our database
		//feature_database.insert(feature_database.end(),		// append it to the end of the feature database
		//						feature_matrix.begin(), 
		//						feature_matrix.end());
		
		// and the pointer to the audio frames
		//audio_database.insert(audio_database.end(),			// and the audio database
		//					  sound_lut.begin(), 
		//					  sound_lut.end());
		
		for (int i = 0; i < feature_matrix.size(); i++) {
			feature_database.push_back(feature_matrix[i]);
			audio_database.push_back(sound_lut[i]);
		}
		
		//printf("features: %d, audio-frames: %d\n", feature_database.size(), audio_database.size());
		numFrames = feature_database.size();
		numFeatures = num_features;
		
		// keep the buffer pointers for deallocation
		unique_buffers.push_back(buf_copy);
	}
	
	void buildIndex()
	{

		dim				= numFeatures;						// dimension of data (x,y,z)
		pts				= numFrames;						// maximum number of data points
		
		if(bBuiltIndex)
		{
			delete kdTree;
			bBuiltIndex = false;
		}

		ANNpointArray positions = annAllocPts(pts, dim);
		for (int i = 0; i < pts; i++) 
		{
			for (int j = 0; j < dim; j++)
			{
				positions[i][j] = feature_database[i][j];
			}
		}
		
		kdTree = new ANNkd_tree(							// build search structure
								positions,//(ANNpointArray)&(feature_database[0]),		// the data points
								pts,						// number of points
								dim);						// dimension of space
		bBuiltIndex = true;
		//annDeallocPts(positions);
	}
	
	
	// get called in the audio requested thread at audio rate
	vector<pkmAudioFile> getNearestFrame(float *&frame, int bufferSize)
	{
		vector<pkmAudioFile> nearestAudioFrames;
		if (bufferSize != fftN) {
			//printf("[ERROR] Buffer size does not match database settings");
			return nearestAudioFrames;
		}
		if (!bBuiltIndex) {
			//printf("[ERROR] First build the index with buildIndex()");
			return nearestAudioFrames;
		}
		vector<double * > feature_matrix;
		vector<pkmAudioFile> audio_instance;
		int num_frames, num_features;
		analyzer->analyzeFile(frame, 
							  bufferSize, 
							  feature_matrix,				// every audio frames mfccs
							  audio_instance,				// every audio frame has a reference to the buffer, the offset, and the length of the buffer
							  num_frames,					// total number of frames for this audio file
							  num_features);				// number of coefficients


		// just the first frame
		ANNpoint queryPt = feature_matrix[0];
		
		kdTree->annkSearch(							// search
						   queryPt,					// query point
						   k,						// number of near neighbors
						   nnIdx,					// nearest neighbors (returned)
						   dists,					// distance (returned)
						   0.1);					// error bound
		
		
		//float sumDists = (dists[0] + dists[1] + dists[2]);
		//printf("sum dist: %f\n", sumDists);
		//if (isnan(sumDists)) {
		//	return nearestAudioFrames;
		//}
		
		for (int i = 0; i < k; i++) {
			//printf("i-th idx: %d, dist: %f\n", nnIdx[i], dists[i]);
			nearestAudioFrames.push_back(audio_database[nnIdx[i]]);
		}

		return nearestAudioFrames;
	}
	
	int size()
	{
		return unique_buffers.size();
	}
	
	
	int							sampleRate, 
								fftN;
	pkmAudioFileAnalyzer		*analyzer;
	vector<double *>			feature_database;
	vector<pkmAudioFile>		audio_database;
	vector<float *>				unique_buffers;
	int							numFeatures,
								numFrames;
	
	// For kNN
	ANNkd_tree					*kdTree;		// distances to nearest HRTFs
	ANNidxArray					nnIdx;			// near neighbor indices
	ANNdistArray				dists;			// near neighbor distances
	int							k;				// number of nearest neighbors
	int							dim;			// dimension of each point
	int							pts;			// number of points
//	ANNpointArray				positions;		// positions of each filter
	bool						bBuiltIndex, bSemaphore;
};