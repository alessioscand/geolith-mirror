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

#ifndef YMFM_OPN_H
#define YMFM_OPN_H

#include "ymfm.h"
#include "ymfm_adpcm.h"
#include "ymfm_fm.h"
#include "ymfm_ssg.h"

//*********************************************************
//  OPN IMPLEMENTATION CLASSES
//*********************************************************

// A note about prescaling and sample rates.
//
// YM2203, YM2608, and YM2610 contain an onboard SSG (basically, a YM2149).
// In order to properly generate sound at fully fidelity, the output sample
// rate of the YM2149 must be input_clock / 8. This is much higher than the
// FM needs, but in the interest of keeping things simple, the OPN generate
// functions will output at the higher rate and just replicate the last FM
// sample as many times as needed.
//
// To make things even more complicated, the YM2203 and YM2608 allow for
// software-controlled prescaling, which affects the FM and SSG clocks in
// different ways. There are three settings: divide by 6/4 (FM/SSG); divide
// by 3/2; and divide by 2/1.
//
// Thus, the minimum output sample rate needed by each part of the chip
// varies with the prescale as follows:
//
//             ---- YM2203 -----    ---- YM2608 -----    ---- YM2610 -----
// Prescale    FM rate  SSG rate    FM rate  SSG rate    FM rate  SSG rate
//     6         /72      /16         /144     /32          /144    /32
//     3         /36      /8          /72      /16
//     2         /24      /4          /48      /8
//
// If we standardized on the fastest SSG rate, we'd end up with the following
// (ratios are output_samples:source_samples):
//
//             ---- YM2203 -----    ---- YM2608 -----    ---- YM2610 -----
//              rate = clock/4       rate = clock/8       rate = clock/16
// Prescale    FM rate  SSG rate    FM rate  SSG rate    FM rate  SSG rate
//     6         18:1     4:1         18:1     4:1          9:1    2:1
//     3          9:1     2:1          9:1     2:1
//     2          6:1     1:1          6:1     1:1
//
// However, that's a pretty big performance hit for minimal gain. Going to
// the other extreme, we could standardize on the fastest FM rate, but then
// at least one prescale case (3) requires the FM to be smeared across two
// output samples:
//
//             ---- YM2203 -----    ---- YM2608 -----    ---- YM2610 -----
//              rate = clock/24      rate = clock/48      rate = clock/144
// Prescale    FM rate  SSG rate    FM rate  SSG rate    FM rate  SSG rate
//     6          3:1     2:3          3:1     2:3          1:1    2:9
//     3        1.5:1     1:3        1.5:1     1:3
//     2          1:1     1:6          1:1     1:6
//
// Stepping back one factor of 2 addresses that issue:
//
//             ---- YM2203 -----    ---- YM2608 -----    ---- YM2610 -----
//              rate = clock/12      rate = clock/24      rate = clock/144
// Prescale    FM rate  SSG rate    FM rate  SSG rate    FM rate  SSG rate
//     6          6:1     4:3          6:1     4:3          1:1    2:9
//     3          3:1     2:3          3:1     2:3
//     2          2:1     1:3          2:1     1:3
//
// This gives us three levels of output fidelity:
//    OPN_FIDELITY_MAX -- highest sample rate, using fastest SSG rate
//    OPN_FIDELITY_MIN -- lowest sample rate, using fastest FM rate
//    OPN_FIDELITY_MED -- medium sample rate such that FM is never smeared
//
// At the maximum clocks for YM2203/YM2608 (4Mhz/8MHz), these rates will
// end up as:
//    OPN_FIDELITY_MAX = 1000kHz
//    OPN_FIDELITY_MIN =  166kHz
//    OPN_FIEDLITY_MED =  333kHz

// ======================> opn_fidelity

enum {
	OPN_FIDELITY_MAX,
	OPN_FIDELITY_MIN,
	OPN_FIDELITY_MED,

	OPN_FIDELITY_DEFAULT = OPN_FIDELITY_MAX
};


// ======================> ym2610/ym2610b

struct ym2610
{
	#define EOS_FLAGS_MASK 0xbf

	// constructor
	ym2610(ymfm_interface &intf, uint8_t channel_mask = 0x36);

	// configuration
	void set_fidelity(uint32_t fidelity) { m_fidelity = fidelity; update_prescale(); }

	// reset
	void reset();

	// pass-through helpers
	uint32_t sample_rate(uint32_t input_clock) const
	{
		switch (m_fidelity)
		{
			case OPN_FIDELITY_MIN:	return input_clock / 144;
			case OPN_FIDELITY_MED:	return input_clock / 144;
			default:
			case OPN_FIDELITY_MAX:	return input_clock / 16;
		}
	}
	uint32_t ssg_effective_clock(uint32_t input_clock) const { return input_clock / 4; }
	void invalidate_caches() { m_fm.invalidate_caches(); }

	// read access
	uint8_t read_status();
	uint8_t read_data();
	uint8_t read_status_hi();
	uint8_t read_data_hi();
	uint8_t read(uint32_t offset);

	// write access
	void write_address(uint8_t data);
	void write_data(uint8_t data);
	void write_address_hi(uint8_t data);
	void write_data_hi(uint8_t data);
	void write(uint32_t offset, uint8_t data);

	// generate one sample of sound
	void generate(int32_t *output);

	// internal helpers
	void update_prescale();
	void clock_fm_and_adpcm();

	// internal state
	uint32_t m_fidelity;                // configured fidelity
	uint16_t m_address;                 // address register
	uint8_t const m_fm_mask;            // FM channel mask
	uint8_t m_fm_samples_per_output;    // how many samples to repeat
	uint8_t m_eos_status;               // end-of-sample signals
	uint8_t m_flag_mask;                // flag mask control
	int32_t m_last_fm[3];               // last FM output
	fm_engine m_fm;                     // core FM engine
};


// ======================> ym2612

struct ym2612
{
	// constructor
	ym2612(ymfm_interface &intf);

	// reset
	void reset();

	// pass-through helpers
	uint32_t sample_rate(uint32_t input_clock) const { return m_fm.sample_rate(input_clock); }
	void invalidate_caches() { m_fm.invalidate_caches(); }

	// read access
	uint8_t read_status();
	uint8_t read(uint32_t offset);

	// write access
	void write_address(uint8_t data);
	void write_data(uint8_t data);
	void write_address_hi(uint8_t data);
	void write_data_hi(uint8_t data);
	void write(uint32_t offset, uint8_t data);

	// generate one sample of sound
	void generate(int32_t *output);

	// generate one sample of YM3438 sound
	void generate3438(int32_t *output);

	// internal state
	uint16_t m_address;              // address register
	uint16_t m_dac_data;             // 9-bit DAC data
	uint8_t m_dac_enable;            // DAC enabled?
	fm_engine m_fm;                  // core FM engine
};


#endif // YMFM_OPN_H
