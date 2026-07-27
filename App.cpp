#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <commdlg.h>
#include <windowsx.h>
#include <stdint.h>

#pragma comment(lib, "winmm.lib")

HHOOK keyboardHook;
const char *defaultSound = "click.wav";

typedef struct {
    DWORD vk;
    char *path;
} KeyMapEntry;

KeyMapEntry *keymap = NULL;
size_t keymap_count = 0;

volatile int awaiting_key_for_map = 0;

// GUI globals
#define WM_REFRESH_MAPPINGS (WM_APP + 1)
#define WM_TOGGLE_VISIBILITY (WM_APP + 2)

HWND mappingWindow = NULL;
HWND hListBox = NULL;

void post_refresh_gui()
{
    if (mappingWindow && IsWindow(mappingWindow))
        PostMessage(mappingWindow, WM_REFRESH_MAPPINGS, 0, 0);
}

// --- Simple software mixer ---
#define MIX_RATE 44100
#define MIX_CHANNELS 1
#define MIX_BITS 16
#define MAX_VOICES 32
#define MIX_BUFF_SAMPLES 2048

typedef struct {
    int16_t *data;
    size_t samples;
    int sampleRate;
    int channels;
} Sample;

typedef struct {
    Sample *s;
    size_t pos;
    int active;
} Voice;

static Voice voices[MAX_VOICES];
static CRITICAL_SECTION mixerLock;
static HWAVEOUT hWaveOut = NULL;
static WAVEHDR waveHeaders[2];
static short *mixBuffers[2];
static int currentBuffer = 0;
static HANDLE hBufferEvent = NULL;

typedef struct SampleCacheEntry { char *path; Sample sample; struct SampleCacheEntry *next; } SampleCacheEntry;
static SampleCacheEntry *sampleCache = NULL;

// clamp helper
static inline int clamp_int(int v) { if (v > 32767) return 32767; if (v < -32768) return -32768; return v; }

const char *get_sound_for_key(DWORD vk)
{
    for (size_t i = 0; i < keymap_count; ++i)
    {
        if (keymap[i].vk == vk && keymap[i].path && keymap[i].path[0])
            return keymap[i].path;
    }

    if (vk == VK_SPACE)
        return "space.wav";
    if (vk == VK_RETURN)
        return "enter.wav";
    if (vk == VK_BACK)
        return "backspace.wav";

    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
        return defaultSound;

    return defaultSound;
}

void set_mapping(DWORD vk, const char *path)
{
    for (size_t i = 0; i < keymap_count; ++i)
    {
        if (keymap[i].vk == vk)
        {
            free(keymap[i].path);
            keymap[i].path = _strdup(path);
            post_refresh_gui();
            return;
        }
    }

    keymap = (KeyMapEntry*)realloc(keymap, sizeof(*keymap) * (keymap_count + 1));
    keymap[keymap_count].vk = vk;
    keymap[keymap_count].path = _strdup(path);
    keymap_count++;
    post_refresh_gui();
}

void save_keymap()
{
    FILE *f = fopen("keymap.json", "w");
    if (!f) return;
    fprintf(f, "{\n");
    for (size_t i = 0; i < keymap_count; ++i)
    {
        fprintf(f, "  \"%u\": \"%s\"%s\n",
                (unsigned)keymap[i].vk,
                keymap[i].path ? keymap[i].path : "",
                (i + 1 < keymap_count) ? "," : "");
    }
    fprintf(f, "}\n");
    fclose(f);
}

void load_keymap()
{
    FILE *f = fopen("keymap.json", "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    char *p = buf;
    while (p && *p)
    {
        while (*p && (*p < '0' || *p > '9')) p++;
        if (!*p) break;
        unsigned int vk = (unsigned int)strtoul(p, &p, 10);
        while (*p && *p != '"') p++;
        if (!*p) break;
        p++; // skip quote
        char *start = p;
        while (*p && *p != '"') p++;
        if (!*p) break;
        size_t len = p - start;
        char *val = (char*)malloc(len + 1);
        memcpy(val, start, len);
        val[len] = '\0';
        set_mapping(vk, val);
        free(val);
    }

    free(buf);
}

DWORD WINAPI RemapThreadProc(LPVOID param)
{
    DWORD vk = (DWORD)(ULONG_PTR)param;
    OPENFILENAMEA ofn;
    CHAR szFile[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "WAV Files\0*.wav\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

    if (GetOpenFileNameA(&ofn))
    {
        set_mapping(vk, szFile);
        save_keymap();
        printf("Mapped key %u -> %s\n", vk, szFile);
    }

    awaiting_key_for_map = 0;
    return 0;
}

BOOL WINAPI ConsoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT)
    {
        if (keyboardHook)
            UnhookWindowsHookEx(keyboardHook);
        printf("Exiting...\n");
        exit(0);
    }
    return TRUE;
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        KBDLLHOOKSTRUCT *kbd = (KBDLLHOOKSTRUCT *)lParam;

        if (wParam == WM_KEYDOWN)
        {
            // If user pressed F2, enter remapping mode
            if (kbd->vkCode == VK_F2)
            {
                if (!awaiting_key_for_map)
                {
                    awaiting_key_for_map = 1;
                    printf("Remap mode: press the key you want to change, then choose a WAV file.\n");
                }
                return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
            }

            if (awaiting_key_for_map)
            {
                // capture this key for remapping
                DWORD vk = kbd->vkCode;
                // spawn file dialog on separate thread
                CreateThread(NULL, 0, RemapThreadProc, (LPVOID)(ULONG_PTR)vk, 0, NULL);
                // awaiting_key_for_map will be cleared by thread
                return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
            }

            const char *sound = get_sound_for_key(kbd->vkCode);
            printf("Key Code: %lu -> %s\n", kbd->vkCode, sound);

            // play via software mixer to allow overlapping short sounds
            // mixer_play will be defined below
            extern void mixer_play(const char*);
            mixer_play(sound);
        }
    }

    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

// --- Mixer implementation ---
Sample *load_wav(const char *path)
{
    // search cache
    for (SampleCacheEntry *e = sampleCache; e; e = e->next) if (strcmp(e->path, path) == 0) return &e->sample;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    // minimal WAV parser
    char riff[4]; fread(riff,1,4,f);
    uint32_t overall_size; fread(&overall_size,4,1,f);
    char wave[4]; fread(wave,1,4,f);
    if (memcmp(riff,"RIFF",4) || memcmp(wave,"WAVE",4)) { fclose(f); return NULL; }

    // read chunks
    uint16_t audioFormat=0; uint16_t channels=0; uint32_t sampleRate=0; uint16_t bitsPerSample=0;
    uint32_t dataSize=0; long dataPos=0;
    while (!feof(f)) {
        char id[4]; if (fread(id,1,4,f) != 4) break;
        uint32_t size; if (fread(&size,4,1,f) != 1) break;
        if (memcmp(id,"fmt ",4)==0) {
            fread(&audioFormat,2,1,f);
            fread(&channels,2,1,f);
            fread(&sampleRate,4,1,f);
            fseek(f,6,SEEK_CUR); // skip byteRate and blockAlign
            fread(&bitsPerSample,2,1,f);
            if (size > 16) fseek(f, size-16, SEEK_CUR);
        } else if (memcmp(id,"data",4)==0) {
            dataSize = size;
            dataPos = ftell(f);
            fseek(f, size, SEEK_CUR);
        } else {
            fseek(f, size, SEEK_CUR);
        }
    }

    if (dataSize == 0 || audioFormat != 1 || bitsPerSample != 16) { fclose(f); return NULL; }

    size_t samples = dataSize / (channels * 2);
    int16_t *buf = (int16_t*)malloc(samples * sizeof(int16_t));
    if (!buf) { fclose(f); return NULL; }
    fseek(f, dataPos, SEEK_SET);
    if (channels == 1) {
        fread(buf, 2, samples, f);
    } else {
        // convert stereo to mono by averaging
        for (size_t i=0;i<samples;i++) {
            int16_t s1,s2; fread(&s1,2,1,f); fread(&s2,2,1,f);
            int v = ((int)s1 + (int)s2) / 2;
            buf[i] = (int16_t)v;
        }
    }
    fclose(f);

    // add to cache
    SampleCacheEntry *e = (SampleCacheEntry*)malloc(sizeof(*e));
    e->path = _strdup(path);
    e->sample.data = buf;
    e->sample.samples = samples;
    e->sample.sampleRate = sampleRate;
    e->sample.channels = channels;
    e->next = sampleCache;
    sampleCache = e;
    return &e->sample;
}

void mixer_fill_and_submit(int idx)
{
    short *out = mixBuffers[idx];
    memset(out, 0, MIX_BUFF_SAMPLES * sizeof(short));

    EnterCriticalSection(&mixerLock);
    for (int v=0; v<MAX_VOICES; ++v) {
        if (!voices[v].active || !voices[v].s) continue;
        Sample *s = voices[v].s;
        for (size_t i=0; i<MIX_BUFF_SAMPLES; ++i) {
            size_t pos = voices[v].pos + i;
            if (pos >= s->samples) { continue; }
            int mixed = out[i] + s->data[pos];
            out[i] = (short)clamp_int(mixed);
        }
        voices[v].pos += MIX_BUFF_SAMPLES;
        if (voices[v].pos >= voices[v].s->samples) {
            voices[v].active = 0;
        }
    }
    LeaveCriticalSection(&mixerLock);

    // prepare header
    WAVEHDR *wh = &waveHeaders[idx];
    wh->lpData = (LPSTR)out;
    wh->dwBufferLength = MIX_BUFF_SAMPLES * sizeof(short);
    wh->dwFlags = 0;
    waveOutPrepareHeader(hWaveOut, wh, sizeof(*wh));
    waveOutWrite(hWaveOut, wh, sizeof(*wh));
}

DWORD WINAPI MixerThread(LPVOID param)
{
    // fill initial two buffers
    mixer_fill_and_submit(0);
    mixer_fill_and_submit(1);

    // wait for buffers to be done and refill when finished
    while (1) {
        // wait on buffer event signaled via callback
        WaitForSingleObject(hBufferEvent, INFINITE);
        // find which header is done
        for (int i=0;i<2;i++) {
            if (waveHeaders[i].dwFlags & WHDR_DONE) {
                waveOutUnprepareHeader(hWaveOut, &waveHeaders[i], sizeof(waveHeaders[i]));
                mixer_fill_and_submit(i);
            }
        }
    }
    return 0;
}

void CALLBACK waveOutProcCallback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    if (uMsg == WOM_DONE) {
        SetEvent(hBufferEvent);
    }
}

int mixer_init()
{
    InitializeCriticalSection(&mixerLock);
    hBufferEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    WAVEFORMATEX wf = {0};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = MIX_CHANNELS;
    wf.nSamplesPerSec = MIX_RATE;
    wf.wBitsPerSample = MIX_BITS;
    wf.nBlockAlign = (wf.wBitsPerSample/8) * wf.nChannels;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    MMRESULT r = waveOutOpen(&hWaveOut, WAVE_MAPPER, &wf, (DWORD_PTR)waveOutProcCallback, 0, CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) return 0;

    mixBuffers[0] = (short*)malloc(MIX_BUFF_SAMPLES * sizeof(short));
    mixBuffers[1] = (short*)malloc(MIX_BUFF_SAMPLES * sizeof(short));
    memset(voices, 0, sizeof(voices));

    CreateThread(NULL, 0, MixerThread, NULL, 0, NULL);
    return 1;
}

void mixer_play(const char *path)
{
    if (!path) return;
    Sample *s = load_wav(path);
    if (!s) return;

    EnterCriticalSection(&mixerLock);
    for (int i=0;i<MAX_VOICES;i++) {
        if (!voices[i].active) {
            voices[i].s = s;
            voices[i].pos = 0;
            voices[i].active = 1;
            break;
        }
    }
    LeaveCriticalSection(&mixerLock);
}

// --- GUI and main ---
LRESULT CALLBACK MappingWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        hListBox = CreateWindowExA(0, "LISTBOX", NULL,
                                   WS_CHILD | WS_VISIBLE | LBS_HASSTRINGS | WS_VSCROLL | WS_BORDER,
                                   10, 10, 460, 240, hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);
        return 0;
    case WM_REFRESH_MAPPINGS:
        if (hListBox)
        {
            SendMessageA(hListBox, LB_RESETCONTENT, 0, 0);
            char buf[1024];
            for (size_t i = 0; i < keymap_count; ++i)
            {
                snprintf(buf, sizeof(buf), "%u -> %s", (unsigned)keymap[i].vk, keymap[i].path ? keymap[i].path : "");
                SendMessageA(hListBox, LB_ADDSTRING, 0, (LPARAM)buf);
            }
        }
        return 0;
    case WM_TOGGLE_VISIBILITY:
        if (IsWindowVisible(hwnd))
            ShowWindow(hwnd, SW_HIDE);
        else
            ShowWindow(hwnd, SW_SHOW);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

DWORD WINAPI GuiThreadProc(LPVOID param)
{
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = MappingWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "KeymapWindowClass";
    RegisterClassA(&wc);

    mappingWindow = CreateWindowA(wc.lpszClassName, "Key Mappings",
                                  WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 500, 320,
                                  NULL, NULL, wc.hInstance, NULL);

    if (mappingWindow)
    {
        // Start hidden
        ShowWindow(mappingWindow, SW_HIDE);
        // initial populate
        PostMessage(mappingWindow, WM_REFRESH_MAPPINGS, 0, 0);

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && argv[1] != NULL && strlen(argv[1]) > 0)
    {
        defaultSound = argv[1];
    }

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    // load mappings, initialize mixer, and start GUI thread
    load_keymap();
    if (!mixer_init()) {
        printf("Warning: mixer failed to initialize. Sounds will not play.\n");
    }
    CreateThread(NULL, 0, GuiThreadProc, NULL, 0, NULL);

    keyboardHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        KeyboardProc,
        GetModuleHandle(NULL),
        0
    );

    if (keyboardHook == NULL)
    {
        printf("Failed to install keyboard hook.\n");
        return 1;
    }

    printf("Keyboard Sound App Running...\n");
    printf("Press Ctrl+C in the console to stop.\n");

    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(keyboardHook);

    return 0;
}
