# Yu-Gi-Oh! Tag Force EHP/EhFolder Archive Tool

This utility is designed to accurately manipulate the EHP/EhFolder archives found in the PSP Yu-Gi-Oh! Tag Force games.

Currently command line only.

# Features

- Extraction of EHP to folder with correct filenames and sizes
- Packing of a folder back to EHP fully compatible with the game

# Compatibility

This tool should be 100% compatible with the official EhFolder specification and any Tag Force game.

# Usage

Extracting: `ehppack InFileName [OutFolder]`

Packing: `ehppack -p InFolder [OutFileName]`

If the optional (in []) parameter isn't specified, it'll reuse the input name.

You may also drag and drop an EHP to the binary to extract a file if you're on a supported OS.

## Windows install script

You may also install this in the Windows' context menu by using the install script.

Simply do the following:

- Put `install.bat` and `install.ps1` next to `ehppack.exe`

- Run `install.bat` as Administrator once (or run `install.ps1` directly with the `Bypass` ExecutionPolicy)

You may need to log out and back in for it to work properly.

The context menu will appear for all `.ehp` files (for unpacking) and all directories (for packing).

The install script installs the binary into `%APPDATA%\ehppack` and adds it to your user's PATH.



Running as Administrator is necessary to write in these registry keys (and subkeys inside):

`HKEY_CLASSES_ROOT\.ehp\shell\ehppack`

`HKEY_CLASSES_ROOT\Directory\shell\ehppack`



# Building with CMake

Use the standard CMake building procedure:

```bash
$ mkdir bin
$ cd bin
$ cmake ..
$ cmake --build .
```

And to install, just do: `# cmake --install .`
