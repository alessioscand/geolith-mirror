/*  gngeo a neogeo emulator
 *  Copyright (C) 2001 Peponas Mathieu
 * 
 *  This program is free software; you can redistribute it and/or modify  
 *  it under the terms of the GNU General Public License as published by   
 *  the Free Software Foundation; either version 2 of the License, or    
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "emu.h"
#include "memory.h"
#include "mamez80/z80.h"
#include "state.h"

static uint8_t *z80map1, *z80map2, *z80map3, *z80map4;

uint8_t mame_z80mem[0x10000];
//typedef struct Z80_STATE {
//    uint16_t PC,SP,AF,BC,DE,HL,IX,IY;
//    uint16_t AF2,BC2,DE2,HL2;
//    uint8_t  R,R2,IFF1,IFF2,IM,I;
//    uint8_t  IRQV,IRQL;
//    uint16_t bank[4];
//    uint8_t  ram[0x800];
//}Z80_STATE;
//static Z80_STATE z80_st;

void mame_z80_writeport16(uint16_t port, uint8_t value)
{
    //printf("Write port %d=%d\n",port,value);
    z80_port_write(port, value);
}

uint8_t mame_z80_readport16(uint16_t port)
{
    //printf("Read port %d\n",port);
    return z80_port_read(port);
}


/* cpu interface implementation */
void cpu_z80_switchbank(uint8_t bank, uint16_t PortNo)
{
    if (bank<=3)
	z80_bank[bank]=PortNo;

    switch (bank) {
    case 0:
		z80map1 = memory.rom.cpu_z80.p + (0x4000 * ((PortNo >> 8) & 0x0f));
		if ((0x4000 * ((PortNo >> 8) & 0x0f))<memory.rom.cpu_z80.size)
	memcpy(mame_z80mem + 0x8000, z80map1, 0x4000);
	break;
    case 1:
	z80map2 = memory.rom.cpu_z80.p + (0x2000 * ((PortNo >> 8) & 0x1f));
		if ((0x2000 * ((PortNo >> 8) & 0x1f))<memory.rom.cpu_z80.size)
	memcpy(mame_z80mem + 0xc000, z80map2, 0x2000);
	break;
    case 2:
	z80map3 = memory.rom.cpu_z80.p + (0x1000 * ((PortNo >> 8) & 0x3f));
	if ((0x1000 * ((PortNo >> 8) & 0x3f))<memory.rom.cpu_z80.size)
	memcpy(mame_z80mem + 0xe000, z80map3, 0x1000);
	break;
    case 3:
	z80map4 = memory.rom.cpu_z80.p + (0x0800 * ((PortNo >> 8) & 0x7f));
	if ((0x0800 * ((PortNo >> 8) & 0x7f))<memory.rom.cpu_z80.size)
	memcpy(mame_z80mem + 0xf000, z80map4, 0x0800);
	break;
    }
}

int mame_z80_irq_callback(int a)
{
    return 0;
}


void cpu_z80_mkstate(gzFile gzf,int mode) {

/* Old save state version was completly buggy, tried to load it anyway
 * the size of the old struct was 2088
 * */
	if (state_version==ST_VER2 && mode==STREAD) {
		uint8_t z80_st_dummy[2088];
		mkstate_data(gzf, &z80_st_dummy, 2088, mode);
	} else
		mkstate_data(gzf, z80_stateData(), z80_stateDataSize, mode);
	mkstate_data(gzf, mame_z80mem, 0x10000, mode);
	if (mode==STREAD) {
		int i;
		for (i = 0; i < 4; i++) {
			cpu_z80_switchbank(i, z80_bank[i]);
		}
//		memcpy(mame_z80mem + 0xf800, memory.z80_ram, 0x800);
	}
}

void cpu_z80_init(void)
{
    //  init_mamez80_mem();
    z80_init();
    //printf("Sizeof Z80_STATE %d\n",sizeof(z80_st));
    /* bank initalisation */
    z80map1 = memory.rom.cpu_z80.p + 0x8000;
    z80map2 = memory.rom.cpu_z80.p + 0xc000;
    z80map3 = memory.rom.cpu_z80.p + 0xe000;
    z80map4 = memory.rom.cpu_z80.p + 0xf000;

    z80_bank[0]=0x8000;
    z80_bank[1]=0xc000;
    z80_bank[2]=0xe000;
    z80_bank[3]=0xf000;

    memcpy(mame_z80mem, memory.rom.cpu_z80.p, 0xf800);
    z80_reset(NULL);
    z80_set_irq_callback(mame_z80_irq_callback);
    //z80_init_save_state();
}

void cpu_z80_run(int nbcycle)
{
    //printf("%x\n",z80_get_reg(Z80_PC));
    z80_execute(nbcycle);
}
void cpu_z80_nmi(void)
{
    //z80_set_irq_line(IRQ_LINE_NMI, 1/*PULSE_LINE- INTERNAL_CLEAR_LINE*/);
    z80_set_irq_line(IRQ_LINE_NMI, ASSERT_LINE);
    z80_set_irq_line(IRQ_LINE_NMI, CLEAR_LINE);
}
void cpu_z80_raise_irq(int l)
{
    z80_set_irq_line(l, ASSERT_LINE);
}
void cpu_z80_lower_irq(void)
{
    z80_set_irq_line(0, CLEAR_LINE);
}

uint16_t cpu_z80_get_pc(void)
{
    return 0;
}
