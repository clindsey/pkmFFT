/*
 *  pkmAudioDatabase.h
 *  mosaic
 *
 *  Created by Mr. Magoo on 4/29/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#pragma once


const int MAX_SIMULTANEOUS_RECORDING_STREAMS = 1;

class pkmAudioDatabase
{
public:
	pkmAudioDatabase(int frame_size)
	{
		frameSize = frame_size;
		tolerance = 0.5;
		stft = new pkmSTFT(frameSize);
	}
	
private:

	
};