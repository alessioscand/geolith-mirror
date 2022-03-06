/*
MIT License

Copyright (c) 2022 Rupert Carmichael

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "ymfm_opn.h"
#include "ymfm_shim.h"

extern "C" {
    #include "memory.h"
}

using namespace ymfm;

class ym_device : public ymfm::ymfm_interface {
public:
    virtual uint8_t ymfm_external_read(access_class type, uint32_t address) override {
        //printf("rd: %08x\n", address);
        switch (type) {
            case ymfm::ACCESS_ADPCM_A:
                if (address & 0xffffff > memory.rom.adpcma.size)
                    return 0;
                return memory.rom.adpcma.p[address & 0xffffff];
            case ymfm::ACCESS_ADPCM_B:
                if (address & 0xffffff > memory.rom.adpcmb.size)
                    return 0;
                return memory.rom.adpcmb.p[address & 0xffffff];
            default:
                return 0;
        }
    }

    virtual void ymfm_external_write(access_class type, uint32_t address, uint8_t data) override { }

    virtual void ymfm_update_irq(bool asserted) override {
        if (asserted)
            cpu_z80_raise_irq(0);
        else
            cpu_z80_lower_irq();
    }

    virtual void ymfm_set_timer(uint32_t tnum, int32_t duration) override {
        if (duration >= 0)
            timer[tnum] += (duration / divisor);
        else
            timer[tnum] = duration;
    }

    virtual void ymfm_set_busy_end(uint32_t clocks) override {
        busytimer += clocks / divisor;
        busyfrac += clocks % divisor;
        if (busyfrac >= divisor) {
            ++busytimer;
            busyfrac -= divisor;
        }
    }

    virtual bool ymfm_is_busy() override {
        return busytimer ? true : false;
    }


    void timer_tick() {
        if (busytimer) {
            --busytimer;
        }

        for (int i = 0; i < 2; ++i) {
            if (timer[i] < 0)
                continue;
            else if (--timer[i] == 0)
                m_engine->engine_timer_expired(i);
        }
    }

    int busytimer;
    int busyfrac;
    int timer[2];
    int divisor;
};

#define SIZE_YMBUF 2048

static int16_t ymbuf[SIZE_YMBUF];
static size_t bufpos;

static ym_device intf;
static ym2610 *chip;
static ym2610::output_data output;

// Grab the pointer to the buffer
int16_t* ymfm_shim_get_buffer(void) {
    bufpos = 0;
    return &ymbuf[0];
}

void ymfm_shim_init(void) {
    intf.divisor = 144;
    chip = new ym2610(intf);
    chip->set_fidelity(OPN_FIDELITY_MED);
}

void ymfm_shim_deinit(void) {
    if (chip)
        delete chip;
}

void ymfm_shim_reset(void) {
    chip->reset();
}

uint8_t ymfm_shim_rd(uint16_t addr) {
    return chip->read(addr);
}

void ymfm_shim_wr(uint16_t addr, uint8_t data) {
    chip->write(addr, data);
}

size_t ymfm_shim_exec(void) {
    intf.timer_tick();
    chip->generate(&output);
    ymbuf[bufpos++] = output.data[0];
    ymbuf[bufpos++] = output.data[1];
    return 1;
}
