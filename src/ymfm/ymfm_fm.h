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

#ifndef YMFM_FM_H
#define YMFM_FM_H

// set phase_step to this value to recalculate it each sample; needed
// in the case of PM LFO changes
#define PHASE_STEP_DYNAMIC 1

// OPN Register defines
// this value is returned from the write() function for rhythm channels
#define RHYTHM_CHANNEL 0xff

// this is the size of a full sin waveform
#define WAVEFORM_LENGTH 0x400

// the following constants need to be defined per family:
//          uint32_t OUTPUTS: The number of outputs exposed (1-4)
//         uint32_t CHANNELS: The number of channels on the chip
//     uint32_t ALL_CHANNELS: A bitmask of all channels
//        uint32_t OPERATORS: The number of operators on the chip
//        uint32_t WAVEFORMS: The number of waveforms offered
//        uint32_t REGISTERS: The number of 8-bit registers allocated
// uint32_t DEFAULT_PRESCALE: The starting clock prescale
// uint32_t EG_CLOCK_DIVIDER: The clock divider of the envelope generator
// uint32_t CSM_TRIGGER_MASK: Mask of channels to trigger in CSM mode
//         uint32_t REG_MODE: The address of the "mode" register controlling timers
//     uint8_t STATUS_TIMERA: Status bit to set when timer A fires
//     uint8_t STATUS_TIMERB: Status bit to set when tiemr B fires
//       uint8_t STATUS_BUSY: Status bit to set when the chip is busy
//        uint8_t STATUS_IRQ: Status bit to set when an IRQ is signalled
//
// the following constants are uncommon:
//          bool DYNAMIC_OPS: True if ops/channel can be changed at runtime (OPL3+)
//       bool EG_HAS_DEPRESS: True if the chip has a DP ("depress"?) envelope stage (OPLL)
//        bool EG_HAS_REVERB: True if the chip has a faux reverb envelope stage (OPQ/OPZ)
//           bool EG_HAS_SSG: True if the chip has SSG envelope support (OPN)
//      bool MODULATOR_DELAY: True if the modulator is delayed by 1 sample (OPL pre-OPL3)
//
#define OUTPUTS 2
#define CHANNELS 6
#define ALL_CHANNELS 0x3f
#define OPERATORS 24
#define STATUS_TIMERA 0x01
#define STATUS_TIMERB 0x02
#define STATUS_BUSY 0x80
#define STATUS_IRQ 0
#define WAVEFORMS 1
#define REGISTERS 0x200
#define REG_MODE 0x27
#define DEFAULT_PRESCALE 6
#define DYNAMIC_OPS false
#define EG_CLOCK_DIVIDER 3
#define EG_HAS_DEPRESS false
#define EG_HAS_REVERB false
#define EG_HAS_SSG true
#define MODULATOR_DELAY false
#define CSM_TRIGGER_MASK 4

//*********************************************************
//  GLOBAL ENUMERATORS
//*********************************************************

// three different keyon sources; actual keyon is an OR over all of these
enum {
	KEYON_NORMAL = 0,
	KEYON_RHYTHM = 1,
	KEYON_CSM = 2
};


//*********************************************************
//  CORE IMPLEMENTATION
//*********************************************************

// ======================> opdata_cache

// this class holds data that is computed once at the start of clocking
// and remains static during subsequent sound generation
struct opdata_cache
{
	uint16_t const *waveform;         // base of sine table
	uint32_t phase_step;              // phase step, or PHASE_STEP_DYNAMIC if PM is active
	uint32_t total_level;             // total level * 8 + KSL
	uint32_t block_freq;              // raw block frequency value (used to compute phase_step)
	int32_t detune;                   // detuning value (used to compute phase_step)
	uint32_t multiple;                // multiple value (x.1, used to compute phase_step)
	uint32_t eg_sustain;              // sustain level, shifted up to envelope values
	uint8_t eg_rate[EG_STATES];       // envelope rate, including KSR
	uint8_t eg_shift = 0;             // envelope shift amount
};


//*********************************************************
//  REGISTER CLASSES
//*********************************************************
// ======================> opn_registers
//
// OPN register map:
//
//      System-wide registers:
//           21 xxxxxxxx Test register
//           22 ----x--- LFO enable [OPNA+ only]
//              -----xxx LFO rate [OPNA+ only]
//           24 xxxxxxxx Timer A value (upper 8 bits)
//           25 ------xx Timer A value (lower 2 bits)
//           26 xxxxxxxx Timer B value
//           27 xx------ CSM/Multi-frequency mode for channel #2
//              --x----- Reset timer B
//              ---x---- Reset timer A
//              ----x--- Enable timer B
//              -----x-- Enable timer A
//              ------x- Load timer B
//              -------x Load timer A
//           28 x------- Key on/off operator 4
//              -x------ Key on/off operator 3
//              --x----- Key on/off operator 2
//              ---x---- Key on/off operator 1
//              ------xx Channel select
//
//     Per-channel registers (channel in address bits 0-1)
//     Note that all these apply to address+100 as well on OPNA+
//        A0-A3 xxxxxxxx Frequency number lower 8 bits
//        A4-A7 --xxx--- Block (0-7)
//              -----xxx Frequency number upper 3 bits
//        B0-B3 --xxx--- Feedback level for operator 1 (0-7)
//              -----xxx Operator connection algorithm (0-7)
//        B4-B7 x------- Pan left [OPNA]
//              -x------ Pan right [OPNA]
//              --xx---- LFO AM shift (0-3) [OPNA+ only]
//              -----xxx LFO PM depth (0-7) [OPNA+ only]
//
//     Per-operator registers (channel in address bits 0-1, operator in bits 2-3)
//     Note that all these apply to address+100 as well on OPNA+
//        30-3F -xxx---- Detune value (0-7)
//              ----xxxx Multiple value (0-15)
//        40-4F -xxxxxxx Total level (0-127)
//        50-5F xx------ Key scale rate (0-3)
//              ---xxxxx Attack rate (0-31)
//        60-6F x------- LFO AM enable [OPNA]
//              ---xxxxx Decay rate (0-31)
//        70-7F ---xxxxx Sustain rate (0-31)
//        80-8F xxxx---- Sustain level (0-15)
//              ----xxxx Release rate (0-15)
//        90-9F ----x--- SSG-EG enable
//              -----xxx SSG-EG envelope (0-7)
//
//     Special multi-frequency registers (channel implicitly #2; operator in address bits 0-1)
//        A8-AB xxxxxxxx Frequency number lower 8 bits
//        AC-AF --xxx--- Block (0-7)
//              -----xxx Frequency number upper 3 bits
//
//     Internal (fake) registers:
//        B8-BB --xxxxxx Latched frequency number upper bits (from A4-A7)
//        BC-BF --xxxxxx Latched frequency number upper bits (from AC-AF)
//

struct opn_registers
{
	// return an array of operator indices for each channel
	struct operator_mapping { uint32_t chan[CHANNELS]; };
	void operator_map(operator_mapping &dest);

	// return LFO/noise states
	uint32_t noise_state() const { return 0; }

	// caching helpers
	void cache_operator_data(uint32_t choffs, uint32_t opoffs, opdata_cache &cache);

	// compute the phase step, given a PM value
	uint32_t compute_phase_step(uint32_t choffs, uint32_t opoffs, opdata_cache const &cache, int32_t lfo_raw_pm);

	// system-wide registers
	uint32_t test();
	uint32_t lfo_enable();
	uint32_t lfo_rate();
	uint32_t timer_a_value();
	uint32_t timer_b_value();
	uint32_t csm();
	uint32_t multi_freq();
	uint32_t reset_timer_b();
	uint32_t reset_timer_a();
	uint32_t enable_timer_b();
	uint32_t enable_timer_a();
	uint32_t load_timer_b();
	uint32_t load_timer_a();
	uint32_t multi_block_freq(uint32_t num);

	// per-channel registers
	uint32_t ch_block_freq(uint32_t choffs);
	uint32_t ch_feedback(uint32_t choffs);
	uint32_t ch_algorithm(uint32_t choffs);
	uint32_t ch_output_any(uint32_t choffs);
	uint32_t ch_output_0(uint32_t choffs);
	uint32_t ch_output_1(uint32_t choffs);
	uint32_t ch_output_2(uint32_t choffs);
	uint32_t ch_output_3(uint32_t choffs);
	uint32_t ch_lfo_am_sens(uint32_t choffs);
	uint32_t ch_lfo_pm_sens(uint32_t choffs);

	// per-operator registers
	uint32_t op_detune(uint32_t opoffs);
	uint32_t op_multiple(uint32_t opoffs);
	uint32_t op_total_level(uint32_t opoffs);
	uint32_t op_ksr(uint32_t opoffs);
	uint32_t op_attack_rate(uint32_t opoffs);
	uint32_t op_decay_rate(uint32_t opoffs);
	uint32_t op_lfo_am_enable(uint32_t opoffs);
	uint32_t op_sustain_rate(uint32_t opoffs);
	uint32_t op_sustain_level(uint32_t opoffs);
	uint32_t op_release_rate(uint32_t opoffs);
	uint32_t op_ssg_eg_enable(uint32_t opoffs);
	uint32_t op_ssg_eg_mode(uint32_t opoffs);

	// internal state
	uint32_t m_lfo_counter;               // LFO counter
	uint8_t m_lfo_am;                     // current LFO AM value
	uint8_t m_regdata[REGISTERS];         // register data
	uint16_t m_waveform[WAVEFORMS][WAVEFORM_LENGTH]; // waveforms
};

extern opn_registers m_regs; // register accessor

//*********************************************************
//  CORE ENGINE CLASSES
//*********************************************************

// forward declarations
struct fm_engine;

// ======================> fm_operator

// fm_operator represents an FM operator (or "slot" in FM parlance), which
// produces an output sine wave modulated by an envelope
struct fm_operator
{
	// "quiet" value, used to optimize when we can skip doing working
	static constexpr uint32_t EG_QUIET = 0x200;

	// constructor
	fm_operator(uint32_t opoffs);

	// reset the operator state
	void reset(void);

	// return the operator/channel offset
	uint32_t opoffs(void);
	uint32_t choffs(void);

	// set the current channel
	void set_choffs(uint32_t choffs);

	// prepare prior to clocking
	bool prepare(void);

	// master clocking function
	void clock(uint32_t env_counter, int32_t lfo_raw_pm);

	// return the current phase value
	uint32_t phase(void);

	// compute operator volume
	int32_t compute_volume(uint32_t phase, uint32_t am_offset);

	// compute volume for the OPM noise channel
	int32_t compute_noise_volume(uint32_t am_offset);

	// key state control
	void keyonoff(uint32_t on, uint32_t type);

	// start the attack phase
	void start_attack(bool is_restart);

	// start the release phase
	void start_release();

	// clock phases
	void clock_keystate(uint32_t keystate);
	void clock_ssg_eg_state();
	void clock_envelope(uint32_t env_counter);
	void clock_phase(int32_t lfo_raw_pm);

	// return effective attenuation of the envelope
	uint32_t envelope_attenuation(uint32_t am_offset);

	// internal state
	uint32_t m_choffs;                     // channel offset in registers
	uint32_t m_opoffs;                     // operator offset in registers
	uint32_t m_phase;                      // current phase value (10.10 format)
	uint16_t m_env_attenuation;            // computed envelope attenuation (4.6 format)
	uint32_t m_env_state;                  // current envelope state
	uint8_t m_ssg_inverted;                // non-zero if the output should be inverted (bit 0)
	uint8_t m_key_state;                   // current key state: on or off (bit 0)
	uint8_t m_keyon_live;                  // live key on state (bit 0 = direct, bit 1 = rhythm, bit 2 = CSM)
	opdata_cache m_cache;                  // cached values for performance
};


// ======================> fm_channel

// fm_channel represents an FM channel which combines the output of 2 or 4
// operators into a final result
struct fm_channel
{
	// constructor
	fm_channel(uint32_t choffs);

	// reset the channel state
	void reset(void);

	// return the channel offset
	uint32_t choffs(void);

	// assign operators
	void assign(uint32_t index, fm_operator *op);

	// signal key on/off to our operators
	void keyonoff(uint32_t states, uint32_t type, uint32_t chnum);

	// prepare prior to clocking
	bool prepare(void);

	// master clocking function
	void clock(uint32_t env_counter, int32_t lfo_raw_pm);

	// specific 2-operator and 4-operator output handlers
	void output_2op(int32_t *output, uint32_t rshift, int32_t clipmax);
	void output_4op(int32_t *output, uint32_t rshift, int32_t clipmax);

	// are we a 4-operator channel or a 2-operator one?
	bool is4op(void);

	// helper to add values to the outputs based on channel enables
	void add_to_output(uint32_t choffs, int32_t *output, int32_t value);

	// internal state
	uint32_t m_choffs;     // channel offset in registers
	int16_t m_feedback[2]; // feedback memory for operator 1
	int16_t m_feedback_in; // next input value for op 1 feedback (set in output)
	fm_operator *m_op[4];  // up to 4 operators
};


// ======================> fm_engine

// fm_engine represents a set of operators and channels which together
// form a Yamaha FM core; chips that implement other engines (ADPCM, wavetable,
// etc) take this output and combine it with the others externally
struct fm_engine
{
	// constructor
	fm_engine();

	// reset the overall state
	void reset();

	// master clocking function
	uint32_t clock(uint32_t chanmask);

	// compute sum of channel outputs
	void output(int32_t *output, uint32_t rshift, int32_t clipmax, uint32_t chanmask);

	// write to the OPN registers
	void write(uint16_t regnum, uint8_t data);

	// return the current status
	uint8_t status(void);

	// set/reset bits in the status register, updating the IRQ status
	uint8_t set_reset_status(uint8_t set, uint8_t reset);

	// set the IRQ mask
	void set_irq_mask(uint8_t mask);

	// return the current clock prescale
	uint32_t clock_prescale(void);

	// set prescale factor (2/3/6)
	void set_clock_prescale(uint32_t prescale);

	// compute sample rate
	uint32_t sample_rate(uint32_t baseclock);

	// invalidate any caches
	void invalidate_caches(void);

	// timer callback; called by the interface when a timer fires
	void engine_timer_expired(uint32_t tnum);

	// check interrupts; called by the interface after synchronization
	void engine_check_interrupts();

	// mode register write; called by the interface after synchronization
	void engine_mode_write(uint8_t data);

	// assign the current set of operators to channels
	void assign_operators();

	// update the state of the given timer
	void update_timer(uint32_t which, uint32_t enable, int32_t delta_clocks);

	// internal state
	uint32_t m_env_counter;          // envelope counter; low 2 bits are sub-counter
	uint8_t m_status;                // current status register
	uint8_t m_clock_prescale;        // prescale factor (2/3/6)
	uint8_t m_irq_mask;              // mask of which bits signal IRQs
	uint8_t m_irq_state;             // current IRQ state
	uint8_t m_timer_running[2];      // current timer running state
	uint8_t m_total_clocks;          // low 8 bits of the total number of clocks processed
	uint32_t m_active_channels;      // mask of active channels (computed by prepare)
	uint32_t m_modified_channels;    // mask of channels that have been modified
	uint32_t m_prepare_count;        // counter to do periodic prepare sweeps
	fm_channel *m_channel[CHANNELS]; // channel pointers
	fm_operator *m_operator[OPERATORS]; // operator pointers
};

extern fm_engine *m_engine;

#endif // YMFM_FM_H
