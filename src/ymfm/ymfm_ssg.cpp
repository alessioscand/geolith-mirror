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

#include <stdint.h>
#include <string.h>

#include "ymfm_ssg.h"

#define REGISTERS 0x10

// internal state
static uint8_t m_regdata[REGISTERS];         // register data

namespace ymfm
{

//*********************************************************
// SSG REGISTERS
//*********************************************************

// direct read/write access
uint8_t ssg_registers_read(uint32_t index) {
	return m_regdata[index];
}

void ssg_registers_write(uint32_t index, uint8_t data) {
	m_regdata[index] = data;
}

// system-wide registers
static inline uint32_t ssg_registers_noise_period(void) {
	return bitfield(m_regdata[0x06], 0, 5);
}

static inline uint32_t ssg_registers_io_b_out(void) {
	return bitfield(m_regdata[0x07], 7);
}

static inline uint32_t ssg_registers_io_a_out(void) {
	return bitfield(m_regdata[0x07], 6);
}

static inline uint32_t ssg_registers_envelope_period(void) {
	return m_regdata[0x0b] | (m_regdata[0x0c] << 8);
}

static inline uint32_t ssg_registers_envelope_continue(void) {
	return bitfield(m_regdata[0x0d], 3);
}

static inline uint32_t ssg_registers_envelope_attack(void) {
	return bitfield(m_regdata[0x0d], 2);
}

static inline uint32_t ssg_registers_envelope_alternate(void) {
	return bitfield(m_regdata[0x0d], 1);
}

static inline uint32_t ssg_registers_envelope_hold(void) {
	return bitfield(m_regdata[0x0d], 0);
}

static inline uint32_t ssg_registers_io_a_data(void) {
	return m_regdata[0x0e];
}

static inline uint32_t ssg_registers_io_b_data(void) {
	return m_regdata[0x0f];
}

// per-channel registers
static inline uint32_t ssg_registers_ch_noise_enable(uint32_t choffs) {
	return bitfield(~m_regdata[0x07], 3 + choffs);
}

static inline uint32_t ssg_registers_ch_tone_enable(uint32_t choffs) {
	return bitfield(~m_regdata[0x07], 0 + choffs);
}

static inline uint32_t ssg_registers_ch_tone_period(uint32_t choffs) {
	return m_regdata[0x00 + 2 * choffs] | (bitfield(m_regdata[0x01 + 2 * choffs], 0, 4) << 8);
}

static inline uint32_t ssg_registers_ch_envelope_enable(uint32_t choffs) {
	return bitfield(m_regdata[0x08 + choffs], 4);
}

static inline uint32_t ssg_registers_ch_amplitude(uint32_t choffs) {
	return bitfield(m_regdata[0x08 + choffs], 0, 4);
}


//-------------------------------------------------
//  reset - reset the register state
//-------------------------------------------------

void ssg_registers::reset()
{
	memset(&m_regdata[0], 0, REGISTERS);
}


//-------------------------------------------------
//  save_restore - save or restore the data
//-------------------------------------------------

void ssg_registers::save_restore(ymfm_saved_state &state)
{
	state.save_restore(m_regdata);
}



//*********************************************************
// SSG ENGINE
//*********************************************************

//-------------------------------------------------
//  ssg_engine - constructor
//-------------------------------------------------

ssg_engine::ssg_engine(ymfm_interface &intf) :
	m_intf(intf),
	m_tone_count{ 0,0,0 },
	m_tone_state{ 0,0,0 },
	m_envelope_count(0),
	m_envelope_state(0),
	m_noise_count(0),
	m_noise_state(1)
{
}


//-------------------------------------------------
//  reset - reset the engine state
//-------------------------------------------------

void ssg_engine::reset()
{
	// reset register state
	m_regs.reset();

	// reset engine state
	for (int chan = 0; chan < 3; chan++)
	{
		m_tone_count[chan] = 0;
		m_tone_state[chan] = 0;
	}
	m_envelope_count = 0;
	m_envelope_state = 0;
	m_noise_count = 0;
	m_noise_state = 1;
}


//-------------------------------------------------
//  save_restore - save or restore the data
//-------------------------------------------------

void ssg_engine::save_restore(ymfm_saved_state &state)
{
	// save register state
	m_regs.save_restore(state);

	// save engine state
	state.save_restore(m_tone_count);
	state.save_restore(m_tone_state);
	state.save_restore(m_envelope_count);
	state.save_restore(m_envelope_state);
	state.save_restore(m_noise_count);
	state.save_restore(m_noise_state);
}


//-------------------------------------------------
//  clock - master clocking function
//-------------------------------------------------

void ssg_engine::clock()
{
	// clock tones; tone period units are clock/16 but since we run at clock/8
	// that works out for us to toggle the state (50% duty cycle) at twice the
	// programmed period
	for (int chan = 0; chan < 3; chan++)
	{
		m_tone_count[chan]++;
		if (m_tone_count[chan] >= ssg_registers_ch_tone_period(chan))
		{
			m_tone_state[chan] ^= 1;
			m_tone_count[chan] = 0;
		}
	}

	// clock noise; noise period units are clock/16 but since we run at clock/8,
	// our counter needs a right shift prior to compare; note that a period of 0
	// should produce an indentical result to a period of 1, so add a special
	// check against that case
	m_noise_count++;
	if ((m_noise_count >> 1) >= ssg_registers_noise_period() && m_noise_count != 1)
	{
		m_noise_state ^= (bitfield(m_noise_state, 0) ^ bitfield(m_noise_state, 3)) << 17;
		m_noise_state >>= 1;
		m_noise_count = 0;
	}

	// clock envelope; envelope period units are clock/8 (manual says clock/256
	// but that's for all 32 steps)
	m_envelope_count++;
	if (m_envelope_count >= ssg_registers_envelope_period())
	{
		m_envelope_state++;
		m_envelope_count = 0;
	}
}


//-------------------------------------------------
//  output - output the current state
//-------------------------------------------------

void ssg_engine::output(output_data &output)
{
	// volume to amplitude table, taken from MAME's implementation but biased
	// so that 0 == 0
	static int16_t const s_amplitudes[32] =
	{
		     0,   32,   78,  141,  178,  222,  262,  306,
		   369,  441,  509,  585,  701,  836,  965, 1112,
		  1334, 1595, 1853, 2146, 2576, 3081, 3576, 4135,
		  5000, 6006, 7023, 8155, 9963,11976,14132,16382
	};

	// compute the envelope volume
	uint32_t envelope_volume;
	if ((ssg_registers_envelope_hold() | (ssg_registers_envelope_continue() ^ 1)) && m_envelope_state >= 32)
	{
		m_envelope_state = 32;
		envelope_volume = ((ssg_registers_envelope_attack() ^ ssg_registers_envelope_alternate()) & ssg_registers_envelope_continue()) ? 31 : 0;
	}
	else
	{
		uint32_t attack = ssg_registers_envelope_attack();
		if (ssg_registers_envelope_alternate())
			attack ^= bitfield(m_envelope_state, 5);
		envelope_volume = (m_envelope_state & 31) ^ (attack ? 0 : 31);
	}

	// iterate over channels
	for (int chan = 0; chan < 3; chan++)
	{
		// noise depends on the noise state, which is the LSB of m_noise_state
		uint32_t noise_on = ssg_registers_ch_noise_enable(chan) & m_noise_state;

		// tone depends on the current tone state
		uint32_t tone_on = ssg_registers_ch_tone_enable(chan) & m_tone_state[chan];

		// if neither tone nor noise enabled, return 0
		uint32_t volume;
		if ((noise_on | tone_on) == 0)
			volume = 0;

		// if the envelope is enabled, use its amplitude
		else if (ssg_registers_ch_envelope_enable(chan))
			volume = envelope_volume;

		// otherwise, scale the tone amplitude up to match envelope values
		// according to the datasheet, amplitude 15 maps to envelope 31
		else
		{
			volume = ssg_registers_ch_amplitude(chan) * 2;
			if (volume != 0)
				volume |= 1;
		}

		// convert to amplitude
		output.data[chan] = s_amplitudes[volume];
	}
}


//-------------------------------------------------
//  read - handle reads from the SSG registers
//-------------------------------------------------

uint8_t ssg_engine::read(uint32_t regnum)
{
	// read from the I/O ports call the handlers if they are configured for input
	if (regnum == 0x0e && !ssg_registers_io_a_out())
		return m_intf.ymfm_external_read(ACCESS_IO, 0);
	else if (regnum == 0x0f && !ssg_registers_io_b_out())
		return m_intf.ymfm_external_read(ACCESS_IO, 1);

	// otherwise just return the register value
	return ssg_registers_read(regnum);
}


//-------------------------------------------------
//  write - handle writes to the SSG registers
//-------------------------------------------------

void ssg_engine::write(uint32_t regnum, uint8_t data)
{
	// store the raw value to the register array;
	// most writes are passive, consumed only when needed
	ssg_registers_write(regnum, data);

	// writes to the envelope shape register reset the state
	if (regnum == 0x0d)
		m_envelope_state = 0;

	// writes to the I/O ports call the handlers if they are configured for output
	else if (regnum == 0x0e && ssg_registers_io_a_out())
		m_intf.ymfm_external_write(ACCESS_IO, 0, data);
	else if (regnum == 0x0f && ssg_registers_io_b_out())
		m_intf.ymfm_external_write(ACCESS_IO, 1, data);
}

}
