# GTK Glyph Viewer

A basic glyph rendering program using the Freetype library intended as a base for testing various modifications. This similar in functionality to the `ftgrid` demo program for Freetype but uses a GTK based UI.

Some useful features are:
* Uses a file open dialog to open font files so no more typing long paths on the command line.
* When opening a file with multiple faces (e.g. a TrueType Collection) it prompts the user with a list of the faces in the file to select.
* Better linear blending. The `ftgrid` demo (and the other Freetype demos) use a smaller lookup table resulting in bands of shade and gives less than 256 shades of grey. The output is now closer to what a graphics library would draw.
* There's an option to draw the subpixel elements as a trio of greyscale segments inside the scaled pixel instead of a RGB colored pixel. This lets the user see the effects of the LCD filtering and the shape of the rasterized output down to a subpixel level.
* Can click and drag to move the drawn glyph about.

Some missing functionality from `ftgrid` that can perhaps be added in future: no emboldening, no status text, no custom LCD filters, only greyscale and horizontal subpixel antialiasing supported, no bitmap strikes displayed (the program is supposed to show outline rasterization, not embedded bitmaps e.g. MS Gothic), no custom pixel density (pixels per inch - it's stuck at 96 right now).

This program and its source code are licensed under the terms of the GNU General Public License V2. No warrenty is provided. See the `COPYING` file for more details.

### Building

This was developed on MSYS2 and I haven't used/tested it outside of that environment. You need to have GTK2 installed and it's dependencies as well as the standard development tools. The build system used is `cmake` using a `makefile` generator.

1. Create a build directory, I use `[repo root]/build`.

>`$ mkdir build`

>`$ cd build`

2. Generate the `makefile` with the path to the repo root containing `CMakelists.txt`. Then run `make`.

>`$ cmake -G MSYS Makefiles" ..`

>`$ make`

You can now run the built program:

>`$ ./gtkgylphviewer`