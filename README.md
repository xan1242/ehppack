# Yu-Gi-Oh! Tag Force EHP Archive Tool
This utility is designed to accurately manipulate the EHP archives found in the PSP Yu-Gi-Oh! Tag Force games.

Currently command line only.

# Features
- Extraction of EHP to folder with correct filenames and sizes
- Packing of a folder back to EHP fully compatible with the game

# Compatibility
Right now it was only tested with Tag Force 6 (packing and extracting) but it can also extract and repack files from all other Tag Force games.

# Usage
Extracting: `ehppack InFileName [OutFolder]`

Packing: `ehppack -p InFolder [OutFileName]`

If the optional (in []) parameter isn't specified, it'll reuse the input name.

You may also drag and drop an EHP to the binary to extract a file if you're on a supported OS.
