/*---------------------------------------------------------------------------------

	derived from the default ARM7 core

		Copyright (C) 2005 - 2010
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.

	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.

	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <dswifi7.h>
#include <maxmod7.h>
#include <nds/bios.h>

//tools.
#include "opcode.h"

//Bit29-30  Format       (0=PCM8, 1=PCM16, 2=IMA-ADPCM, 3=PSG/Noise)
#define PCM8  0<<29 
#define PCM16 1<<29
#define ADPCM 2<<29
#define PSGNOISE 3<<29

//sound buffer
u32 waveram[16];

int pitch=0;
int pitch2=0;
int basefreq=0;
int volume=0;

//FIFO struct
typedef struct{
u32 REG_FIFO_SENDEMPTY_STAT; 	//0<<0
u32 REG_FIFO_SENDFULL_STAT; 	//0<<1
u32 REG_FIFO_SENDEMPTY_IRQ; 	//0<<2
u32 REG_FIFO_SENDEMPTY_CLR; 	//0<<3
u32 REG_FIFO_RECVEMPTY; 		//0<<8
u32 REG_FIFO_RECVFULL; 			//0<<9
u32 REG_FIFO_RECVNOTEMPTY_IRQ; 	//0<<10
u32 REG_FIFO_ERRSENDRECVEMPTYFULL; //0<<14
u32 REG_FIFO_ENABLESENDRECV; 	//0/15
u32 REG_FIFO_RECVNOTEMPTY; //0<<17 (0 empty, 1 not)
}fiforegister;


//BIOS vector sound table
static s16 sineTable[256] = {
  (s16)0x0000, (s16)0x0192, (s16)0x0323, (s16)0x04B5, (s16)0x0645, (s16)0x07D5, (s16)0x0964, (s16)0x0AF1,
  (s16)0x0C7C, (s16)0x0E05, (s16)0x0F8C, (s16)0x1111, (s16)0x1294, (s16)0x1413, (s16)0x158F, (s16)0x1708,
  (s16)0x187D, (s16)0x19EF, (s16)0x1B5D, (s16)0x1CC6, (s16)0x1E2B, (s16)0x1F8B, (s16)0x20E7, (s16)0x223D,
  (s16)0x238E, (s16)0x24DA, (s16)0x261F, (s16)0x275F, (s16)0x2899, (s16)0x29CD, (s16)0x2AFA, (s16)0x2C21,
  (s16)0x2D41, (s16)0x2E5A, (s16)0x2F6B, (s16)0x3076, (s16)0x3179, (s16)0x3274, (s16)0x3367, (s16)0x3453,
  (s16)0x3536, (s16)0x3612, (s16)0x36E5, (s16)0x37AF, (s16)0x3871, (s16)0x392A, (s16)0x39DA, (s16)0x3A82,
  (s16)0x3B20, (s16)0x3BB6, (s16)0x3C42, (s16)0x3CC5, (s16)0x3D3E, (s16)0x3DAE, (s16)0x3E14, (s16)0x3E71,
  (s16)0x3EC5, (s16)0x3F0E, (s16)0x3F4E, (s16)0x3F84, (s16)0x3FB1, (s16)0x3FD3, (s16)0x3FEC, (s16)0x3FFB,
  (s16)0x4000, (s16)0x3FFB, (s16)0x3FEC, (s16)0x3FD3, (s16)0x3FB1, (s16)0x3F84, (s16)0x3F4E, (s16)0x3F0E,
  (s16)0x3EC5, (s16)0x3E71, (s16)0x3E14, (s16)0x3DAE, (s16)0x3D3E, (s16)0x3CC5, (s16)0x3C42, (s16)0x3BB6,
  (s16)0x3B20, (s16)0x3A82, (s16)0x39DA, (s16)0x392A, (s16)0x3871, (s16)0x37AF, (s16)0x36E5, (s16)0x3612,
  (s16)0x3536, (s16)0x3453, (s16)0x3367, (s16)0x3274, (s16)0x3179, (s16)0x3076, (s16)0x2F6B, (s16)0x2E5A,
  (s16)0x2D41, (s16)0x2C21, (s16)0x2AFA, (s16)0x29CD, (s16)0x2899, (s16)0x275F, (s16)0x261F, (s16)0x24DA,
  (s16)0x238E, (s16)0x223D, (s16)0x20E7, (s16)0x1F8B, (s16)0x1E2B, (s16)0x1CC6, (s16)0x1B5D, (s16)0x19EF,
  (s16)0x187D, (s16)0x1708, (s16)0x158F, (s16)0x1413, (s16)0x1294, (s16)0x1111, (s16)0x0F8C, (s16)0x0E05,
  (s16)0x0C7C, (s16)0x0AF1, (s16)0x0964, (s16)0x07D5, (s16)0x0645, (s16)0x04B5, (s16)0x0323, (s16)0x0192,
  (s16)0x0000, (s16)0xFE6E, (s16)0xFCDD, (s16)0xFB4B, (s16)0xF9BB, (s16)0xF82B, (s16)0xF69C, (s16)0xF50F,
  (s16)0xF384, (s16)0xF1FB, (s16)0xF074, (s16)0xEEEF, (s16)0xED6C, (s16)0xEBED, (s16)0xEA71, (s16)0xE8F8,
  (s16)0xE783, (s16)0xE611, (s16)0xE4A3, (s16)0xE33A, (s16)0xE1D5, (s16)0xE075, (s16)0xDF19, (s16)0xDDC3,
  (s16)0xDC72, (s16)0xDB26, (s16)0xD9E1, (s16)0xD8A1, (s16)0xD767, (s16)0xD633, (s16)0xD506, (s16)0xD3DF,
  (s16)0xD2BF, (s16)0xD1A6, (s16)0xD095, (s16)0xCF8A, (s16)0xCE87, (s16)0xCD8C, (s16)0xCC99, (s16)0xCBAD,
  (s16)0xCACA, (s16)0xC9EE, (s16)0xC91B, (s16)0xC851, (s16)0xC78F, (s16)0xC6D6, (s16)0xC626, (s16)0xC57E,
  (s16)0xC4E0, (s16)0xC44A, (s16)0xC3BE, (s16)0xC33B, (s16)0xC2C2, (s16)0xC252, (s16)0xC1EC, (s16)0xC18F,
  (s16)0xC13B, (s16)0xC0F2, (s16)0xC0B2, (s16)0xC07C, (s16)0xC04F, (s16)0xC02D, (s16)0xC014, (s16)0xC005,
  (s16)0xC000, (s16)0xC005, (s16)0xC014, (s16)0xC02D, (s16)0xC04F, (s16)0xC07C, (s16)0xC0B2, (s16)0xC0F2,
  (s16)0xC13B, (s16)0xC18F, (s16)0xC1EC, (s16)0xC252, (s16)0xC2C2, (s16)0xC33B, (s16)0xC3BE, (s16)0xC44A,
  (s16)0xC4E0, (s16)0xC57E, (s16)0xC626, (s16)0xC6D6, (s16)0xC78F, (s16)0xC851, (s16)0xC91B, (s16)0xC9EE,
  (s16)0xCACA, (s16)0xCBAD, (s16)0xCC99, (s16)0xCD8C, (s16)0xCE87, (s16)0xCF8A, (s16)0xD095, (s16)0xD1A6,
  (s16)0xD2BF, (s16)0xD3DF, (s16)0xD506, (s16)0xD633, (s16)0xD767, (s16)0xD8A1, (s16)0xD9E1, (s16)0xDB26,
  (s16)0xDC72, (s16)0xDDC3, (s16)0xDF19, (s16)0xE075, (s16)0xE1D5, (s16)0xE33A, (s16)0xE4A3, (s16)0xE611,
  (s16)0xE783, (s16)0xE8F8, (s16)0xEA71, (s16)0xEBED, (s16)0xED6C, (s16)0xEEEF, (s16)0xF074, (s16)0xF1FB,
  (s16)0xF384, (s16)0xF50F, (s16)0xF69C, (s16)0xF82B, (s16)0xF9BB, (s16)0xFB4B, (s16)0xFCDD, (s16)0xFE6E
};

//sound stuff
//sample frequency: 22050 11025 44100
//nds hardware channel setup
typedef struct{
char chstat[15]; //0 free - 1 busy channel
char channel[15]; 
char mode[3]; //bit30-31 & 0x60000000 -> (0=PCM8, 1=PCM16, 2=IMA-ADPCM, 3=PSG/Noise)
int frequency[15]; //22050 11025 44100
}NDSCH;

// info about the sample
typedef struct tagSwavInfo
{
	u8  nWaveType;    // 0 = PCM8, 1 = PCM16, 2 = (IMA-)ADPCM
	u8  bLoop;        // Loop flag = TRUE|FALSE
	u16 nSampleRate;  // Sampling Rate
	u16 nTime;        // (ARM7_CLOCK / nSampleRate) [ARM7_CLOCK: 33.513982MHz / 2 = 1.6756991 E +7]
	u16 nLoopOffset;  // Loop Offset (expressed in words (32-bits))
	u32 nNonLoopLen;  // Non Loop Length (expressed in words (32-bits))
} SWAVINFO;

//returns free channel or 0x10 if error
int getfreech(NDSCH * snd){
int i;
	for(i=0;i<15;i++){
		if (snd->chstat[i]==0) return snd->channel[i];
	}
return 0x10;
}

//toggles channel to 0 - 1, returns 0 if success, 1 otherwise 
int updatechannel(int channel, int toggle, NDSCH * snd){
	if ((snd->chstat[channel]=toggle)) return 0;
	else return 1;
}
u16 volumetable(int volume){
	return (u64)volume*swiGetVolumeTable(volume);
}
// This function was obtained through disassembly of Ninty's sound driver
u16 AdjustFreq(u16 basefreq, int pitch){
	u64 freq;
	int shift = 0;
	pitch = -pitch;
	while (pitch < 0){
		shift --;
		pitch += 0x300;
	}
	while (pitch >= 0x300){
		shift ++;
		pitch -= 0x300;
	}
	freq = (u64)basefreq * ((u32)swiGetPitchTable(pitch) + 0x10000);
	shift -= 16;
	if (shift <= 0)
		freq >>= -shift;
	else if (shift < 32){
		if (freq & ((~0ULL) << (32-shift))) return 0xFFFF;
		freq <<= shift;
	}else
		return 0x10;
	if (freq < 0x10) return 0x10;
	if (freq > 0xFFFF) return 0xFFFF;
	return (u16)freq;
}

static inline u16 ADJUST_FREQ(u16 basefreq, int noteN, int baseN){
	return AdjustFreq(basefreq, ((noteN - baseN) * 64));
}

static inline u16 ADJUST_PITCH_BEND(u16 basefreq, int pitchb, int pitchr){
	if (!pitchb) return basefreq;
	return AdjustFreq(basefreq, (pitchb*pitchr) >> 1);
}

// PCM8  0<<29 // PCM16 1<<29 // IMA-ADPCM 2<<29 // PSGNOISE 3<<29
// starting with channel 0 at 4000400h..400040Fh, up to channel 15 at 40004F0h..40004FFh.
int playsound(int freq,int channel,int volume,int pan,int psgduty,int mode,int enable){
	
	
//PSG Wave Duty (channel 8..13 in PSG mode)
//Each duty cycle consists of eight HIGH or LOW samples, so the sound frequency is 1/8th of the selected sample rate. The duty cycle always starts at the begin of the LOW period when the sound gets (re-)started.
//  0  12.5% "_______-_______-_______-"
//  1  25.0% "______--______--______--"
//  2  37.5% "_____---_____---_____---"
//  3  50.0% "____----____----____----"
//  4  62.5% "___-----___-----___-----"
//  5  75.0% "__------__------__------"
//  6  87.5% "_-------_-------_-------"
//  7   0.0% "________________________"
//The Wave Duty bits exist and are read/write-able on all channels (although they are actually used only in PSG mode on channels 8-13).

//PSG Noise (channel 14..15 in PSG mode)
//Noise randomly switches between HIGH and LOW samples, the output levels are calculated, at the selected sample rate, as such:
//  X=X SHR 1, IF carry THEN Out=LOW, X=X XOR 6000h ELSE Out=HIGH
//The initial value when (re-)starting the sound is X=7FFFh. The formula is more or less same as "15bit polynomial counter" used on 8bit Gameboy and GBA.
	
	
	//40004x0h - NDS7 - SOUNDxCNT - Sound Channel X Control Register (R/W)
	//Bit0-6    Volume Mul   (0..127=silent..loud)
	//Bit7      Not used     (always zero)
	//Bit8-9    Volume Div   (0=Normal, 1=Div2, 2=Div4, 3=Div16)
	//Bit10-14  Not used     (always zero)
	//Bit15     Hold         (0=Normal, 1=Hold last sample after one-shot sound)
	//Bit16-22  Panning      (0..127=left..right) (64=half volume on both speakers)
	//Bit23     Not used     (always zero)
	//Bit24-26  Wave Duty    (0..7) ;HIGH=(N+1)*12.5%, LOW=(7-N)*12.5% (PSG only)
	//Bit27-28  Repeat Mode  (0=Manual, 1=Loop Infinite, 2=One-Shot, 3=Prohibited)
	//Bit29-30  Format       (0=PCM8, 1=PCM16, 2=IMA-ADPCM, 3=PSG/Noise)
	//Bit31     Start/Status (0=Stop, 1=Start/Busy)
		
		//ori: stru32inlasm(0x4000400+(channel<<4),0x0,volume<<0|pan<<16|psgduty<<24|mode<<29|enable<<31);
	*(u32*)(0x4000400+(channel<<4))=(volume<<0|pan<<16|psgduty<<24|mode<<29|enable<<31);
	
	//40004x4h - NDS7 - SOUNDxSAD - Sound Channel X Data Source Register (W)
	//Bit0-26  Source Address (must be word aligned, bit0-1 are always zero)
	//Bit27-31 Not used

	//40004x8h - NDS7 - SOUNDxTMR - Sound Channel X Timer Register (W)
	//Bit0-15  Timer Value, Sample frequency, timerval=-(33513982/2)/freq
	//The PSG Duty Cycles are composed of eight "samples", and so, the frequency for Rectangular Wave is 1/8th of the selected sample frequency.
	//For PSG Noise, the noise frequency is equal to the sample frequency.
		
		//ori: stru32inlasm(0x4000408+(channel<<4),0x0,freq);
	*(u32*)(0x4000408+(channel<<4))=freq;
	return 0;
}

//power management
//---------------------------------------------------------------------------------
void enablesound() {
//---------------------------------------------------------------------------------
	powerOn(POWER_SOUND);
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE ) | PM_SOUND_AMP );
	REG_SOUNDCNT = SOUND_ENABLE;
	REG_MASTER_VOLUME = 127;
}

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
	Wifi_Update();
}


//---------------------------------------------------------------------------------
void VcountHandler() {
//---------------------------------------------------------------------------------
	inputGetAndSend();
}

volatile bool exitflag = false;

//---------------------------------------------------------------------------------
void powerButtonCB() {
//---------------------------------------------------------------------------------
	exitflag = true;
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
	readUserSettings();

	irqInit();
	fifoInit();

	mmInstall(FIFO_MAXMOD);
	// Start the RTC tracking IRQ
	initClockIRQ();

	SetYtrigger(80);

	installWifiFIFO();
	installSoundFIFO();

	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);   

	setPowerButtonCB(powerButtonCB);   

	// Keep the ARM7 mostly idle
	while (!exitflag) {
		if ( 0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R))) {
			exitflag = true;
		}

		
		if(fifoCheckValue32(FIFO_USER_01))
		{
			//we don't care what value got sent. all we're interested in is the signal
			fifoGetValue32(FIFO_USER_01);

			int t1,t2;
			u32 temp=touchReadTemperature(&t1, &t2);
			//these will queue up
			fifoSendValue32(FIFO_USER_01,t1);
			fifoSendValue32(FIFO_USER_01,t2);
			fifoSendValue32(FIFO_USER_01,temp);
		}

		swiIntrWait(1,IRQ_FIFO_NOT_EMPTY | IRQ_VBLANK);
	}
	return 0;
}
