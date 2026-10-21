#include <gccore.h>
#include <wiiuse/wpad.h>
#include <asndlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLERATE 48000
#define SAMPLES_PER_BUFFER 1024
#define SINE_LUT_SIZE 2048
#define SINE_LUT_MASK (SINE_LUT_SIZE - 1)
#define PI 3.14159265358979323846f

// Waveform definitions
typedef enum { WAVE_SINE, WAVE_SQUARE, WAVE_SAWTOOTH } WaveType;
static volatile WaveType g_current_wave = WAVE_SINE;

static s16 sine_table[SINE_LUT_SIZE] ATTRIBUTE_ALIGN(32);
static s16 audio_buffers[2][SAMPLES_PER_BUFFER] ATTRIBUTE_ALIGN(32);
static volatile u8 current_buffer = 0;
static u32 phase_accumulator = 0;

static volatile u32 g_phase_step = 0;
static volatile u32 g_vol_scale = 204; 

void init_sine_table(void) {
    for (int i = 0; i < SINE_LUT_SIZE; i++) {
        float angle = (2.0f * PI * i) / SINE_LUT_SIZE;
        sine_table[i] = (s16)(32000.0f * sinf(angle));
    }
    DCFlushRange(sine_table, sizeof(sine_table));
}

void audio_callback(int voice) {
    s16 *buf = audio_buffers[current_buffer];
    u32 step = g_phase_step;
    u32 vol = g_vol_scale;
    WaveType wave = g_current_wave;

    for (int i = 0; i < SAMPLES_PER_BUFFER; i++) {
        u32 lut_index = (phase_accumulator >> 16) & SINE_LUT_MASK;
        s32 sample = 0;

        if (wave == WAVE_SINE) {
            sample = (sine_table[lut_index] * vol) >> 8;
        } else if (wave == WAVE_SQUARE) {
            sample = (lut_index < (SINE_LUT_SIZE / 2)) ? (vol * 127) : -(vol * 127);
        } else if (wave == WAVE_SAWTOOTH) {
            sample = (((s32)lut_index - (SINE_LUT_SIZE / 2)) * (s32)vol * 255) / SINE_LUT_SIZE;
        }

        buf[i] = (s16)sample;
        phase_accumulator += step;
    }

    DCFlushRange(buf, SAMPLES_PER_BUFFER * sizeof(s16));
    ASND_AddVoice(voice, buf, SAMPLES_PER_BUFFER * sizeof(s16));
    current_buffer ^= 1;
}

int main(int argc, char **argv) {
    void *fb = NULL;
    GXRModeObj *rmode = NULL;

    init_sine_table();
    memset(audio_buffers, 0, sizeof(audio_buffers));
    DCFlushRange(audio_buffers, sizeof(audio_buffers));

    g_phase_step = (u32)(((440.0f * SINE_LUT_SIZE) / SAMPLERATE) * 65536.0f);
    g_vol_scale = (u32)(0.8f * 256.0f);

    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    fb = MEM_K0_TO_K1(SYS_GetArena1Lo());
    SYS_SetArena1Lo(SYS_GetArena1Lo() + VIDEO_GetFrameBufferSize(rmode));

    console_init(fb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(fb);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC);

    ASND_Init();
    ASND_Pause(0);
    ASND_SetVoice(0, VOICE_MONO_16BIT, SAMPLERATE, 0, audio_buffers[0], SAMPLES_PER_BUFFER * sizeof(s16), 255, 255, audio_callback);

    float current_freq = 440.0f;
    float current_amp = 0.8f;
    const char* wave_names[] = {"Sine", "Square", "Sawtooth"};

    while (1) {
        WPAD_ScanPads();
        WPADData *wiimote = WPAD_Data(0);
        u32 pressed = WPAD_ButtonsDown(0);

        if (wiimote != NULL && wiimote->err == WPAD_ERR_NONE) {
            float pitch = wiimote->orient.pitch;
            float roll = wiimote->orient.roll;

            if (pitch < -90.0f) pitch = -90.0f;
            if (pitch > 90.0f) pitch = 90.0f;
            current_freq = 100.0f + (1000.0f - 100.0f) * (pitch - (-90.0f)) / 180.0f;

            if (roll < -90.0f) roll = -90.0f;
            if (roll > 90.0f) roll = 90.0f;
            current_amp = 0.2f + (1.0f - 0.2f) * (roll - (-90.0f)) / 180.0f;
            
            // Switch waveforms
            if (pressed & WPAD_BUTTON_A) g_current_wave = WAVE_SINE;
            if (pressed & WPAD_BUTTON_B) g_current_wave = WAVE_SQUARE;
            if (pressed & WPAD_BUTTON_1) g_current_wave = WAVE_SAWTOOTH;
        }

        g_phase_step = (u32)(((current_freq * SINE_LUT_SIZE) / SAMPLERATE) * 65536.0f);
        g_vol_scale = (u32)(current_amp * 256.0f);

        if (pressed & WPAD_BUTTON_HOME) break;

        printf("\x1b[H"); // Move cursor to top-left instead of clearing the whole screen to prevent flickering
        printf("=== Wii Motion Synth ===\n\n");
        printf("Waveform:  %-10s \n", wave_names[g_current_wave]);
        printf("Frequency: %-7.2f Hz  \n", current_freq);
        printf("Amplitude: %-7.2f    \n\n", current_amp);
        printf("Controls:\n");
        printf(" Tilt Up/Down: Pitch (Freq)\n");
        printf(" Tilt L/R:     Roll  (Vol)\n");
        printf(" A Button:     Sine Wave\n");
        printf(" B Button:     Square Wave\n");
        printf(" 1 Button:     Saw Wave\n\n");
        printf("Press HOME to exit.       \n");

        VIDEO_WaitVSync();
    }

    ASND_StopVoice(0);
    WPAD_Shutdown();
    return 0;
}