# Converts the simulator's PPM frames to PNG. With --round the frame is shown the way the
# round glass shows it: everything outside the 480 px circle is cut away, on a dark bezel.
import sys
from PIL import Image, ImageDraw
src, dst = sys.argv[1], sys.argv[2]
im = Image.open(src).convert("RGB")
if "--round" in sys.argv:
    k = 4
    mask = Image.new("L", (im.width * k, im.height * k), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, im.width * k - 1, im.height * k - 1), fill=255)
    mask = mask.resize(im.size, Image.LANCZOS)
    pad = 26
    W = im.width + 2 * pad
    out = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    bez = Image.new("L", (W * k, W * k), 0)
    ImageDraw.Draw(bez).ellipse((0, 0, W * k - 1, W * k - 1), fill=255)
    bez = bez.resize((W, W), Image.LANCZOS)
    out.paste(Image.new("RGBA", (W, W), (24, 26, 30, 255)), (0, 0), bez)
    out.paste(im, (pad, pad), mask)
    out.save(dst)
else:
    im.save(dst)
