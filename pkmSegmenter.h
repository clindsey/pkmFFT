/*
 *  pkmSegmenter.h
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
 */

#pragma once

#include <Accelerate/Accelerate.h>
#include "pkmAudioFeatures.h"
#include "pkmMatrix.h"
#include "pkmRecorder.h"

const int SAMPLE_RATE = 44100;
const int FRAME_SIZE = 512;
const int NUM_BACK_BUFFERS_FOR_FEATURE_ANALYSIS = SAMPLE_RATE*1/FRAME_SIZE;
const int NUM_FORE_BUFFERS_FOR_FEATURE_ANALYSIS = SAMPLE_RATE*1/FRAME_SIZE;
const int NUM_BUFFERS_FOR_SEGMENTATION_ANALYSIS = SAMPLE_RATE*5/FRAME_SIZE;
const int MIN_SEGMENT_LENGTH = SAMPLE_RATE*1;
const int MAX_SEGMENT_LENGTH = SAMPLE_RATE*4;


class pkmSegmenter
{
public:
	
	// buffer size refers to the length of the buffer to average over
	// frame size is each audio chunk
	pkmSegmenter(bool showDrawing = true)
	{
		audioFeature				= new pkmAudioFeatures(SAMPLE_RATE, FRAME_SIZE);
		audioIn						= (float *)malloc(sizeof(float) * FRAME_SIZE);
		current_feature				= (float *)malloc(sizeof(float) * audioFeature->getNumCoefficients());
		feature_background_buffer	= pkm::Mat(NUM_BACK_BUFFERS_FOR_FEATURE_ANALYSIS, audioFeature->getNumCoefficients());
		feature_foreground_buffer	= pkm::Mat(NUM_FORE_BUFFERS_FOR_FEATURE_ANALYSIS, audioFeature->getNumCoefficients());
		background_distance_buffer	= pkm::Mat(NUM_BUFFERS_FOR_SEGMENTATION_ANALYSIS, 1);
		foreground_distance_buffer	= pkm::Mat(NUM_BUFFERS_FOR_SEGMENTATION_ANALYSIS, 1);
		feature_background_average	= pkm::Mat(1, audioFeature->getNumCoefficients());
		feature_foreground_average	= pkm::Mat(1, audioFeature->getNumCoefficients());
		
		bSegmenting					= false;
		bSegmented					= false;
		bDraw						= showDrawing;
		
		// recorded segment
		audioSegment				= new pkmRecorder();
	}
	~pkmSegmenter()
	{
		delete audioFeature;
		free(audioIn);
		free(current_feature);
		delete audioSegment;
	}
	
	float distanceMetric(float *buf1, float *buf2, int size)
	{
		return pkm::Mat::sumOfAbsoluteDifferences(buf1, buf2, size);
	}
	
	void resetBackgroundModel()
	{
		feature_background_buffer	= pkm::Mat(NUM_BACK_BUFFERS_FOR_FEATURE_ANALYSIS, audioFeature->getNumCoefficients());
	}
	
	// given a new frame of audio, did we detect a segment?
	bool update()
	{
		if (feature_background_buffer.bCircularInsertionFull) 
		{
			// find the average of the past N feature frames
			feature_background_average = 
				feature_background_buffer.sum() / (float)NUM_BACK_BUFFERS_FOR_FEATURE_ANALYSIS;
			
			// get the distance from the current frame and the previous N frame's average 
			// (note this can be any metric)
			float distance = 
				distanceMetric(feature_background_average.data, 
							   current_feature, 
							   feature_background_average.cols);
			
			// if we aren't segmenting, add the current distance to the previous M distances buffer
			if (!bSegmenting) {
				background_distance_buffer.insertRowCircularly(&distance);
			}
			
			// calculate the mean and deviation for analysis
			float mean_distance = pkm::Mat::mean(background_distance_buffer.data, background_distance_buffer.rows);
			float std_distance = sqrtf(fabs(pkm::Mat::var(background_distance_buffer.data, background_distance_buffer.rows)));		
			
			//printf("distance: %f\nmean_distance: %f\nstd_distance: %f\n", distance, mean_distance, std_distance);
			
			// if it is an outlier, then we have detected an event
			if (!bSegmenting && 
				(fabs(distance - mean_distance) - 4.0f*std_distance) > 0)
			{
				pkm::Mat current_feature_mat(1, audioFeature->getNumCoefficients(), current_feature, false);
				feature_foreground_buffer = pkm::Mat::repeat(current_feature_mat, NUM_FORE_BUFFERS_FOR_FEATURE_ANALYSIS);
				foreground_distance_buffer.reset(NUM_FORE_BUFFERS_FOR_FEATURE_ANALYSIS, 1);
				bSegmenting = true;
			}
			// we are segmenting, check for conditions to stop segmenting if we are
			// passed the minimum segment length
			else if(bSegmenting && audioSegment->size > MIN_SEGMENT_LENGTH)
			{
				// too similar to the original background, no more event
				if( (fabs(distance - mean_distance) - 0.5f*std_distance) < 0 )
				{
					bSegmenting = false;
					bSegmented = true;
				}
				// too big a file, stop segmenting
				else if(audioSegment->size >= MAX_SEGMENT_LENGTH)
				{
					//resetBackgroundModel();
					bSegmenting = false;
					bSegmented = true;
				}
				// 
				else 
				{
					// find the average of the past N feature frames
					feature_foreground_average = feature_foreground_buffer.sum() / (float)NUM_FORE_BUFFERS_FOR_FEATURE_ANALYSIS;
					
					// get the distance from the current frame and the previous N frame's average (note this can be any metric)
					float fore_distance = distanceMetric(feature_foreground_average.data, current_feature, feature_foreground_average.cols);
					
					// if we aren't segmenting, add the current distance to the previous M distances buffer
					foreground_distance_buffer.insertRowCircularly(&distance);
					
					// calculate the mean and deviation for analysis
					float mean_fore_distance = pkm::Mat::mean(foreground_distance_buffer.data, foreground_distance_buffer.rows);
					float std_fore_distance = sqrtf(fabs(pkm::Mat::var(foreground_distance_buffer.data, foreground_distance_buffer.rows)));	
					
					if ( foreground_distance_buffer.bCircularInsertionFull && 
						(fabs(fore_distance - mean_fore_distance) - 3.5f*std_fore_distance) > 0)
					{
						bSegmenting = false;
						bSegmented = true;
					}
				}
			}
			/*
			if(bDraw)
			{
				// Print the statistics
				float rms_feature = pkm::Mat::rms(feature_background_average.data, feature_background_average.cols);
				char buf[256];
				sprintf(buf, "distance: %8f\nstd(distance): %8f\nmean(distance): %f\nRMS: %8f", distance, std_distance, mean_distance, rms_feature);
				ofSetColor(255, 255, 255);
				ofDrawBitmapString(buf, 20, 20);
				
				
				// visual feedback showing a square during an event
				if (bSegmenting) {	
					ofRect(150, 20, 150, 20);
				}
				
				// Draw the current feature and the Avg Background and Foreground feature
				float width_step = ofGetScreenWidth() / audioFeature->getNumCoefficients();
				float height_factor = ofGetScreenHeight() / 4.0f;
				
				ofSetColor(255, 255, 255);
				for (int i = 0; i < audioFeature->getNumCoefficients()-1; i++) {
					ofLine(i*width_step, ofGetScreenHeight() - fabs(current_feature[i]*height_factor), 
						   (i+1)*width_step, ofGetScreenHeight() - fabs(current_feature[i+1]*height_factor));
				}
				
				ofSetColor(200, 10, 100);
				for (int i = 0; i < audioFeature->getNumCoefficients()-1; i++) {
					ofLine(i*width_step, ofGetScreenHeight() - fabs(feature_background_average.data[i]*height_factor), 
						   (i+1)*width_step, ofGetScreenHeight() - fabs(feature_background_average.data[i+1]*height_factor));
				}
				
				ofSetColor(10, 200, 100);
				for (int i = 0; i < audioFeature->getNumCoefficients()-1; i++) {
					ofLine(i*width_step, ofGetScreenHeight() - fabs(feature_foreground_average.data[i]*height_factor), 
						   (i+1)*width_step, ofGetScreenHeight() - fabs(feature_foreground_average.data[i+1]*height_factor));
				}
			}
			 */
		}
		else {
			// find the average of the past N feature frames
			feature_background_average = feature_background_buffer.sum() / (float)NUM_BACK_BUFFERS_FOR_FEATURE_ANALYSIS;
			
			// get the distance from the current frame and the previous N frame's average (note this can be any metric)
			float distance = pkm::Mat::sumOfAbsoluteDifferences(feature_background_average.data, current_feature, feature_background_average.cols);
			
			// if we aren't segmenting, add the current distance to the previous M distances buffer
			if (!bSegmenting) {
				background_distance_buffer.insertRowCircularly(&distance);
			}
			
			/*
			 if(bDraw)
			 {
				char buf[256];
				sprintf(buf, "initializing model...");
			
				ofSetColor(255, 255, 255);
				ofDrawBitmapString(buf, 150, 180);
			 }
			 */
		}
		
		return bSegmented;
	}
	
	// get the last recorded segment (if updateAudio() == true)
	void getSegment(float **buf, int &buf_size, float **features, int &feature_size)
	{
		if (!bSegmented) {
			printf("[ERROR]: Should only call this function once and only if update() returns true!");
			return;
		}
		buf_size = audioSegment->size;
		printf("segmenting %d samples\n", buf_size);
		*buf = (float *)malloc(sizeof(float) * buf_size);
		cblas_scopy(buf_size, audioSegment->data, 1, *buf, 1);
		audioSegment->reset();
		
		feature_size = audioFeature->getNumCoefficients();
		*features = (float *)malloc(sizeof(float) * feature_size);
		cblas_scopy(feature_size, feature_foreground_average.data, 1, *features, 1);
		
		bSegmented = false;
	}
	
	void audioReceived(float * input, int bufferSize, int nChannels)
	{
		audioFeature->computeMFCC(input, current_feature);
		if (bSegmenting) {
			feature_foreground_buffer.insertRowCircularly(current_feature);
			audioSegment->insert(input, bufferSize);
		}
		else {
			feature_background_buffer.insertRowCircularly(current_feature);
		}
	}
	
	
	pkmRecorder				*audioSegment;

	pkmAudioFeatures		*audioFeature;
	float					*audioIn, 
							*current_feature;
	
	pkm::Mat				feature_background_buffer;
	pkm::Mat				feature_background_average;
	pkm::Mat				feature_foreground_buffer;
	pkm::Mat				feature_foreground_average;
	pkm::Mat				feature_deviation;
	pkm::Mat				background_distance_buffer;
	pkm::Mat				foreground_distance_buffer;
	
	bool					bSegmenting, bSegmented, bDraw;
};