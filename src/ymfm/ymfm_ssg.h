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

#ifndef YMFM_SSG_H
#define YMFM_SSG_H

#pragma once

#include "ymfm.h"

namespace ymfm
{

//*********************************************************
//  OVERRIDE INTERFACE
//*********************************************************

// ======================> ssg_override

// this class describes a simple interface to allow the internal SSG to be
// overridden with another implementation
struct ssg_override
{
	// reset our status
	virtual void ssg_reset() = 0;

	// read/write to the SSG registers
	virtual uint8_t ssg_read(uint32_t regnum) = 0;
	virtual void ssg_write(uint32_t regnum, uint8_t data) = 0;

	// notification when the prescale has changed
	virtual void ssg_prescale_changed() = 0;
};


//*********************************************************
//  REGISTER CLASS
//*********************************************************

// ======================> ssg_registers

//
// SSG register map:
//
//      System-wide registers:
//           06 ---xxxxx Noise period
//           07 x------- I/O B in(0) or out(1)
//              -x------ I/O A in(0) or out(1)
//              --x----- Noise enable(0) or disable(1) for channel C
//              ---x---- Noise enable(0) or disable(1) for channel B
//              ----x--- Noise enable(0) or disable(1) for channel A
//              -----x-- Tone enable(0) or disable(1) for channel C
//              ------x- Tone enable(0) or disable(1) for channel B
//              -------x Tone enable(0) or disable(1) for channel A
//           0B xxxxxxxx Envelope period fine
//           0C xxxxxxxx Envelope period coarse
//           0D ----x--- Envelope shape: continue
//              -----x-- Envelope shape: attack/decay
//              ------x- Envelope shape: alternate
//              -------x Envelope shape: hold
//           0E xxxxxxxx 8-bit parallel I/O port A
//           0F xxxxxxxx 8-bit parallel I/O port B
//
//      Per-channel registers:
//     00,02,04 xxxxxxxx Tone period (fine) for channel A,B,C
//     01,03,05 ----xxxx Tone period (coarse) for channel A,B,C
//     08,09,0A ---x---- Mode: fixed(0) or variable(1) for channel A,B,C
//              ----xxxx Amplitude for channel A,B,C
//
struct ssg_registers
{
	// constants
	static constexpr uint32_t OUTPUTS = 3;
	static constexpr uint32_t CHANNELS = 3;
	static constexpr uint32_t ALL_CHANNELS = (1 << CHANNELS) - 1;

	// constructor
	ssg_registers() { }

	// reset to initial state
	void reset();

	// save/restore
	void save_restore(ymfm_saved_state &state);

	// direct read/write access
	uint8_t read(uint32_t index);
	void write(uint32_t index, uint8_t data);

	// system-wide registers
	uint32_t noise_period() const;
	uint32_t io_b_out() const;
	uint32_t io_a_out() const;
	uint32_t envelope_period() const;
	uint32_t envelope_continue() const;
	uint32_t envelope_attack() const;
	uint32_t envelope_alternate() const;
	uint32_t envelope_hold() const;
	uint32_t io_a_data() const;
	uint32_t io_b_data() const;

	// per-channel registers
	uint32_t ch_noise_enable(uint32_t choffs) const;
	uint32_t ch_tone_enable(uint32_t choffs) const;
	uint32_t ch_tone_period(uint32_t choffs) const;
	uint32_t ch_envelope_enable(uint32_t choffs) const;
	uint32_t ch_amplitude(uint32_t choffs) const;
};


// ======================> ssg_engine

struct ssg_engine
{
	static constexpr int OUTPUTS = ssg_registers::OUTPUTS;
	static constexpr int CHANNELS = ssg_registers::CHANNELS;
	static constexpr int CLOCK_DIVIDER = 8;

	using output_data = ymfm_output<OUTPUTS>;

	// constructor
	ssg_engine(ymfm_interface &intf);

	// configure an override
	void override(ssg_override &override) { m_override = &override; }

	// reset our status
	void reset();

	// save/restore
	void save_restore(ymfm_saved_state &state);

	// master clocking function
	void clock();

	// compute sum of channel outputs
	void output(output_data &output);

	// read/write to the SSG registers
	uint8_t read(uint32_t regnum);
	void write(uint32_t regnum, uint8_t data);

	// return a reference to our interface
	ymfm_interface &intf() { return m_intf; }

	// return a reference to our registers
	ssg_registers &regs() { return m_regs; }

	// true if we are overridden
	bool overridden() const { return (m_override != nullptr); }

	// indicate the prescale has changed
	void prescale_changed() { if (m_override != nullptr) m_override->ssg_prescale_changed(); }

	// internal state
	ymfm_interface &m_intf;                   // reference to the interface
	uint32_t m_tone_count[3];               // current tone counter
	uint32_t m_tone_state[3];               // current tone state
	uint32_t m_envelope_count;              // envelope counter
	uint32_t m_envelope_state;              // envelope state
	uint32_t m_noise_count;                 // current noise counter
	uint32_t m_noise_state;                 // current noise state
	ssg_registers m_regs;                   // registers
	ssg_override *m_override;               // override interface
};

}

#endif // YMFM_SSG_H
