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

#define MIN_FRAMES 5

class pkmAudioPlayer
{
public:
	pkmAudioPlayer(pkmAudioFile &myFile, int frame_size = 512, int num_frames_to_play = 0, bool loop = true)
	{
		audioFile = myFile;			
		//printf("audiofile size: %d, offset: %d\n", audioFile.length, audioFile.offset);
		frameSize = frame_size;		
		framesToPlay = num_frames_to_play;
		bLoop = loop;
		empty = (float *)malloc(sizeof(float) * frameSize);
		memset(empty, 0, sizeof(float)*frameSize);
		
		assert((audioFile.length - audioFile.offset) > 0);
	}
	~pkmAudioPlayer()
	{
		free(empty);
	}
	bool initialize()
	{
		
		// default play to the end of the file from the starting offset (if it exists)
		if (framesToPlay == 0) {
			framesToPlay = (audioFile.length - audioFile.offset) / frameSize;
		}
		
		// if the frames to play are too high, decrement it until it matches the end of the file
		while ( (audioFile.length < (audioFile.offset + frameSize*framesToPlay)) )
		{
			framesToPlay--;
		}
		
		//printf("frames to play: %d\n", framesToPlay);
		
		if (framesToPlay <= MIN_FRAMES) {
			framesToPlay = 1;
			audioFile.buffer = empty;
			audioFile.offset = 0;
			return false;
		}
		// start at the first frame (past the offset if any)
		currentFrame = 0;
		return true;
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
			if (currentFrame > framesToPlay) {
				return empty;
			}
		}
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
	float				*empty;
};
