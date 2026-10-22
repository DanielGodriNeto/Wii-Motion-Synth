#include <gccore.h>
#include <wiiuse/wpad.h>
#include <asndlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>

#define SAMPLERATE 48000
#define SAMPLES_PER_BUFFER 1024
#define SINE_LUT_SIZE 2048
#define SINE_LUT_MASK (SINE_LUT_SIZE - 1)
#define PI 3.14159265358979323846f

typedef enum { WAVE_SINE, WAVE_SQUARE, WAVE_SAWTOOTH, WAVE_TRIANGLE, WAVE_NOISE, WAVE_PULSE, WAVE_PULSE_NARROW } WaveType;
static volatile WaveType g_current_wave = WAVE_SINE;

static s16 sine_table[SINE_LUT_SIZE] ATTRIBUTE_ALIGN(32);
static s16 audio_buffers[2][SAMPLES_PER_BUFFER] ATTRIBUTE_ALIGN(32);
static volatile u8 current_buffer = 0;
static u32 phase_accumulator = 0;

static volatile u32 g_phase_step = 0;
static volatile u32 g_vol_scale = 204; 

int map_pitch_to_freq(float pitch) {
    return 100 + ((pitch + 90.0f) / 180.0f) * 900;
}

int map_roll_to_vol(float roll) {
    int vol = 255 - (abs((int)roll) * 255 / 180);
    if(vol < 0) vol = 0;
    if(vol > 255) vol = 255;
    return vol;
}

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
        } else if (wave == WAVE_TRIANGLE) {
            s32 half_size = SINE_LUT_SIZE / 2;
            s32 pos = lut_index % half_size;
            s32 val = (pos * (s32)vol * 255) / half_size;
            sample = (lut_index < half_size) ? (val - (vol * 127)) : ((vol * 127) - val);
        } else if (wave == WAVE_NOISE) {
            sample = ((rand() % 256) - 128) * vol;
        } else if (wave == WAVE_PULSE) {
            // Pulse wave with 25% duty cycle (SINE_LUT_SIZE / 4)
            sample = (lut_index < (SINE_LUT_SIZE / 4)) ? (vol * 127) : -(vol * 127);
        } else if (wave == WAVE_PULSE_NARROW) {
            // 12.5% duty cycle (SINE_LUT_SIZE / 8)
            sample = (lut_index < (SINE_LUT_SIZE / 8)) ? (vol * 127) : -(vol * 127);
        }       

        buf[i] = (s16)sample;
        phase_accumulator += step;
    }

    DCFlushRange(buf, SAMPLES_PER_BUFFER * sizeof(s16));
    ASND_AddVoice(voice, buf, SAMPLES_PER_BUFFER * sizeof(s16));
    current_buffer ^= 1;
}

const int SCALE_FREQS[] = {
    131, 147, 165, 196, 220, 262, 294, 330, 392, 440, 523, 587, 659, 784, 880, 1047
};
#define NUM_NOTES (sizeof(SCALE_FREQS)/sizeof(SCALE_FREQS[0]))

int quantize_frequency(int raw_freq) {
    int closest_freq = SCALE_FREQS[0];
    int min_diff = 9999;
    for(int i = 0; i < NUM_NOTES; i++) {
        int diff = abs(raw_freq - SCALE_FREQS[i]);
        if(diff < min_diff) {
            min_diff = diff;
            closest_freq = SCALE_FREQS[i];
        }
    }
    return closest_freq;
}

// --- FEATURE: VISUAL OSCILLOSCOPE (ZOOMED IN) ---
void draw_oscilloscope(short* audio_buffer, u32 buffer_size) {
    // Zoom in by drawing only 1/4th of the buffer
    u32 display_samples = buffer_size / 4; 
    
    GX_Begin(GX_LINESTRIP, GX_VTXFMT0, display_samples);
    for(u32 i = 0; i < display_samples; i++) {
        float x = (float)i / (float)display_samples * 640.0f;
        float y = 240.0f - ((float)audio_buffer[i] / 32768.0f * 150.0f); 
        GX_Position3f32(x, y, 0.0f);
        GX_Color4u8(0, 255, 255, 255); // Cyan waveform
    }
    GX_End();
}

int main(int argc, char **argv) {
    srand(time(NULL));

    init_sine_table();
    memset(audio_buffers, 0, sizeof(audio_buffers));
    DCFlushRange(audio_buffers, sizeof(audio_buffers));

    g_phase_step = (u32)(((440.0f * SINE_LUT_SIZE) / SAMPLERATE) * 65536.0f);
    g_vol_scale = (u32)(0.8f * 256.0f);

    VIDEO_Init();
    GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
    void *frameBuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(frameBuffer);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    // Initialize the text console directly over the framebuffer
    console_init(frameBuffer, 0, 0, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    void *gp_fifo = memalign(32, 256 * 1024);
    memset(gp_fifo, 0, 256 * 1024);
    GX_Init(gp_fifo, 256 * 1024);

    GXColor background = {0, 0, 0, 255}; // Explicitly define Black
    GX_SetCopyClear(background, 0x00ffffff);
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetDispCopyYScale((f32)rmode->xfbHeight / (f32)rmode->efbHeight);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, rmode->xfbHeight);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight==2*rmode->xfbHeight)?GX_TRUE:GX_FALSE));
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetDispCopyGamma(GX_GM_1_0);

    Mtx44 ortho;
    guOrtho(ortho, 0, 480, 0, 640, 0, 300);
    GX_LoadProjectionMtx(ortho, GX_ORTHOGRAPHIC);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    GX_SetNumChans(1);
    // Initialize the color channel to pull directly from the vertex
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    
    GX_SetNumTexGens(0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);  

    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC);

    ASND_Init();
    ASND_Pause(0);
    ASND_SetVoice(0, VOICE_MONO_16BIT, SAMPLERATE, 0, audio_buffers[0], SAMPLES_PER_BUFFER * sizeof(s16), 255, 255, audio_callback);

    bool show_oscilloscope = true; 
    bool show_welcome = true; // New Welcome Screen State
    int lfo_phase = 0;

    // Clear the console text before starting
    printf("\x1b[2J"); 

    while(1) {
        WPAD_ScanPads();
        u32 pressed = WPAD_ButtonsDown(0);
        u32 held = WPAD_ButtonsHeld(0);
        WPADData *data = WPAD_Data(0);

        if (pressed & WPAD_BUTTON_HOME) break;

        // --- WELCOME SCREEN LOGIC ---
        if (show_welcome) {
            if (pressed & WPAD_BUTTON_A) {
                show_welcome = false; // Exit welcome screen
                printf("\x1b[2J");    // Clear the text
            } else {
                printf("\x1b[4;1H"); 
                printf("             ========================================\n");
                printf("                       WII MOTION SYNTH              \n");
                printf("             ========================================\n\n");
                printf("      Turn your Wiimote into an interactive audio synthesizer.\n\n");
                printf("      [ HOW TO PLAY ]\n");
                printf("      - Tilt Up/Down (Pitch) to change the Frequency (Note).\n");
                printf("      - Twist Left/Right (Roll) to change the Volume.\n\n");
                printf("      [ CONTROLS ]\n");
                printf("      Hold B    : Snap-to-Scale\n");
                printf("      Hold A    : Square Wave\n");
                printf("      Hold 1    : Sawtooth Wave\n");
                printf("      Hold 2    : Octave Shift\n");
                printf("      D-Pad UP  : Triangle Wave\n");
                printf("      D-Pad DWN : White Noise\n");
                printf("      D-Pad LFT : Pulse Wave\n");
                printf("      D-Pad RGT : Narrow Pulse\n");
                printf("      Hold PLUS : Mute Gate\n");
                printf("      MINUS     : Toggle Oscilloscope / Stats UI\n");
                printf("      Nunchuk   : Joystick LFO (Vibrato & Tremolo)\n\n\n");
                printf("                    >> PRESS 'A' TO START <<\n");

                g_vol_scale = 0; // Keep the synth completely silent 
                VIDEO_WaitVSync();
                continue; // Skip the rest of the audio/render loop
            }
        }

        // --- MAIN APP LOGIC ---
        
        // Display Toggle
        if (pressed & WPAD_BUTTON_MINUS) {
            show_oscilloscope = !show_oscilloscope;
            if (!show_oscilloscope) {
                printf("\x1b[2J"); // Clear the console text when switching to UI mode
            }
        }

        // Waveforms
        if (held & WPAD_BUTTON_UP) g_current_wave = WAVE_TRIANGLE;
        else if (held & WPAD_BUTTON_DOWN) g_current_wave = WAVE_NOISE;
        else if (held & WPAD_BUTTON_LEFT) g_current_wave = WAVE_PULSE;
        else if (held & WPAD_BUTTON_RIGHT) g_current_wave = WAVE_PULSE_NARROW;
        else if (held & WPAD_BUTTON_1) g_current_wave = WAVE_SAWTOOTH;
        else if (held & WPAD_BUTTON_A) g_current_wave = WAVE_SQUARE;
        else g_current_wave = WAVE_SINE;

        // Raw, unsmoothed accelerometer data
        int target_freq = map_pitch_to_freq(data->orient.pitch);
        int target_vol = map_roll_to_vol(data->orient.roll);

        if (held & WPAD_BUTTON_B) {
            target_freq = quantize_frequency(target_freq);
        }

        // Octave Shift modifier
        if (held & WPAD_BUTTON_2) {
            target_freq *= 2; 
        }

        // Nunchuk LFO
        if (data->exp.type == WPAD_EXP_NUNCHUK) {
            int joy_y = (int)data->exp.nunchuk.js.pos.y - (int)data->exp.nunchuk.js.center.y;
            int joy_x = (int)data->exp.nunchuk.js.pos.x - (int)data->exp.nunchuk.js.center.x;
            
            if (abs(joy_y) > 10) target_freq += (sine_table[lfo_phase] * joy_y) / 60000; 
            if (abs(joy_x) > 10) target_vol -= abs((sine_table[lfo_phase] * joy_x) / 60000);
            
            lfo_phase = (lfo_phase + 170) % SINE_LUT_SIZE;
        }

        // Mute Gate modifier
        if (held & WPAD_BUTTON_PLUS) {
            target_vol = 0;
        }

        g_phase_step = (u32)(((float)target_freq * SINE_LUT_SIZE / SAMPLERATE) * 65536.0f);
        g_vol_scale = target_vol;

        // --- RENDER LOGIC ---
        if (show_oscilloscope) {
            // Draw hardware graphics
            draw_oscilloscope(audio_buffers[current_buffer ^ 1], SAMPLES_PER_BUFFER);
            GX_DrawDone();
            GX_CopyDisp(frameBuffer, GX_TRUE);
        } else {
            // Determine the current wave name for the text UI
            const char* wave_name = "SINE";
            if (g_current_wave == WAVE_SQUARE) wave_name = "SQUARE";
            else if (g_current_wave == WAVE_SAWTOOTH) wave_name = "SAWTOOTH";
            else if (g_current_wave == WAVE_TRIANGLE) wave_name = "TRIANGLE";
            else if (g_current_wave == WAVE_NOISE) wave_name = "NOISE";
            else if (g_current_wave == WAVE_PULSE) wave_name = "PULSE";
            else if (g_current_wave == WAVE_PULSE_NARROW) wave_name = "NARROW";

            // Draw CPU Text UI
            printf("\x1b[1;1H"); 
            printf("--- WII MOTION SYNTH ---\n\n");
            printf("Current Wave: %-8s\n", wave_name);
            printf("Frequency: %4d Hz     \n", target_freq);
            printf("Volume: %3d%%          \n\n", (target_vol * 100) / 255);
            
            printf("--- SENSOR TELEMETRY ---\n");
            printf("Pitch (Up/Down) : %6.1f\n", data->orient.pitch);
            printf("Roll  (Twist)   : %6.1f\n", data->orient.roll);
            printf("Yaw   (Rotation): %6.1f\n\n", data->orient.yaw);
            
            printf("--- CONTROLS ---\n");
            printf("Hold B    : Snap-to-Scale\n");
            printf("Hold A    : Square Wave\n");
            printf("Hold 1    : Sawtooth Wave\n");
            printf("Hold 2    : Octave Shift\n");
            printf("D-Pad UP  : Triangle Wave\n");
            printf("D-Pad DWN : White Noise\n");
            printf("D-Pad LFT : Pulse Wave\n");
            printf("D-Pad RGT : Narrow Pulse\n");
            printf("Hold PLUS : Mute Gate\n");
            printf("MINUS     : Toggle Visualizer\n");
            printf("HOME      : Exit\n");
        }

        VIDEO_WaitVSync();
    }

    ASND_StopVoice(0);
    WPAD_Shutdown();
    return 0;
}