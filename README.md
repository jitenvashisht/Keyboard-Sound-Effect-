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
------------------------

- Press `F2` while the app is running to enter remapping mode.
- After pressing `F2`, press the key you want to remap.
- A file chooser will open; pick a WAV file to assign to that key.
- Mappings are saved to `keymap.json` in the project folder.

GUI window
----------

- Press `F3` while the app is running to toggle a small window that shows current mappings.
- The window updates automatically when mappings change.

Helper script
-------------

You can also map keys from the command line using `map_key.ps1`:

```powershell
# Map the A key to a WAV file
.\map_key.ps1 -Key A -File "C:\path\to\a_sound.wav"

# Map Space to a file
.\map_key.ps1 -Key Space -File "space.wav"
```

The script accepts friendly key names like `A`-`Z`, `0`-`9`, `Space`, `Enter`, `Backspace`, and `F1`-`F12`.

`keymap.json` format
--------------------

The file stores mappings as a simple JSON object where keys are virtual-key codes and values are file paths, for example:

```
{
	"32": "space.wav",
	"65": "my_a_sound.wav"
}
```

You can edit the file by hand and restart the app, or use `F2` to change mappings interactively.
