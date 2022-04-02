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

#include <string.h>

#include "ymfm_opn.h"
#include "ymfm_fm.ipp"

#define STATUS_EOS 0x01 // FIXME: This is defined twice (ADPCM B)

// ======================> ssg_resampler
static void (*ssg_resampler_resample)(int32_t*);
static uint32_t m_ssg_resampler_sampindex;
static int32_t m_ssg_resampler_last[3];

// simulate the DAC discontinuity (YM2612)
static inline int32_t dac_discontinuity(int32_t value)
{
	return (value < 0) ? (value - 2) : (value + 3);
}

// helper to encode four operator numbers into a 32-bit value in the
// operator maps for each register class
static inline uint32_t operator_list(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4)
{
	return o1 | (o2 << 8) | (o3 << 16) | (o4 << 24);
}

// helper to apply KSR to the raw ADSR rate, ignoring ksr if the
// raw value is 0, and clamping to 63
static inline uint32_t effective_rate(uint32_t rawrate, uint32_t ksr)
{
	return (rawrate == 0) ? 0 : std::min<uint32_t>(rawrate + ksr, 63);
}

namespace ymfm
{

//*********************************************************
//  OPN/OPNA REGISTERS
//*********************************************************

//-------------------------------------------------
//  opn_registers_base - constructor
//-------------------------------------------------

opn_registers_base::opn_registers_base() :
	m_lfo_counter(0),
	m_lfo_am(0)
{
	// create the waveforms
	for (uint32_t index = 0; index < WAVEFORM_LENGTH; index++)
		m_waveform[0][index] = abs_sin_attenuation(index) | (bitfield(index, 9) << 15);
}


//-------------------------------------------------
//  reset - reset to initial state
//-------------------------------------------------

void opn_registers_base::reset()
{
	memset(&m_regdata[0], 0, REGISTERS);
	// enable output on both channels by default
	m_regdata[0xb4] = m_regdata[0xb5] = m_regdata[0xb6] = 0xc0;
	m_regdata[0x1b4] = m_regdata[0x1b5] = m_regdata[0x1b6] = 0xc0;
}


//-------------------------------------------------
//  operator_map - return an array of operator
//  indices for each channel; for OPN this is fixed
//-------------------------------------------------

void opn_registers_base::operator_map(operator_mapping &dest) const
{
	// Note that the channel index order is 0,2,1,3, so we bitswap the index.
	//
	// This is because the order in the map is:
	//    carrier 1, carrier 2, modulator 1, modulator 2
	//
	// But when wiring up the connections, the more natural order is:
	//    carrier 1, modulator 1, carrier 2, modulator 2
	static const operator_mapping s_fixed_map =
	{ {
		operator_list(  0,  6,  3,  9 ),  // Channel 0 operators
		operator_list(  1,  7,  4, 10 ),  // Channel 1 operators
		operator_list(  2,  8,  5, 11 ),  // Channel 2 operators
		operator_list( 12, 18, 15, 21 ),  // Channel 3 operators
		operator_list( 13, 19, 16, 22 ),  // Channel 4 operators
		operator_list( 14, 20, 17, 23 ),  // Channel 5 operators
	} };
	dest = s_fixed_map;
}


//-------------------------------------------------
//  write - handle writes to the register array
//-------------------------------------------------

bool opn_registers_base::write(uint16_t index, uint8_t data, uint32_t &channel, uint32_t &opmask)
{
	assert(index < REGISTERS);

	// writes in the 0xa0-af/0x1a0-af region are handled as latched pairs
	// borrow unused registers 0xb8-bf/0x1b8-bf as temporary holding locations
	if ((index & 0xf0) == 0xa0)
	{
		uint32_t latchindex = 0xb8 | (bitfield(index, 3) << 2) | bitfield(index, 0, 2);
		latchindex |= index & 0x100;

		// writes to the upper half just latch (only low 6 bits matter)
		if (bitfield(index, 2))
			m_regdata[latchindex] = data | 0x80;

		// writes to the lower half only commit if the latch is there
		else if (bitfield(m_regdata[latchindex], 7))
		{
			m_regdata[index | 4] = m_regdata[latchindex] & 0x3f;
			m_regdata[latchindex] = 0;
		}
	}

	// everything else is normal
	m_regdata[index] = data;

	// handle writes to the key on index
	if (index == 0x28)
	{
		channel = bitfield(data, 0, 2);
		if (channel == 3)
			return false;

		channel += bitfield(data, 2, 1) * 3;
		opmask = bitfield(data, 4, 4);
		return true;
	}
	return false;
}


//-------------------------------------------------
//  clock_noise_and_lfo - clock the noise and LFO,
//  handling clock division, depth, and waveform
//  computations
//-------------------------------------------------

int32_t opn_registers_base::clock_noise_and_lfo()
{
	// OPN has no noise generation

	// if LFO not enabled (not present on OPN), quick exit with 0s
	if (!lfo_enable())
	{
		m_lfo_counter = 0;
		m_lfo_am = 0;
		return 0;
	}

	// this table is based on converting the frequencies in the applications
	// manual to clock dividers, based on the assumption of a 7-bit LFO value
	static uint8_t const lfo_max_count[8] = { 109, 78, 72, 68, 63, 45, 9, 6 };
	uint32_t subcount = uint8_t(m_lfo_counter++);

	// when we cross the divider count, add enough to zero it and cause an
	// increment at bit 8; the 7-bit value lives from bits 8-14
	if (subcount >= lfo_max_count[lfo_rate()])
	{
		// note: to match the published values this should be 0x100 - subcount;
		// however, tests on the hardware and nuked bear out an off-by-one
		// error exists that causes the max LFO rate to be faster than published
		m_lfo_counter += 0x101 - subcount;
	}

	// AM value is 7 bits, staring at bit 8; grab the low 6 directly
	m_lfo_am = bitfield(m_lfo_counter, 8, 6);

	// first half of the AM period (bit 6 == 0) is inverted
	if (bitfield(m_lfo_counter, 8+6) == 0)
		m_lfo_am ^= 0x3f;

	// PM value is 5 bits, starting at bit 10; grab the low 3 directly
	int32_t pm = bitfield(m_lfo_counter, 10, 3);

	// PM is reflected based on bit 3
	if (bitfield(m_lfo_counter, 10+3))
		pm ^= 7;

	// PM is negated based on bit 4
	return bitfield(m_lfo_counter, 10+4) ? -pm : pm;
}


//-------------------------------------------------
//  lfo_am_offset - return the AM offset from LFO
//  for the given channel
//-------------------------------------------------

uint32_t opn_registers_base::lfo_am_offset(uint32_t choffs) const
{
	// shift value for AM sensitivity is [7, 3, 1, 0],
	// mapping to values of [0, 1.4, 5.9, and 11.8dB]
	uint32_t am_shift = (1 << (ch_lfo_am_sens(choffs) ^ 3)) - 1;

	// QUESTION: max sensitivity should give 11.8dB range, but this value
	// is directly added to an x.8 attenuation value, which will only give
	// 126/256 or ~4.9dB range -- what am I missing? The calculation below
	// matches several other emulators, including the Nuked implemenation.

	// raw LFO AM value on OPN is 0-3F, scale that up by a factor of 2
	// (giving 7 bits) before applying the final shift
	return (m_lfo_am << 1) >> am_shift;
}


//-------------------------------------------------
//  cache_operator_data - fill the operator cache
//  with prefetched data
//-------------------------------------------------

void opn_registers_base::cache_operator_data(uint32_t choffs, uint32_t opoffs, opdata_cache &cache)
{
	// set up the easy stuff
	cache.waveform = &m_waveform[0][0];

	// get frequency from the channel
	uint32_t block_freq = cache.block_freq = ch_block_freq(choffs);

	// if multi-frequency mode is enabled and this is channel 2,
	// fetch one of the special frequencies
	if (multi_freq() && choffs == 2)
	{
		if (opoffs == 2)
			block_freq = cache.block_freq = multi_block_freq(1);
		else if (opoffs == 10)
			block_freq = cache.block_freq = multi_block_freq(2);
		else if (opoffs == 6)
			block_freq = cache.block_freq = multi_block_freq(0);
	}

	// compute the keycode: block_freq is:
	//
	//     BBBFFFFFFFFFFF
	//     ^^^^???
	//
	// the 5-bit keycode uses the top 4 bits plus a magic formula
	// for the final bit
	uint32_t keycode = bitfield(block_freq, 10, 4) << 1;

	// lowest bit is determined by a mix of next lower FNUM bits
	// according to this equation from the YM2608 manual:
	//
	//   (F11 & (F10 | F9 | F8)) | (!F11 & F10 & F9 & F8)
	//
	// for speed, we just look it up in a 16-bit constant
	keycode |= bitfield(0xfe80, bitfield(block_freq, 7, 4));

	// detune adjustment
	cache.detune = detune_adjustment(op_detune(opoffs), keycode);

	// multiple value, as an x.1 value (0 means 0.5)
	cache.multiple = op_multiple(opoffs) * 2;
	if (cache.multiple == 0)
		cache.multiple = 1;

	// phase step, or PHASE_STEP_DYNAMIC if PM is active; this depends on
	// block_freq, detune, and multiple, so compute it after we've done those
	if (lfo_enable() == 0 || ch_lfo_pm_sens(choffs) == 0)
		cache.phase_step = compute_phase_step(choffs, opoffs, cache, 0);
	else
		cache.phase_step = PHASE_STEP_DYNAMIC;

	// total level, scaled by 8
	cache.total_level = op_total_level(opoffs) << 3;

	// 4-bit sustain level, but 15 means 31 so effectively 5 bits
	cache.eg_sustain = op_sustain_level(opoffs);
	cache.eg_sustain |= (cache.eg_sustain + 1) & 0x10;
	cache.eg_sustain <<= 5;

	// determine KSR adjustment for enevlope rates
	uint32_t ksrval = keycode >> (op_ksr(opoffs) ^ 3);
	cache.eg_rate[EG_ATTACK] = effective_rate(op_attack_rate(opoffs) * 2, ksrval);
	cache.eg_rate[EG_DECAY] = effective_rate(op_decay_rate(opoffs) * 2, ksrval);
	cache.eg_rate[EG_SUSTAIN] = effective_rate(op_sustain_rate(opoffs) * 2, ksrval);
	cache.eg_rate[EG_RELEASE] = effective_rate(op_release_rate(opoffs) * 4 + 2, ksrval);
}


//-------------------------------------------------
//  compute_phase_step - compute the phase step
//-------------------------------------------------

uint32_t opn_registers_base::compute_phase_step(uint32_t choffs, uint32_t opoffs, opdata_cache const &cache, int32_t lfo_raw_pm)
{
	// OPN phase calculation has only a single detune parameter
	// and uses FNUMs instead of keycodes

	// extract frequency number (low 11 bits of block_freq)
	uint32_t fnum = bitfield(cache.block_freq, 0, 11) << 1;

	// if there's a non-zero PM sensitivity, compute the adjustment
	uint32_t pm_sensitivity = ch_lfo_pm_sens(choffs);
	if (pm_sensitivity != 0)
	{
		// apply the phase adjustment based on the upper 7 bits
		// of FNUM and the PM depth parameters
		fnum += opn_lfo_pm_phase_adjustment(bitfield(cache.block_freq, 4, 7), pm_sensitivity, lfo_raw_pm);

		// keep fnum to 12 bits
		fnum &= 0xfff;
	}

	// apply block shift to compute phase step
	uint32_t block = bitfield(cache.block_freq, 11, 3);
	uint32_t phase_step = (fnum << block) >> 2;

	// apply detune based on the keycode
	phase_step += cache.detune;

	// clamp to 17 bits in case detune overflows
	// QUESTION: is this specific to the YM2612/3438?
	phase_step &= 0x1ffff;

	// apply frequency multiplier (which is cached as an x.1 value)
	return (phase_step * cache.multiple) >> 1;
}


//-------------------------------------------------
//  log_keyon - log a key-on event
//-------------------------------------------------

std::string opn_registers_base::log_keyon(uint32_t choffs, uint32_t opoffs)
{
	uint32_t chnum = (choffs & 3) + 3 * bitfield(choffs, 8);
	uint32_t opnum = (opoffs & 15) - ((opoffs & 15) / 4) + 12 * bitfield(opoffs, 8);

	uint32_t block_freq = ch_block_freq(choffs);
	if (multi_freq() && choffs == 2)
	{
		if (opoffs == 2)
			block_freq = multi_block_freq(1);
		else if (opoffs == 10)
			block_freq = multi_block_freq(2);
		else if (opoffs == 6)
			block_freq = multi_block_freq(0);
	}

	char buffer[256];
	char *end = &buffer[0];

	end += sprintf(end, "%u.%02u freq=%04X dt=%u fb=%u alg=%X mul=%X tl=%02X ksr=%u adsr=%02X/%02X/%02X/%X sl=%X",
		chnum, opnum,
		block_freq,
		op_detune(opoffs),
		ch_feedback(choffs),
		ch_algorithm(choffs),
		op_multiple(opoffs),
		op_total_level(opoffs),
		op_ksr(opoffs),
		op_attack_rate(opoffs),
		op_decay_rate(opoffs),
		op_sustain_rate(opoffs),
		op_release_rate(opoffs),
		op_sustain_level(opoffs));

	if (OUTPUTS > 1)
		end += sprintf(end, " out=%c%c",
			ch_output_0(choffs) ? 'L' : '-',
			ch_output_1(choffs) ? 'R' : '-');
	if (op_ssg_eg_enable(opoffs))
		end += sprintf(end, " ssg=%X", op_ssg_eg_mode(opoffs));
	bool am = (lfo_enable() && op_lfo_am_enable(opoffs) && ch_lfo_am_sens(choffs) != 0);
	if (am)
		end += sprintf(end, " am=%u", ch_lfo_am_sens(choffs));
	bool pm = (lfo_enable() && ch_lfo_pm_sens(choffs) != 0);
	if (pm)
		end += sprintf(end, " pm=%u", ch_lfo_pm_sens(choffs));
	if (am || pm)
		end += sprintf(end, " lfo=%02X", lfo_rate());
	if (multi_freq() && choffs == 2)
		end += sprintf(end, " multi=1");

	return buffer;
}
}


//*********************************************************
//  SSG RESAMPLER
//*********************************************************

//-------------------------------------------------
//  add_last - helper to add the last computed
//  value to the sums, applying the given scale
//-------------------------------------------------

static inline void ssg_resampler_add_last(int32_t &sum0, int32_t &sum1, int32_t &sum2, int32_t scale)
{
	sum0 += m_ssg_resampler_last[0] * scale;
	sum1 += m_ssg_resampler_last[1] * scale;
	sum2 += m_ssg_resampler_last[2] * scale;
}


//-------------------------------------------------
//  clock_and_add - helper to clock a new value
//  and then add it to the sums, applying the
//  given scale
//-------------------------------------------------

static inline void ssg_resampler_clock_and_add(int32_t &sum0, int32_t &sum1, int32_t &sum2, int32_t scale)
{
	ssg_engine_clock();
	ssg_engine_output(m_ssg_resampler_last);
	ssg_resampler_add_last(sum0, sum1, sum2, scale);
}


//-------------------------------------------------
//  write_to_output - helper to write the sums to
//  the appropriate outputs, applying the given
//  divisor to the final result
//-------------------------------------------------

static inline void ssg_resampler_write_to_output(int32_t *output, int32_t sum0, int32_t sum1, int32_t sum2, int32_t divisor)
{
	// mixing to one, apply a 2/3 factor to prevent overflow
	output[2] = (sum0 + sum1 + sum2) * 2 / (3 * divisor);

	// track the sample index here
	m_ssg_resampler_sampindex++;
}


//-------------------------------------------------
//  resample_2_1 - resample SSG output to the
//  target at a rate of 1 SSG sample to every
//  2 output samples
//-------------------------------------------------

static void ssg_resampler_resample_2_1(int32_t *output)
{
	if (m_ssg_resampler_sampindex % 2 == 0)
	{
		ssg_engine_clock();
		ssg_engine_output(m_ssg_resampler_last);
	}
	ssg_resampler_write_to_output(output, m_ssg_resampler_last[0],
		m_ssg_resampler_last[1], m_ssg_resampler_last[2], 1);
}


//-------------------------------------------------
//  resample_2_9 - resample SSG output to the
//  target at a rate of 9 SSG samples to every
//  2 output samples
//-------------------------------------------------

static void ssg_resampler_resample_2_9(int32_t *output)
{
	int32_t sum0 = 0, sum1 = 0, sum2 = 0;
	if (bitfield(m_ssg_resampler_sampindex, 0) != 0)
		ssg_resampler_add_last(sum0, sum1, sum2, 1);
	ssg_resampler_clock_and_add(sum0, sum1, sum2, 2);
	ssg_resampler_clock_and_add(sum0, sum1, sum2, 2);
	ssg_resampler_clock_and_add(sum0, sum1, sum2, 2);
	ssg_resampler_clock_and_add(sum0, sum1, sum2, 2);
	if (bitfield(m_ssg_resampler_sampindex, 0) == 0)
		ssg_resampler_clock_and_add(sum0, sum1, sum2, 1);
	ssg_resampler_write_to_output(output, sum0, sum1, sum2, 9);
}


//-------------------------------------------------
//  resample_nop - no-op resampler
//-------------------------------------------------

static void ssg_resampler_resample_nop(int32_t *output)
{
	// nothing to do except increment the sample index
	++m_ssg_resampler_sampindex;
}


//-------------------------------------------------
//  ssg_resampler - init
//-------------------------------------------------

static inline void ssg_resampler_init(void)
{
	m_ssg_resampler_sampindex = 0;
	ssg_resampler_resample = &ssg_resampler_resample_nop;
	m_ssg_resampler_last[0] = m_ssg_resampler_last[1] = m_ssg_resampler_last[2] = 0;
}


//-------------------------------------------------
//  configure - configure a new ratio
//-------------------------------------------------

static inline void ssg_resampler_configure(uint8_t outsamples, uint8_t srcsamples)
{
	switch (outsamples * 10 + srcsamples)
	{
		case 2*10 + 1:	/* 2:1 */	ssg_resampler_resample = &ssg_resampler_resample_2_1; break;
		case 2*10 + 9:	/* 2:9 */	ssg_resampler_resample = &ssg_resampler_resample_2_9; break;
		default: assert(false); break;
	}
}


namespace ymfm {

//*********************************************************
//  YM2610
//*********************************************************

//-------------------------------------------------
//  ym2610 - constructor
//-------------------------------------------------

ym2610::ym2610(ymfm_interface &intf, uint8_t channel_mask) :
	m_fidelity(OPN_FIDELITY_MAX),
	m_address(0),
	m_fm_mask(channel_mask),
	m_eos_status(0x00),
	m_flag_mask(EOS_FLAGS_MASK),
	m_fm(intf)
{
	ssg_engine_init();
	ssg_resampler_init();
	adpcm_a_engine_init();
	adpcm_b_engine_init();
	update_prescale();
}


//-------------------------------------------------
//  reset - reset the system
//-------------------------------------------------

void ym2610::reset()
{
	// reset the engines
	m_fm.reset();
	ssg_engine_reset();
	adpcm_a_engine_reset();
	adpcm_b_engine_reset();

	// initialize our special interrupt states
	m_eos_status = 0x00;
	m_flag_mask = EOS_FLAGS_MASK;
}


//-------------------------------------------------
//  read_status - read the status register
//-------------------------------------------------

uint8_t ym2610::read_status()
{
	uint8_t result = m_fm.status() & (fm_engine::STATUS_TIMERA | fm_engine::STATUS_TIMERB);
	if (ymfm_is_busy())
		result |= fm_engine::STATUS_BUSY;
	return result;
}


//-------------------------------------------------
//  read_data - read the data register
//-------------------------------------------------

uint8_t ym2610::read_data()
{
	uint8_t result = 0;
	if (m_address < 0x0e)
	{
		// 00-0D: Read from SSG
		result = ssg_engine_read(m_address & 0x0f);
	}
	else if (m_address < 0x10)
	{
		// 0E-0F: I/O ports not supported
		result = 0xff;
	}
	else if (m_address == 0xff)
	{
		// FF: ID code
		result = 1;
	}
	return result;
}


//-------------------------------------------------
//  read_status_hi - read the extended status
//  register
//-------------------------------------------------

uint8_t ym2610::read_status_hi()
{
	return m_eos_status & m_flag_mask;
}


//-------------------------------------------------
//  read_data_hi - read the upper data register
//-------------------------------------------------

uint8_t ym2610::read_data_hi()
{
	uint8_t result = 0;
	return result;
}


//-------------------------------------------------
//  read - handle a read from the device
//-------------------------------------------------

uint8_t ym2610::read(uint32_t offset)
{
	uint8_t result = 0;
	switch (offset & 3)
	{
		case 0: // status port, YM2203 compatible
			result = read_status();
			break;

		case 1: // data port (only SSG)
			result = read_data();
			break;

		case 2: // status port, extended
			result = read_status_hi();
			break;

		case 3: // ADPCM-B data
			result = read_data_hi();
			break;
	}
	return result;
}


//-------------------------------------------------
//  write_address - handle a write to the address
//  register
//-------------------------------------------------

void ym2610::write_address(uint8_t data)
{
	// just set the address
	m_address = data;
}


//-------------------------------------------------
//  write - handle a write to the data register
//-------------------------------------------------

void ym2610::write_data(uint8_t data)
{
	// ignore if paired with upper address
	if (bitfield(m_address, 8))
		return;

	if (m_address < 0x0e)
	{
		// 00-0D: write to SSG
		ssg_engine_write(m_address & 0x0f, data);
	}
	else if (m_address < 0x10)
	{
		// 0E-0F: I/O ports not supported
	}
	else if (m_address < 0x1c)
	{
		// 10-1B: write to ADPCM-B
		// YM2610 effectively forces external mode on, and disables recording
		if (m_address == 0x10)
			data = (data | 0x20) & ~0x40;
		adpcm_b_engine_write(m_address & 0x0f, data);
	}
	else if (m_address == 0x1c)
	{
		// 1C: EOS flag reset
		m_flag_mask = ~data & EOS_FLAGS_MASK;
		m_eos_status &= ~(data & EOS_FLAGS_MASK);
	}
	else
	{
		// 1D-FF: write to FM
		m_fm.write(m_address, data);
	}

	// mark busy for a bit
	ymfm_set_busy_end(32 * m_fm.clock_prescale());
}


//-------------------------------------------------
//  write_address_hi - handle a write to the upper
//  address register
//-------------------------------------------------

void ym2610::write_address_hi(uint8_t data)
{
	// just set the address
	m_address = 0x100 | data;
}


//-------------------------------------------------
//  write_data_hi - handle a write to the upper
//  data register
//-------------------------------------------------

void ym2610::write_data_hi(uint8_t data)
{
	// ignore if paired with upper address
	if (!bitfield(m_address, 8))
		return;

	if (m_address < 0x130)
	{
		// 100-12F: write to ADPCM-A
		adpcm_a_engine_write(m_address & 0x3f, data);
	}
	else
	{
		// 130-1FF: write to FM
		m_fm.write(m_address, data);
	}

	// mark busy for a bit
	ymfm_set_busy_end(32 * m_fm.clock_prescale());
}


//-------------------------------------------------
//  write - handle a write to the register
//  interface
//-------------------------------------------------

void ym2610::write(uint32_t offset, uint8_t data)
{
	switch (offset & 3)
	{
		case 0: // address port
			write_address(data);
			break;

		case 1: // data port
			write_data(data);
			break;

		case 2: // upper address port
			write_address_hi(data);
			break;

		case 3: // upper data port
			write_data_hi(data);
			break;
	}
}


//-------------------------------------------------
//  generate - generate one sample of sound
//-------------------------------------------------

void ym2610::generate(int32_t *output)
{
	// FM output is just repeated the prescale number of times
	if (m_ssg_resampler_sampindex % m_fm_samples_per_output == 0)
		clock_fm_and_adpcm();
	output[0] = m_last_fm[0];
	output[1] = m_last_fm[1];

	// resample the SSG as configured
	ssg_resampler_resample(output);
}


//-------------------------------------------------
//  update_prescale - update the prescale value,
//  recomputing derived values
//-------------------------------------------------

void ym2610::update_prescale()
{
	// Fidelity:   ---- minimum ----    ---- medium -----    ---- maximum-----
	//              rate = clock/144     rate = clock/144     rate = clock/16
	// Prescale    FM rate  SSG rate    FM rate  SSG rate    FM rate  SSG rate
	//     6          1:1     2:9          1:1     2:9         9:1     2:1

	// compute the number of FM samples per output sample, and select the
	// resampler function
	if (m_fidelity == OPN_FIDELITY_MIN || m_fidelity == OPN_FIDELITY_MED)
	{
		m_fm_samples_per_output = 1;
		ssg_resampler_configure(2, 9);
	}
	else
	{
		m_fm_samples_per_output = 9;
		ssg_resampler_configure(2, 1);
	}
}


//-------------------------------------------------
//  clock_fm_and_adpcm - clock FM and ADPCM state
//-------------------------------------------------

void ym2610::clock_fm_and_adpcm()
{
	// clock the system
	uint32_t env_counter = m_fm.clock(m_fm_mask);

	// clock the ADPCM-A engine on every envelope cycle
	if (bitfield(env_counter, 0, 2) == 0)
		m_eos_status |= adpcm_a_engine_clock(0x3f);

	// clock the ADPCM-B engine every cycle
	adpcm_b_engine_clock();

	// we track the last ADPCM-B EOS value in bit 6 (which is hidden from callers);
	// if it changed since the last sample, update the visible EOS state in bit 7
	uint8_t live_eos = ((adpcm_b_engine_status() & STATUS_EOS) != 0) ? 0x40 : 0x00;
	if (((live_eos ^ m_eos_status) & 0x40) != 0)
		m_eos_status = (m_eos_status & ~0xc0) | live_eos | (live_eos << 1);

	// update the FM content; OPNB is 13-bit with no intermediate clipping
	m_last_fm[0] = m_last_fm[1] = m_last_fm[2] = 0;
	m_fm.output(m_last_fm, 1, 32767, m_fm_mask);

	// mix in the ADPCM and clamp
	adpcm_a_engine_output(m_last_fm, 0x3f);
	adpcm_b_engine_output(m_last_fm, 1);

	m_last_fm[0] = clamp(m_last_fm[0], -32768, 32767);
	m_last_fm[1] = clamp(m_last_fm[1], -32768, 32767);
	m_last_fm[2] = clamp(m_last_fm[2], -32768, 32767);
}



//*********************************************************
//  YM2612
//*********************************************************

//-------------------------------------------------
//  ym2612 - constructor
//-------------------------------------------------

ym2612::ym2612(ymfm_interface &intf) :
	m_address(0),
	m_dac_data(0),
	m_dac_enable(0),
	m_fm(intf)
{
}


//-------------------------------------------------
//  reset - reset the system
//-------------------------------------------------

void ym2612::reset()
{
	// reset the engines
	m_fm.reset();
}


//-------------------------------------------------
//  read_status - read the status register
//-------------------------------------------------

uint8_t ym2612::read_status()
{
	uint8_t result = m_fm.status();
	if (ymfm_is_busy())
		result |= fm_engine::STATUS_BUSY;
	return result;
}


//-------------------------------------------------
//  read - handle a read from the device
//-------------------------------------------------

uint8_t ym2612::read(uint32_t offset)
{
	uint8_t result = 0;
	switch (offset & 3)
	{
		case 0: // status port, YM2203 compatible
			result = read_status();
			break;

		case 1: // data port (unused)
		case 2: // status port, extended
		case 3: // data port (unused)
			//debug::log_unexpected_read_write("Unexpected read from YM2612 offset %d\n", offset & 3);
			break;
	}
	return result;
}


//-------------------------------------------------
//  write_address - handle a write to the address
//  register
//-------------------------------------------------

void ym2612::write_address(uint8_t data)
{
	// just set the address
	m_address = data;
}


//-------------------------------------------------
//  write_data - handle a write to the data
//  register
//-------------------------------------------------

void ym2612::write_data(uint8_t data)
{
	// ignore if paired with upper address
	if (bitfield(m_address, 8))
		return;

	if (m_address == 0x2a)
	{
		// 2A: DAC data (most significant 8 bits)
		m_dac_data = (m_dac_data & ~0x1fe) | ((data ^ 0x80) << 1);
	}
	else if (m_address == 0x2b)
	{
		// 2B: DAC enable (bit 7)
		m_dac_enable = bitfield(data, 7);
	}
	else if (m_address == 0x2c)
	{
		// 2C: test/low DAC bit
		m_dac_data = (m_dac_data & ~1) | bitfield(data, 3);
	}
	else
	{
		// 00-29, 2D-FF: write to FM
		m_fm.write(m_address, data);
	}

	// mark busy for a bit
	ymfm_set_busy_end(32 * m_fm.clock_prescale());
}


//-------------------------------------------------
//  write_address_hi - handle a write to the upper
//  address register
//-------------------------------------------------

void ym2612::write_address_hi(uint8_t data)
{
	// just set the address
	m_address = 0x100 | data;
}


//-------------------------------------------------
//  write_data_hi - handle a write to the upper
//  data register
//-------------------------------------------------

void ym2612::write_data_hi(uint8_t data)
{
	// ignore if paired with upper address
	if (!bitfield(m_address, 8))
		return;

	// 100-1FF: write to FM
	m_fm.write(m_address, data);

	// mark busy for a bit
	ymfm_set_busy_end(32 * m_fm.clock_prescale());
}


//-------------------------------------------------
//  write - handle a write to the register
//  interface
//-------------------------------------------------

void ym2612::write(uint32_t offset, uint8_t data)
{
	switch (offset & 3)
	{
		case 0: // address port
			write_address(data);
			break;

		case 1: // data port
			write_data(data);
			break;

		case 2: // upper address port
			write_address_hi(data);
			break;

		case 3: // upper data port
			write_data_hi(data);
			break;
	}
}


//-------------------------------------------------
//  generate - generate one sample of sound
//-------------------------------------------------

void ym2612::generate(int32_t *output)
{
	// clock the system
	m_fm.clock(fm_engine::ALL_CHANNELS);

	// sum individual channels to apply DAC discontinuity on each
	output[0] = output[1] = 0;
	int32_t temp[2];

	// first do FM-only channels; OPN2 is 9-bit with intermediate clipping
	int const last_fm_channel = m_dac_enable ? 5 : 6;
	for (int chan = 0; chan < last_fm_channel; chan++)
	{
		temp[0] = temp[1] = 0;
		m_fm.output(temp, 5, 256, 1 << chan);
		output[0] += dac_discontinuity(temp[0]);
		output[1] += dac_discontinuity(temp[1]);
	}

	// add in DAC
	if (m_dac_enable)
	{
		// DAC enabled: start with DAC value then add the first 5 channels only
		int32_t dacval = dac_discontinuity(int16_t(m_dac_data << 7) >> 7);
		output[0] += m_fm.regs().ch_output_0(0x102) ? dacval : dac_discontinuity(0);
		output[1] += m_fm.regs().ch_output_1(0x102) ? dacval : dac_discontinuity(0);
	}

	// output is technically multiplexed rather than mixed, but that requires
	// a better sound mixer than we usually have, so just average over the six
	// channels; also apply a 64/65 factor to account for the discontinuity
	// adjustment above
	output[0] = (output[0] * 128) * 64 / (6 * 65);
	output[1] = (output[1] * 128) * 64 / (6 * 65);
}


//-------------------------------------------------
//  generate - generate one sample of YM3438 sound
//-------------------------------------------------

void ym2612::generate3438(int32_t *output)
{
	// clock the system
	m_fm.clock(fm_engine::ALL_CHANNELS);

	// first do FM-only channels; OPN2C is 9-bit with intermediate clipping
	if (!m_dac_enable)
	{
		// DAC disabled: all 6 channels sum together
		output[0] = output[1] = 0;
		m_fm.output(output, 5, 256, fm_engine::ALL_CHANNELS);
	}
	else
	{
		// DAC enabled: start with DAC value then add the first 5 channels only
		int32_t dacval = int16_t(m_dac_data << 7) >> 7;
		output[0] = m_fm.regs().ch_output_0(0x102) ? dacval : 0;
		output[1] = m_fm.regs().ch_output_1(0x102) ? dacval : 0;
		m_fm.output(output, 5, 256, fm_engine::ALL_CHANNELS ^ (1 << 5));
	}

	// YM3438 doesn't have the same DAC discontinuity, though its output is
	// multiplexed like the YM2612
	output[0] = (output[0] * 128) / 6;
	output[1] = (output[1] * 128) / 6;
}

}
