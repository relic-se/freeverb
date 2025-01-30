// Reverb model implementation
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

#include "revmodel.hpp"
#include <stdlib.h>

revmodel::revmodel()
{
	setstereo(true);
	setsamplerate(44100);
	reset();

	// Set default values
	for (int i=0;i<numallpasses;i++) {
		allpassL[i].setfeedback(0.5f);
		allpassR[i].setfeedback(0.5f);
	}
	setwet(initialwet);
	setroomsize(initialroom);
	setdry(initialdry);
	setdamp(initialdamp);
	setwidth(initialwidth);
	setmode(initialmode);

	// Buffer will be full of rubbish - so we MUST mute them
	mute();
}

void revmodel::reset()
{
	// Tie the components to their buffers
	int i, tuning;
	for (i=0;i<numcombs;i++) {
		tuning = resetallpassbuf((float *)bufcombL[i], i, false);
		combL[i].setbuffer(bufcombL[i], tuning);

		tuning = resetallpassbuf((float *)bufcombR[i], i, true);
		combR[i].setbuffer(bufcombR[i], tuning);
	}
	for (i=0;i<numallpasses;i++) {
		tuning = resetallpassbuf((float *)bufallpassL[i], i, false);
		allpassL[i].setbuffer(bufallpassL[i], tuning);

		tuning = resetallpassbuf((float *)bufallpassR[i], i, true);
		allpassR[i].setbuffer(bufallpassR[i], tuning);
	}
}

int revmodel::resetcombbuf(float *buf, int i, bool stereo)
{
	int tuning = gettuning(i, (int *)combtuning, numcombtuning, stereo);
	if (buf)
		delete buf;
	buf = (float *)std::malloc(tuning * sizeof(float));
	return tuning;
}

int revmodel::resetallpassbuf(float *buf, int i, bool stereo)
{
	int tuning = gettuning(i, (int *)allpasstuning, numallpasstuning, stereo);
	if (buf)
		delete buf;
	buf = (float *)std::malloc(tuning * sizeof(float));
	return tuning;
}

int revmodel::gettuning(int i, int *map, int size, bool stereo)
{
	int tuning;
	if (i < size) {
		tuning = map[i];
	} else {
		tuning = (map[size - 1] - map[size - 2]) * (i - (size - 1)) + map[size - 1];
	}
	tuning = tuning * samplerate / 44100;
	if (stereo)
		tuning += stereospread;
	return tuning;
}

void revmodel::mute()
{
	if (getmode() >= freezemode)
		return;
	
	int i;
	for (i=0;i<numcombs;i++)
	{
		combL[i].mute();
		combR[i].mute();
	}
	for (i=0;i<numallpasses;i++)
	{
		allpassL[i].mute();
		allpassR[i].mute();
	}
}

void revmodel::processreplace(float *inputL, float *inputR, float *outputL, float *outputR, long numsamples, int skip)
{
	float outL,outR,input;
	int i;

	while(numsamples-- > 0)
	{
		outL = outR = 0;
		input = stereo ? ((*inputL + *inputR) * gain) : (*inputL * gain);

		// Accumulate comb filters in parallel
		for(i=0; i<numcombs; i++)
		{
			outL += combL[i].process(input);
			if (stereo) outR += combR[i].process(input);
		}

		// Feed through allpasses in series
		for(i=0; i<numallpasses; i++)
		{
			outL = allpassL[i].process(outL);
			if (stereo) outR = allpassR[i].process(outR);
		}

		// Calculate output REPLACING anything already there
		if (stereo) {
			*outputL = outL*wet1 + outR*wet2 + *inputL*dry;
			*outputR = outR*wet1 + outL*wet2 + *inputR*dry;
		} else {
			*outputL = outL*wet + *inputL*dry;
		}

		// Increment sample pointers, allowing for interleave (if any)
		inputL += skip;
		outputL += skip;
		if (stereo) {
			inputR += skip;
			outputR += skip;
		}
	}
}

void revmodel::processmix(float *inputL, float *inputR, float *outputL, float *outputR, long numsamples, int skip)
{
	float outL,outR,input;
	int i;

	while(numsamples-- > 0)
	{
		outL = outR = 0;
		input = stereo ? ((*inputL + *inputR) * gain) : (*inputL * gain);

		// Accumulate comb filters in parallel
		for(i=0; i<numcombs; i++)
		{
			outL += combL[i].process(input);
			if (stereo) outR += combR[i].process(input);
		}

		// Feed through allpasses in series
		for(i=0; i<numallpasses; i++)
		{
			outL = allpassL[i].process(outL);
			if (stereo) outR = allpassR[i].process(outR);
		}

		// Calculate output MIXING with anything already there
		if (stereo) {
			*outputL += outL*wet1 + outR*wet2 + *inputL*dry;
			*outputR += outR*wet1 + outL*wet2 + *inputR*dry;
		} else {
			*outputL += outL*wet + *inputL*dry;
		}

		// Increment sample pointers, allowing for interleave (if any)
		inputL += skip;
		outputL += skip;
		if (stereo) {
			inputR += skip;
			outputR += skip;
		}
	}
}

void revmodel::update()
{
// Recalculate internal values after parameter change

	int i;

	wet1 = wet*(width/2 + 0.5f);
	wet2 = wet*((1-width)/2);

	if (mode >= freezemode)
	{
		roomsize1 = 1;
		damp1 = 0;
		gain = muted;
	}
	else
	{
		roomsize1 = roomsize;
		damp1 = damp;
		gain = fixedgain;
	}

	for(i=0; i<numcombs; i++)
	{
		combL[i].setfeedback(roomsize1);
		combR[i].setfeedback(roomsize1);
	}

	for(i=0; i<numcombs; i++)
	{
		combL[i].setdamp(damp1);
		combR[i].setdamp(damp1);
	}
}

// The following get/set functions are not inlined, because
// speed is never an issue when calling them, and also
// because as you develop the reverb model, you may
// wish to take dynamic action when they are called.

void revmodel::setroomsize(float value)
{
	roomsize = (value*scaleroom) + offsetroom;
	update();
}

float revmodel::getroomsize()
{
	return (roomsize-offsetroom)/scaleroom;
}

void revmodel::setdamp(float value)
{
	damp = value*scaledamp;
	update();
}

float revmodel::getdamp()
{
	return damp/scaledamp;
}

void revmodel::setwet(float value)
{
	wet = value*scalewet;
	update();
}

float revmodel::getwet()
{
	return wet/scalewet;
}

void revmodel::setdry(float value)
{
	dry = value*scaledry;
}

float revmodel::getdry()
{
	return dry/scaledry;
}

void revmodel::setwidth(float value)
{
	width = value;
	update();
}

float revmodel::getwidth()
{
	return width;
}

void revmodel::setmode(float value)
{
	mode = value;
	update();
}

float revmodel::getmode()
{
	if (mode >= freezemode)
		return 1;
	else
		return 0;
}

void revmodel::setstereo(bool value) {
	stereo = value;
}

void revmodel::setsamplerate(int value) {
	samplerate = value;
	reset();
	mute();
}

bool revmodel::getstereo() {
	return stereo;
}

//ends
