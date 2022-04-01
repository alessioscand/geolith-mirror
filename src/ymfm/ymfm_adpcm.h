// BSD 3-Clause License
//
// Copyright (c) 2021, Aaron Giles
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef YMFM_ADPCM_H
#define YMFM_ADPCM_H

#pragma once

#include "ymfm.h"

namespace ymfm
{

//*********************************************************
//  INTERFACE CLASSES
//*********************************************************

// forward declarations
struct adpcm_a_engine;
struct adpcm_b_engine;

void adpcm_a_registers_write_start(uint32_t choffs, uint32_t address);
void adpcm_a_registers_write_end(uint32_t choffs, uint32_t address);

// ======================> adpcm_a_channel

struct adpcm_a_channel
{
	// constructor
	adpcm_a_channel(uint32_t choffs, uint32_t addrshift);

	// reset the channel state
	void reset();

	// signal key on/off
	void keyonoff(bool on);

	// master clockingfunction
	bool clock();

	// return the computed output value, with panning applied
	void output(ymfm_output<2> &output) const;

	// internal state
	uint32_t const m_choffs;              // channel offset
	uint32_t const m_address_shift;       // address bits shift-left
	uint32_t m_playing;                   // currently playing?
	uint32_t m_curnibble;                 // index of the current nibble
	uint32_t m_curbyte;                   // current byte of data
	uint32_t m_curaddress;                // current address
	int32_t m_accumulator;                // accumulator
	int32_t m_step_index;                 // index in the stepping table
};


// ======================> adpcm_a_engine

struct adpcm_a_engine
{
	// constructor
	adpcm_a_engine(uint32_t addrshift);

	// reset our status
	void reset();

	// master clocking function
	uint32_t clock(uint32_t chanmask);

	// compute sum of channel outputs
	void output(ymfm_output<2> &output, uint32_t chanmask);

	// write to the ADPCM-A registers
	void write(uint32_t regnum, uint8_t data);

	// set the start/end address for a channel (for hardcoded YM2608 percussion)
	void set_start_end(uint8_t chnum, uint16_t start, uint16_t end)
	{
		uint32_t choffs = chnum;
		adpcm_a_registers_write_start(choffs, start);
		adpcm_a_registers_write_end(choffs, end);
	}

	// internal state
	adpcm_a_channel *m_channel[6];                 // array of channels
};


// ======================> adpcm_b_channel

struct adpcm_b_channel
{
	// constructor
	adpcm_b_channel(uint32_t addrshift);

	// reset the channel state
	void reset();

	// signal key on/off
	void keyonoff(bool on);

	// master clocking function
	void clock();

	// return the computed output value, with panning applied
	void output(ymfm_output<2> &output, uint32_t rshift) const;

	// return the status register
	uint8_t status() const { return m_status; }

	// handle special register reads
	uint8_t read(uint32_t regnum);

	// handle special register writes
	void write(uint32_t regnum, uint8_t value);

	// helper - return the current address shift
	uint32_t address_shift() const;

	// load the start address
	void load_start();

	// limit checker
	bool at_limit() const;

	// end checker
	bool at_end() const;

	// internal state
	uint32_t const m_address_shift; // address bits shift-left
	uint32_t m_status;              // currently playing?
	uint32_t m_curnibble;           // index of the current nibble
	uint32_t m_curbyte;             // current byte of data
	uint32_t m_dummy_read;          // dummy read tracker
	uint32_t m_position;            // current fractional position
	uint32_t m_curaddress;          // current address
	int32_t m_accumulator;          // accumulator
	int32_t m_prev_accum;           // previous accumulator (for linear interp)
	int32_t m_adpcm_step;           // next forecast
};


// ======================> adpcm_b_engine

struct adpcm_b_engine
{
	// constructor
	adpcm_b_engine(uint32_t addrshift = 0);

	// reset our status
	void reset();

	// master clocking function
	void clock();

	// compute sum of channel outputs
	void output(ymfm_output<2> &output, uint32_t rshift);

	// read from the ADPCM-B registers
	uint32_t read(uint32_t regnum) { return m_channel->read(regnum); }

	// write to the ADPCM-B registers
	void write(uint32_t regnum, uint8_t data);

	// status
	uint8_t status() const { return m_channel->status(); }

	// internal state
	adpcm_b_channel *m_channel;                 // channel pointer
};

}

#endif // YMFM_ADPCM_H
