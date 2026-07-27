# Keyboard Sound Effect App

This project plays WAV sound effects when keys are pressed on Windows (low-level keyboard hook).

To generate example sound files, run the included PowerShell generator:

```powershell
# From project folder in PowerShell:
.\generate_sounds.ps1
```

Then run the app (build with `gcc` or `cl`):

```powershell
# Build (MinGW):
gcc App.c -o App.exe -lwinmm

# Run (uses example WAVs in the folder):
.\App.exe

# Or specify a default sound file:
.\App.exe "C:\full\path\to\my_click.wav"
```

Place any custom WAVs in this folder or pass full paths. Use Ctrl+C in the terminal to stop the app.

Remapping keys at runtime


GUI window


Helper script

You can also map keys from the command line using `map_key.ps1`:

```powershell
# Map the A key to a WAV file
.\map_key.ps1 -Key A -File "C:\path\to\a_sound.wav"

# Map Space to a file
.\map_key.ps1 -Key Space -File "space.wav"
```

The script accepts friendly key names like `A`-`Z`, `0`-`9`, `Space`, `Enter`, `Backspace`, and `F1`-`F12`.

`keymap.json` format

The file stores mappings as a simple JSON object where keys are virtual-key codes and values are file paths, for example:

```
{
	"32": "space.wav",
	"65": "my_a_sound.wav"
}
```

You can edit the file by hand and restart the app, or use `F2` to change mappings interactively.
# Keyboard Sound Effect App

A lightweight Windows utility that plays short WAV sound effects on keyboard events. It is designed for low-latency feedback with per-key mappings, a simple GUI viewer, and an interactive remapping flow.

Features
--------

- Plays 16-bit PCM WAV files on keypress using a small software mixer (supports overlapping short sounds).
- Per-key mapping persisted to `keymap.json`.
- Interactive remapping: press `F2`, press the key to remap, then choose a WAV file.
- Mappings viewer: press `F3` to toggle a small window that shows current mappings.
- Command-line helper `map_key.ps1` to update `keymap.json` using friendly key names.

Quick Start
-----------

1. Generate example sounds (optional):

```powershell
# From the project folder:
.\generate_sounds.ps1
```

2. Build the app:

```powershell
# MinGW (g++)
g++ App.cpp -o App.exe -lwinmm -lcomdlg32 -luser32

# MSVC (Developer Command Prompt)
# cl App.cpp /link winmm.lib comdlg32.lib user32.lib
```

3. Run:

```powershell
.\App.exe
```

4. Controls while running:

- `F2` — Enter remapping mode. Press the target key, then pick a WAV file.
- `F3` — Toggle the mappings window.
- `Ctrl+C` — Stop the app.

Configuration: `keymap.json`
---------------------------

Mappings are stored in `keymap.json` as a JSON object where each property key is a virtual-key (VK) code and the value is the file path to the WAV. Example:

```json
{
  "32": "space.wav",
  "65": "A.wav"
}
```

Use the helper script to map by friendly name:

```powershell
.\map_key.ps1 -Key A -File "a_sound.wav"
.\map_key.ps1 -Key Space -File "space.wav"
```

Implementation notes
--------------------

- The mixer expects 16-bit PCM WAVs. Stereo files are averaged to mono.
- Mixer sample rate is fixed to 44100 Hz; files with other sample rates may not play perfectly.
- The app uses Win32 APIs (low-level keyboard hook, `GetOpenFileName`, `waveOut`) and compiles with standard Windows toolchains.

Files
-----

- `App.c` — main application source
- `generate_sounds.ps1` — small generator for example WAVs
- `map_key.ps1` — map friendly key names to `keymap.json`
- `keymap.json` — created at runtime to store mappings

Contributing
------------

Pull requests and issues are welcome. Consider adding tests or improvements for audio resampling, volume controls, or a richer UI.

License
-------

Add a `LICENSE` file to clarify reuse terms (MIT is a good default for small utilities).


