# B16Viewer
A viewer for the Commander X16 using the B16/BMX bitmap format being developed for it

## Building
This program requires CC65 in your PATH to build

Steps:
1. Clone the repo. For example: ``git clone https://github.com/catmeow72/b16view.git b16view``
2. cd into the repo. For example: ``cd b16view``
3. Run ``make``

## Usage
To run the program in the official Commander X16 emulator run ``make test``

To run the program in Box16, run ``make test emu_prog=box16``

The program will ask you what file to use, and any compatible B16 file in the directory of the Makefile should work. Just wait for it to load the file and upload it to the emulated VERA, and it should display the bitmap image.