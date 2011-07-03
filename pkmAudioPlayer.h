/*
 *  pkmAudioPlayer.h
 *  memoryMosaic
 *
 *  Created by Mr. Magoo on 5/24/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "pkmAudioFile.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <Accelerate/Accelerate.h>
#define MIN_FRAMES 5

#ifndef MIN
#define MIN(X,Y) (X) > (Y) ? (Y) : (X)
#endif

class pkmAudioPlayer
{
public:
	pkmAudioPlayer(pkmAudioFile &myFile, int frame_size = 512, int num_frames_to_play = 0, bool loop = true)
	{
		audioFile = myFile;			
		assert((audioFile.length - audioFile.offset) > 0);
		
		//printf("audiofile size: %d, offset: %d\n", audioFile.length, audioFile.offset);
		frameSize = frame_size;		
		if (loop) {
			framesToPlay = num_frames_to_play;
		}
		else {
			framesToPlay = MIN(num_frames_to_play, (myFile.length - myFile.offset) / frame_size);
		}

		bLoop = loop;
		empty = (float *)malloc(sizeof(float) * frameSize);
		rampedBuffer = (float *)malloc(sizeof(float) * frameSize);
		memset(empty, 0, sizeof(float)*frameSize);
		
		// Attack Envelope using ramps
		rampInLength = frameSize/2;
		rampInBuffer = (float *)malloc(sizeof(float) * rampInLength);
		float rampStep = 1.0 / (float)rampInLength;
		float startRamp = 0.0;
		vDSP_vramp(&startRamp, &rampStep, rampInBuffer, 1, rampInLength);
		rampOutLength = frameSize/2;
		rampOutBuffer = (float *)malloc(sizeof(float) * rampOutLength);
		startRamp = 1.0;
		rampStep = -rampStep;
		vDSP_vramp(&startRamp, &rampStep, rampOutBuffer, 1, rampOutLength);

	}
	~pkmAudioPlayer()
	{
		free(empty);
		free(rampedBuffer);
		free(rampInBuffer);
		free(rampOutBuffer);
	}
	bool initialize()
	{
		printf("initialize: \n");
		// default play to the end of the file from the starting offset (if it exists)
		if (framesToPlay == 0) {
			framesToPlay = (audioFile.length - audioFile.offset) / frameSize;
		}
		
		// if the frames to play are too high, decrement it until it matches the end of the file
		while ( (audioFile.length < (audioFile.offset + frameSize*framesToPlay)) )
		{
			framesToPlay--;
		}
		
		printf("frames to play: %d\n", framesToPlay);
		
		if (framesToPlay < MIN_FRAMES) {
			framesToPlay = 1;
			audioFile.buffer = empty;
			audioFile.offset = 0;
			return false;
		}
		// start at the first frame (past the offset if any)
		currentFrame = 0;
		return true;
	}
	
	float getWeight()
	{
		
	}
	
    bool isLastFrame()
    {
        return (currentFrame >= framesToPlay-1);
    }
	
	// get the next audio frame to play 
	float * getNextFrame()
	{
		int offset = currentFrame*frameSize;
		if (bLoop) {
			currentFrame = (currentFrame + 1) % framesToPlay;
		}
		else {
			currentFrame++;
			if (currentFrame >= framesToPlay) {
				return empty;
			}
		}
		// fade in
		if (currentFrame == 0) {
			cblas_scopy(frameSize, audioFile.buffer + audioFile.offset + offset, 1, rampedBuffer, 1);
			vDSP_vmul(rampedBuffer, 1, rampInBuffer, 1, rampedBuffer, 1, rampInLength);
			return rampedBuffer;
		}
		// fade out
		else if(currentFrame == framesToPlay-1) {
			//printf("f\n");
			cblas_scopy(frameSize, audioFile.buffer + audioFile.offset + offset, 1, rampedBuffer, 1);
			vDSP_vmul(rampedBuffer + frameSize - rampOutLength, 1, rampOutBuffer, 1, rampedBuffer + frameSize - rampOutLength, 1, rampOutLength);
			return rampedBuffer;			
		}
		// no fade
		else
			return (audioFile.buffer + audioFile.offset + offset);
	}
	
	bool isFinished()
	{
		return !bLoop && currentFrame >= framesToPlay;
	}
	
	pkmAudioFile		audioFile;
	
	int					frameSize;
	int					framesToPlay;
	int					currentFrame;
	bool				bLoop;
	int					rampInLength, rampOutLength;
	float				*empty, *rampInBuffer, *rampOutBuffer, *rampedBuffer;
};
