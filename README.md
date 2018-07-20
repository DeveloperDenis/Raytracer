# Sample Output Images

![Sample Image 1](/output/default_scene.png)
![Sample Image 2](/output/easy3.png)
![Sample Image 4](/output/interpolation.png)
![Sample Image 5](/output/spheres.png)

# Building

If you navigate to the 'src' directory, you will see a "build.bat" file and a "build.sh" file. Running build.bat will build
the Windows executable, and running build.sh will build the Linux executable.

# Running

My submission comes with executables built for both Linux (Raytracer) and Windows (Raytracer.exe). The builds have been
tested on Linux Mint and Windows 10. The Windows build should work with versions as far back as Windows 7, and the Linux
build should work on any Linux distribution that uses XWindows as the windowing system (most distros).

The executables must be run from the data directory, either by copying the executable into the directory and running, or
by navigating to the data directory in the terminal and typing something like "..\build\Raytracer.exe"
If you are on Windows, you can open the Visual Studio solution file and simply run from there since it has already been
set to run from the data directory, and you can right-click on "Raytracer" under "Solution Raytracer" in the solution
explorer and click properties to change the command line arguments so that you can run with different ray files.

Either of the builds can take a window size and a ray file as command line arguments. If no arguments are provided,
a default window size of 640 x 480 is used and "default_scene.ray" is loaded. Window size is in the format "widthxheight",
an example would be "1280x720". The ray file argument must contain the file name including the .ray extension.


# Application Features

When the application is started, there will be a rectangle near the top of the screen. This is the button to start the
raytracer using the ray scene file selected through the command line arguments. The progress bar at the bottom of the
window will fill up as the raytracer progressively calculates the values for each pixel.
The window can be resized at any time and the raytracer will use the new window width and height the next time it runs.
You toggle the UI's visibility by right-clicking in the window.

There is support for plane, box, sphere, and triangle (and by extension, triangulated mesh) intersections. Triangles have normal and material interpolation across the triangle surface.

The application reads ".ray" files to construct the scene. I wrote my own basic parser of the ray files, which makes a bunch of assumptions and is not very robust, but it does work for all ray files included with the project.
The camera takes its values from the ray file and supports arbitrary view directions, up vectors, and positions.

There can be any number of lights in a scene. There are three different types of lights: ambient, directional, and point.
The Phong local illumination model is used to calculate the shading of the objects. Shadow casting is also supported.
