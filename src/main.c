/**
  @file main_sdl.c

  This is an SDL implementation of the game front end. It can be used to
  compile a native executable or a transpiled JS browser version with
  emscripten.

  This frontend is not strictly minimal, it could be reduced a lot. If you want
  a learning example of frontend, look at another, simpler one, e.g. terminal.

  by Miloslav Ciz (drummyfish), 2019

  Released under CC0 1.0 (https://creativecommons.org/publicdomain/zero/1.0/)
  plus a waiver of all other intellectual property. The goal of this work is to
  be and remain completely in the public domain forever, available for any use
  whatsoever.
*/


#include "cd32.h"

#define SFG_FPS 20
#define SFG_SCREEN_RESOLUTION_X 160
#define SFG_SCREEN_RESOLUTION_Y 160
#define SFG_RAYCASTING_SUBSAMPLE 16
#define SFG_DIMINISH_SPRITES 0
#define SFG_DITHERED_SHADOW 0
#define SFG_BACKGROUND_BLUR 0
#define SFG_RAYCASTING_MAX_STEPS 15
#define SFG_RAYCASTING_MAX_HITS 5
#define	SFG_CAN_EXIT 0
#define SFG_DRAW_LEVEL_BACKGROUND 0

#define SFG_TEXTURE_DISTANCE 0 //Disables texturing
#define RCL_TEXTURE_VERTICAL_STRETCH 0
#define RCL_ROLL_TEXTURE_COORDS 0


/*
  SDL is easier to play thanks to nice controls so make the player take full
  damage to make it a bit harder.
*/
#define SFG_PLAYER_DAMAGE_MULTIPLIER 1024

#if SFG_SCREEN_RESOLUTION_X == 160
#define Y_FRAMEBUFFER(yy) ((yy << 7) + (yy << 5))
#elif SFG_SCREEN_RESOLUTION_X == 128
#define Y_FRAMEBUFFER(yy) (yy << 7)
#elif SFG_SCREEN_RESOLUTION_X == 320
#define Y_FRAMEBUFFER(yy) ((yy << 8) + (yy << 6))
#else
#error "Please adjust Y_FRAMEBUFFER"
#endif


#define SFG_setPixel(x, y, c)  gfxbuf[Y_FRAMEBUFFER(y) + x] = c;
#define DCUSTOM (*((volatile UWORD*) 0xdff000))

#define VIDEO_WIDTH 320
#define VIDEO_HEIGHT 256
#define VIDEO_DEPTH 4
#define NUM_PLANES (1 << VIDEO_DEPTH)


#define SFG_getTimeMs() ticks * 200
#define SFG_sleepMs(t) 
static uint32_t ticks;

/*
static inline void *my_memset(void *s, int c, size_t count)
{
	void *xs = s;
	size_t temp;

	c &= 0xff;
	c |= c << 8;
	c |= c << 16;
	if ((long)s & 1) {
		char *cs = s;
		*cs++ = c;
		s = cs;
		count--;
	}
	if (count > 2 && (long)s & 2) {
		short *ss = s;
		*ss++ = c;
		s = ss;
		count -= 2;
	}
	temp = count >> 2;
	if (temp) {
		long *ls = s;
		size_t temp1;
		asm volatile (
			"	movel %1,%2\n"
			"	andw  #7,%2\n"
			"	lsrl  #3,%1\n"
			"	negw  %2\n"
			"	jmp   %%pc@(2f,%2:w:2)\n"
			"1:	movel %3,%0@+\n"
			"	movel %3,%0@+\n"
			"	movel %3,%0@+\n"
			"	movel %3,%0@+\n"
			"	movel %3,%0@+\n"
			"	movel %3,%0@+\n"
			"	movel %3,%0@+\n"
			"	movel %3,%0@+\n"
			"2:	dbra  %1,1b\n"
			"	clrw  %1\n"
			"	subql #1,%1\n"
			"	jpl   1b"
			: "=a" (ls), "=d" (temp), "=&d" (temp1)
			: "d" (c), "0" (ls), "1" (temp));
		s = ls;
	}
	if (count & 2) {
		short *ss = s;
		*ss++ = c;
		s = ss;
	}
	if (count & 1) {
		char *cs = s;
		*cs = c;
	}
	return xs;
}

*/

static inline void my_memset(void *ptr, unsigned char val, size_t size) {
    asm volatile (
        "move.b %2, %%d0\n\t"   // set D0 to the constant value
        "1:\n\t"
        "move.b %%d0, (%0)+\n\t"  // store constant value to memory and increment address
        "dbra %1, 1b\n\t"      // decrement loop counter and repeat if not zero
        : "+a" (ptr), "+d" (size)
        : "g" (val)
        : "d0", "cc", "memory"
    );
}

#include "game.h"
//#include "sounds.h"

// now implement the Anarch API functions (SFG_*)

/*
void SFG_setPixel(uint16_t x, uint16_t y, uint8_t colorIndex)
{
	//gfxbuf[(y * SFG_SCREEN_RESOLUTION_X + (x))] = colorIndex;
	gfxbuf[((y << 8) + (y << 6)) + x] = colorIndex;
}


uint32_t SFG_getTimeMs()
{
	return ticks * 1000;
}*/

void SFG_save(uint8_t data[SFG_SAVE_SIZE])
{
}

uint8_t SFG_load(uint8_t data[SFG_SAVE_SIZE])
{
	// no saving for web version
	return 0;
}


void SFG_getMouseOffset(int16_t *x, int16_t *y)
{
}

void SFG_processEvent(uint8_t event, uint8_t data)
{
}


static uint32_t state0 = 0;
int8_t SFG_keyPressed(uint8_t key)
{ 
	
  #define b(x) ((state0 & JPF_ ## x) == JPF_ ## x)

  switch (key)
  {
    case SFG_KEY_UP: return b(JOY_UP); break;
    case SFG_KEY_RIGHT: 
      return b(JOY_RIGHT); break;
    case SFG_KEY_DOWN: 
      return b(JOY_DOWN); break;
    case SFG_KEY_LEFT: return b(JOY_LEFT); break;
    case SFG_KEY_A: return b(BUTTON_RED); break;
    case SFG_KEY_B: return b(BUTTON_BLUE); break;
    /*case SFG_KEY_C: return b(YELLOW); break;*/
    case SFG_KEY_JUMP: return b(BUTTON_GREEN); break;
	case SFG_KEY_STRAFE_LEFT: return b(BUTTON_FORWARD); break;
    case SFG_KEY_STRAFE_RIGHT: return b(BUTTON_REVERSE); break;
    
    //case SFG_KEY_MAP: return b(MINUS); break;
    
    case SFG_KEY_CYCLE_WEAPON: return b(BUTTON_YELLOW); break;
    case SFG_KEY_MENU: return b(BUTTON_PLAY); break;
   /* case SFG_KEY_NEXT_WEAPON:
      if (b(ZR))
        return 1;
      else
      return 0;
      break;*/

    default: return 0; break;
  }

  #undef b
}
 


uint8_t musicOn = 0;
// ^ this has to be init to 0 (not 1), else a few samples get played at start


void SFG_setMusic(uint8_t value)
{
  switch (value)
  {
    case SFG_MUSIC_TURN_ON: musicOn = 1; break;
    case SFG_MUSIC_TURN_OFF: musicOn = 0; break;
    case SFG_MUSIC_NEXT:
    {
		//SFG_nextMusicTrack();
	}
	break;
    default: break;
  }
}

void SFG_playSound(uint8_t soundIndex, uint8_t volume)
{
}

const uint8_t cur_pal[768] = {0,0,0,2,3,2,11,13,11,37,33,32,76,78,76,130,132,130,202,211,202,
255,255,255,2,1,1,11,3,2,32,8,8,58,15,14,106,28,26,157,46,44,
237,69,67,255,108,106,2,2,1,14,8,4,32,19,8,67,39,17,106,69,26,
157,103,51,237,151,76,255,203,106,2,4,1,11,13,2,26,33,8,51,64,14,
85,103,32,143,165,44,202,237,76,255,255,118,1,4,1,6,13,2,11,36,8,
22,64,14,44,103,26,67,158,44,95,237,67,143,255,118,1,4,1,4,13,6,
8,33,14,17,64,32,32,103,51,44,158,85,67,237,118,118,255,171,1,4,4,
4,13,11,11,31,26,17,64,58,37,97,95,51,158,157,76,237,237,118,255,255,
1,1,2,4,6,11,8,13,32,14,28,58,32,53,95,51,82,157,76,119,237,
118,165,255,1,1,2,4,3,14,11,8,32,22,15,58,37,28,95,58,46,157,
85,64,237,130,108,255,2,1,4,8,3,14,26,8,32,51,15,58,85,28,106,
130,49,157,186,69,219,255,114,255,4,1,1,14,3,8,32,8,17,58,15,37,
106,28,67,157,46,95,237,69,157,255,108,202,6,0,0,17,0,0,37,0,1,
85,0,1,130,1,1,186,3,4,255,5,6,255,17,22,6,2,0,17,6,1,
37,15,1,76,28,1,130,46,1,186,69,6,255,103,6,255,132,22,2,7,0,
11,19,0,26,42,1,51,82,1,85,132,1,130,187,6,186,255,6,219,255,22,
0,7,0,1,19,0,2,42,1,2,82,1,11,132,1,17,187,4,26,255,8,
44,255,17,0,7,1,0,19,2,1,42,6,1,78,14,2,132,22,6,187,37,
2,255,51,17,255,85,0,7,6,0,19,17,0,42,44,1,78,76,4,125,118,
2,187,186,8,255,255,26,255,255,0,1,6,0,3,17,1,7,37,1,12,76,
4,26,118,1,36,186,8,53,255,17,82,255,1,0,6,1,0,17,1,1,37,
4,1,76,6,2,130,14,4,186,22,7,255,37,19,255,2,0,6,11,0,17,
26,0,44,44,1,85,85,1,130,118,5,186,171,7,255,202,19,255,6,0,1,
17,0,6,37,0,14,85,1,26,130,1,51,186,3,67,255,5,106,255,19,130,
6,0,0,17,0,0,37,0,0,67,0,0,118,0,0,171,1,0,237,1,0,
255,0,0,6,1,0,17,2,0,37,7,0,67,12,0,118,23,0,171,33,0,
237,49,0,255,69,1,4,7,0,14,19,0,32,39,0,51,73,0,95,119,0,
143,180,0,202,246,0,255,255,0,1,7,0,1,19,0,2,39,0,6,73,0,
8,119,0,8,180,0,17,246,0,32,255,1,1,7,0,0,19,0,0,39,0,
1,73,0,1,114,0,0,172,1,0,246,1,0,255,2,0,7,1,0,19,8,
0,39,14,0,73,26,0,114,44,0,172,67,0,246,106,0,255,130,0,2,6,
0,7,17,0,15,37,0,28,67,0,46,118,1,73,157,0,108,237,4,138,255,
0,0,6,0,0,17,1,0,37,0,1,67,1,1,118,0,2,171,1,2,237,
0,6,255,1,0,4,1,0,17,1,0,37,4,0,67,4,1,118,8,0,171,
14,0,237,17,1,255,6,0,6,11,0,22,26,0,37,51,0,76,85,0,118,
130,0,171,186,0,237,237,0,255,6,0,1,17,0,2,37,0,8,67,0,14,
118,0,22,171,0,37,237,1,51,255,1,67};

void main()
{
   
	Init_Video(320, 200, SFG_SCREEN_RESOLUTION_X, SFG_SCREEN_RESOLUTION_Y);
	SetPalette_Video(cur_pal);
	ticks = 0;
	//ret = Init_Audio();
	//if (ret == 1) return 1;
	
	//Init_CD();
	
	//BOOL success = Play_CD_Track(2, LOOP_CDDA);


	SFG_init();

	do
	{
		state0 = ReadJoyPort(0);
		
		SFG_mainLoopBody();
		
		//WriteChunkyPixels(&temprp2,80,40,80+SFG_SCREEN_RESOLUTION_X,40+SFG_SCREEN_RESOLUTION_Y,(uint8_t*)gfxbuf, SFG_SCREEN_RESOLUTION_X);
		
		UpdateScreen_Video_partial(80, 40, SFG_SCREEN_RESOLUTION_X, SFG_SCREEN_RESOLUTION_Y);
		ticks++;
	} while (1);
}
