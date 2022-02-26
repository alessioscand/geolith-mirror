/*
 * MAME License
 * 
 * Redistribution and use of the MAME code or any derivative works are 
 * permitted provided that the following conditions are met:
 *
 *  * Redistributions may not be sold, nor may they be used in a 
 *    commercial product or activity.
 *
 *  * Redistributions that are modified from the original source 
 *    must include the complete source code, including the source 
 *    code for all components used by a binary built from the modified 
 *    sources. However, as a special exception, the source code distributed 
 *    need not include anything that is normally distributed (in either 
 *    source or binary form) with the major components (compiler, kernel, 
 *    and so on) of the operating system on which the executable runs, 
 *    unless that component itself accompanies the executable.
 *
 *  * Redistributions must reproduce the above copyright notice, this 
 *    list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mame_layer.h"
#include <string.h>
#include <stdio.h>

/*
  fixed
  main
  mainbios
  sprites
  ym

audiocpu
audiocrypt
fixed
mainbios
maincpu
sprites
ym

*/

UINT8 *memory_region( GAME_ROMS *r, char *region ) {
    if (strcmp(region,"audiocpu")==0) return r->cpu_z80.p;
    if (strcmp(region,"audiocrypt")==0) return r->cpu_z80c.p;
    if (strcmp(region,"fixed")==0) return r->game_sfix.p;
    if (strcmp(region,"maincpu")==0) return r->cpu_m68k.p;
    if (strcmp(region,"mainbios")==0) return r->bios_m68k.p;
    if (strcmp(region,"sprites")==0) return r->tiles.p;
    if (strcmp(region,"ym")==0) return r->adpcma.p;
    printf("memory_region %s not found",region);

    return NULL;
}
UINT32 memory_region_length( GAME_ROMS *r, char *region ) {
    if (strcmp(region,"audiocpu")==0) return r->cpu_z80.size;
    if (strcmp(region,"audiocrypt")==0) return r->cpu_z80c.size;
    if (strcmp(region,"fixed")==0) return r->game_sfix.size;
    if (strcmp(region,"maincpu")==0) return r->cpu_m68k.size;
    if (strcmp(region,"mainbios")==0) return r->bios_m68k.size;
    if (strcmp(region,"sprites")==0) return r->tiles.size;
    if (strcmp(region,"ym")==0) return r->adpcma.size;
    printf("memory_region_length %s not found",region);

    return 0;
}

void *malloc_or_die(UINT32 b) {
    void *a=malloc(b);
    if (a) return a;
    printf("Not enough memory :( exiting\n");
    exit(1);
    return NULL;
}
