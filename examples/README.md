# Example shaders

Each shader must have an image corresponding to it. If you just want to display the shader,
then just use an empty image that matches your display resolution (Inportant!).
Try running the tokyo shader example, the empty image is 2880x1920, if you have a
higher resolution display then please replace the empty image.

    wlsbg -i <REPO_PATH>/examples/empty.png -s <REPO_PATH>/examples/tokyo.glsl

You can also modify the background image using the shader.
Try running the pixelate shader example.

    wlsbg -i <REPO_PATH>/examples/kiki.jpg -s <REPO_PATH>/examples/pixelate.glsl

Shaders can also be controlled by the mouse, try running the
big balls shader example and click around with the mouse.

    wlsbg -i <REPO_PATH>/examples/empty.png -s <REPO_PATH>/examples/bigballs.glsl
