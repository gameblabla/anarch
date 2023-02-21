/**
  @file main_sdl.c

  This is an SDL2 implementation of the game front end. It can be used to
  compile a native executable or a transpiled JS browser version with
  emscripten.

  This frontend is not strictly minimal, it could be reduced a lot. If you want
  a learning example of frontend, look at another, simpler one, e.g. terminal.

  To compile with emscripten run:

  emcc ./main_sdl.c -s USE_SDL=2 -O3 --shell-file HTMLshell.html -o game.html

  by Miloslav Ciz (drummyfish), 2019

  Released under CC0 1.0 (https://creativecommons.org/publicdomain/zero/1.0/)
  plus a waiver of all other intellectual property. The goal of this work is to
  be and remain completely in the public domain forever, available for any use
  whatsoever.
*/
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <malloc.h>
#include <stdlib.h>
#include <proc_ui/procui.h>
#include <vpad/input.h>
#include <coreinit/thread.h>
#include <coreinit/systeminfo.h>
#include <coreinit/time.h>
#include <coreinit/screen.h>
#include <coreinit/cache.h>
#include <whb/log_cafe.h>
#include <whb/log_udp.h>
#include <whb/log.h>
#include <whb/proc.h>

#include <coreinit/core.h>
#include <coreinit/dynload.h>
#include <coreinit/foreground.h>
#include <proc_ui/procui.h>
#include <whb/proc.h>

#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__) || defined(__NT__) || defined(__APPLE__)
  #define SFG_OS_IS_MALWARE 1
#endif

// #define SFG_START_LEVEL 1
// #define SFG_QUICK_WIN 1
// #define SFG_IMMORTAL 1
// #define SFG_ALL_LEVELS 1
// #define SFG_UNLOCK_DOOR 1
// #define SFG_REVEAL_MAP 1
// #define SFG_INFINITE_AMMO 1
// #define SFG_TIME_MULTIPLIER 512
// #define SFG_CPU_LOAD(percent) printf("CPU load: %d%\n",percent);
// #define GAME_LQ

char *scrBuffer;
char *screenBuffer;
uint32_t buttons_hold; //Held buttons
uint32_t buttons_pressed; //Pressed buttons
uint32_t buttons_released; //Released buttons
int screen_buf0_size = 0;
int screen_buf1_size = 0;
int scr_buf0_size = 0;
int scr_buf1_size = 0;
int cur_buf1;

void flipBuffers();

static void clearBuffers() {
    for(int i = 0; i < 2; i++) {
        OSScreenClearBufferEx(0, 0);
        OSScreenClearBufferEx(1, 0);
        flipBuffers();
    }
}

static void initDraw(char* buf, int size0, int size1) {
	scrBuffer = buf;
	scr_buf0_size = size0;
	scr_buf1_size = size1;

	clearBuffers();
	uint32_t *screen2 = scrBuffer + scr_buf0_size;
	OSScreenPutPixelEx(1, 0, 0, 0xABCDEFFF);

	if (screen2[0] == 0xABCDEFFF) cur_buf1 = 0;
	else cur_buf1 = 1;
}

void flipBuffers() {
	//Flush the cache
	DCFlushRange(scrBuffer + scr_buf0_size, scr_buf1_size);
	DCFlushRange(scrBuffer, scr_buf0_size);
	//Flip the buffer
	OSScreenFlipBuffersEx(0);
	OSScreenFlipBuffersEx(1);
	cur_buf1 = !cur_buf1;
}

static void drawString(int x, int line, char* string) {
	OSScreenPutFontEx(0, x, line, string);
	OSScreenPutFontEx(1, x, line, string);
}

#define SFG_FPS 60
#define SFG_LOG(str) puts(str);
#define SFG_SCREEN_RESOLUTION_X 640
#define SFG_SCREEN_RESOLUTION_Y 480
#define SFG_DITHERED_SHADOW 1
#define SFG_DIMINISH_SPRITES 1
#define SFG_HEADBOB_SHEAR (-1 * SFG_SCREEN_RESOLUTION_Y / 80)
#define SFG_BACKGROUND_BLUR 1
    

/*
  SDL is easier to play thanks to nice controls so make the player take full
  damage to make it a bit harder.
*/
#define SFG_PLAYER_DAMAGE_MULTIPLIER 1024

#define SDL_MUSIC_VOLUME 16

#define SDL_ANALOG_DIVIDER 1024

#if !SFG_OS_IS_MALWARE
  #include <signal.h>
#endif


#include <stdio.h>
#include <unistd.h>
#include <SDL2/SDL.h>

#define RGBX_BUFFER 1
#define SDL_AUDIO 1

#include "game.h"
#include "sounds.h"

uint32_t sdlScreen[SFG_SCREEN_RESOLUTION_X * SFG_SCREEN_RESOLUTION_Y]; // ARGB8888

static OSTime start;

// now implement the Anarch API functions (SFG_*)

void SFG_setPixel(uint16_t x, uint16_t y, uint8_t colorIndex)
{
	OSScreenPutPixelEx(0, x + 320, y + 120, paletteRGB565[colorIndex]); // Center it for TV
	OSScreenPutPixelEx(1, x + 107, y, paletteRGB565[colorIndex]); // Gamepad takes fullscreen
}

uint32_t SFG_getTimeMs()
{
	OSTime now;
    now = OSGetSystemTime();
    return (uint32_t)OSTicksToMilliseconds(now - start);
}

void SFG_save(uint8_t data[SFG_SAVE_SIZE])
{
  FILE *f = fopen(SFG_SAVE_FILE_PATH,"wb");

  puts("SDL: opening and writing save file");

  if (f == NULL)
  {
    puts("SDL: could not open the file!");
    return;
  }

  fwrite(data,1,SFG_SAVE_SIZE,f);

  fclose(f);
}

uint8_t SFG_load(uint8_t data[SFG_SAVE_SIZE])
{
#ifndef __EMSCRIPTEN__
  FILE *f = fopen(SFG_SAVE_FILE_PATH,"rb");

  puts("SDL: opening and reading save file");

  if (f == NULL)
  {
    puts("SDL: no save file to open");
  }
  else
  {
    fread(data,1,SFG_SAVE_SIZE,f);
    fclose(f);
  }

  return 1;
#else
  // no saving for web version
  return 0;
#endif
}

void SFG_sleepMs(uint16_t timeMs)
{
	OSSleepTicks(OSMillisecondsToTicks(timeMs));
}

VPADStatus status;
VPADReadError error;
int vpad_fatal = 0;

void SFG_getMouseOffset(int16_t *x, int16_t *y)
{
	*x += status.rightStick.x;
	*y += status.rightStick.y;
	/*
#ifndef __EMSCRIPTEN__

#if !defined(__WIIU__)
  if (mouseMoved)
  {
    int mX, mY;

    SDL_GetMouseState(&mX,&mY);

    *x = mX - SFG_SCREEN_RESOLUTION_X / 2;
    *y = mY - SFG_SCREEN_RESOLUTION_Y / 2;

    SDL_WarpMouseInWindow(window,
      SFG_SCREEN_RESOLUTION_X / 2, SFG_SCREEN_RESOLUTION_Y / 2);
  }
#endif
  if (sdlController != NULL)
  {
    *x +=
      (SDL_GameControllerGetAxis(sdlController,SDL_CONTROLLER_AXIS_RIGHTX) + 
      SDL_GameControllerGetAxis(sdlController,SDL_CONTROLLER_AXIS_LEFTX)) /
      SDL_ANALOG_DIVIDER;

    *y +=
      (SDL_GameControllerGetAxis(sdlController,SDL_CONTROLLER_AXIS_RIGHTY) + 
      SDL_GameControllerGetAxis(sdlController,SDL_CONTROLLER_AXIS_LEFTY)) /
      SDL_ANALOG_DIVIDER;
  }
#endif*/
}

void SFG_processEvent(uint8_t event, uint8_t data)
{
}


int8_t SFG_keyPressed(uint8_t key)
{
  #define b(x) (( (status.hold & VPAD_BUTTON_ ## x ) == VPAD_BUTTON_ ## x))

  switch (key)
  {
    case SFG_KEY_UP: return b(UP); break;
    case SFG_KEY_RIGHT: 
      return b(RIGHT); break;
    case SFG_KEY_DOWN: 
      return b(DOWN); break;
    case SFG_KEY_LEFT: return b(LEFT); break;
    case SFG_KEY_A: return b(A); break;
    case SFG_KEY_B: return b(B); break;
    case SFG_KEY_C: return b(X); break;
    case SFG_KEY_JUMP: return b(Y); break;
    case SFG_KEY_STRAFE_LEFT: return b(L); break;
    case SFG_KEY_STRAFE_RIGHT: return b(R); break;
    case SFG_KEY_MAP: return b(MINUS); break;
    case SFG_KEY_CYCLE_WEAPON: return b(ZL); break;
    case SFG_KEY_MENU: return b(PLUS); break;
    case SFG_KEY_NEXT_WEAPON:
      if (b(ZR))
        return 1;
      else
      return 0;
      break;

    default: return 0; break;
  }

  #undef k
  #undef b
}
  
int running = 1;

void Cleanup()
{
#ifdef SDL_AUDIO
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	SDL_Quit();
#endif

    if (screenBuffer) free(screenBuffer);

/*  Deinit everything */
    OSScreenShutdown();
    WHBProcShutdown();
    
    ProcUIShutdown();
}


int AppRunning() {
	int app = 1;
	if (OSIsMainCore()) {
		switch (ProcUIProcessMessages(true)) {
                case PROCUI_STATUS_EXITING:
                    // Being closed, prepare to exit
                    app = 0;
                    break;
                case PROCUI_STATUS_RELEASE_FOREGROUND:
                    // Free up MEM1 to next foreground app, deinit screen, etc.
                    ProcUIDrawDoneRelease();
                    break;
                case PROCUI_STATUS_IN_FOREGROUND:
                    // Executed while app is in foreground
                    app = 1;
                    break;
                case PROCUI_STATUS_IN_BACKGROUND:
                    OSSleepTicks(OSMillisecondsToTicks(20));
                    break;
            }
	}
	if (running == 0) app = 0;

	return app;
}

void mainLoopIteration()
{
#ifdef SDL_AUDIO
	SDL_Event event;
	SDL_PollEvent(&event);
#endif
	VPADRead(VPAD_CHAN_0, &status, 1, &error);
	
	if (!SFG_mainLoopBody())
		running = 0;

	flipBuffers();
}

uint16_t audioBuff[SFG_SFX_SAMPLE_COUNT];
uint16_t audioPos = 0; // audio position for the next audio buffer fill
uint32_t audioUpdateFrame = 0; // game frame at which audio buffer fill happened

static inline int16_t mixSamples(int16_t sample1, int16_t sample2)
{
  return sample1 + sample2;
}

uint8_t musicOn = 0;
// ^ this has to be init to 0 (not 1), else a few samples get played at start

void audioFillCallback(void *userdata, uint8_t *s, int l)
{
  uint16_t *s16 = (uint16_t *) s;

  for (int i = 0; i < l / 2; ++i)
  {
    s16[i] = musicOn ?
      mixSamples(audioBuff[audioPos], SDL_MUSIC_VOLUME *
      (SFG_getNextMusicSample() - SFG_musicTrackAverages[SFG_MusicState.track]))
      : audioBuff[audioPos];

    audioBuff[audioPos] = 0;
    audioPos = (audioPos < SFG_SFX_SAMPLE_COUNT - 1) ? (audioPos + 1) : 0;
  }

  audioUpdateFrame = SFG_game.frame;
}

void SFG_setMusic(uint8_t value)
{
  switch (value)
  {
    case SFG_MUSIC_TURN_ON: musicOn = 1; break;
    case SFG_MUSIC_TURN_OFF: musicOn = 0; break;
    case SFG_MUSIC_NEXT: SFG_nextMusicTrack(); break;
    default: break;
  }
}

void SFG_playSound(uint8_t soundIndex, uint8_t volume)
{
  uint16_t pos = (audioPos +
    ((SFG_game.frame - audioUpdateFrame) * SFG_MS_PER_FRAME * 8)) %
    SFG_SFX_SAMPLE_COUNT;

  uint16_t volumeScale = 1 << (volume / 37);

  for (int i = 0; i < SFG_SFX_SAMPLE_COUNT; ++i)
  {
    audioBuff[pos] = mixSamples(audioBuff[pos], 
      (128 - SFG_GET_SFX_SAMPLE(soundIndex,i)) * volumeScale);

    pos = (pos < SFG_SFX_SAMPLE_COUNT - 1) ? (pos + 1) : 0;
  }
}

void handleSignal(int signal)
{
  running = 0;
}


int main(int argc, char *argv[])
{
    OSDynLoad_Module mod;
    int aroma = OSDynLoad_Acquire("homebrew_kernel", &mod) == OS_DYNLOAD_OK;
    if (aroma) {
        OSDynLoad_Release(mod);
        ProcUIInit(&OSSavesDone_ReadyToRelease);
        OSEnableHomeButtonMenu(true);
    } else
        return 0;

	SFG_init();

	OSScreenInit();
	screen_buf0_size = OSScreenGetBufferSizeEx(0);
    screen_buf1_size = OSScreenGetBufferSizeEx(1);
    
    screenBuffer =  memalign(0x100, screen_buf0_size + screen_buf1_size);
    OSScreenSetBufferEx(0, screenBuffer);
    OSScreenSetBufferEx(1, (screenBuffer + screen_buf0_size));
    OSScreenEnableEx(0, 1);
    OSScreenEnableEx(1, 1);
    
    initDraw(screenBuffer, screen_buf0_size, screen_buf1_size);
    clearBuffers();
    
#ifdef SDL_AUDIO
	#warning "SDL Audio enabled"
	SDL_Init(SDL_INIT_AUDIO);
	SDL_AudioSpec audioSpec;

	SDL_memset(&audioSpec, 0, sizeof(audioSpec));
	audioSpec.callback = audioFillCallback;
	audioSpec.freq = 8000;
	audioSpec.format = AUDIO_S16SYS;
	audioSpec.channels = 1;
	audioSpec.samples = 256;

	if (SDL_OpenAudio(&audioSpec,NULL) < 0)
		puts("SDL: could not initialize audio");

	for (int16_t i = 0; i < SFG_SFX_SAMPLE_COUNT; ++i)
		audioBuff[i] = 0;

	SDL_PauseAudio(0);
	
	SDL_PumpEvents();

#endif

	running = 1;
	start = OSGetSystemTime();

	while (AppRunning())
	{
		mainLoopIteration();
	}
	
	Cleanup();

	return 0;
}
