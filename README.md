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
