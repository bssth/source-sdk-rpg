# Builds the static menu background (console/bb_bg*) from a bb_bg screenshot.
# GameUI shows it while the bb_bg background map loads, then fades to the map.
# Usage (from the mod folder):
#   python materialsrc/console/make_bg.py materialsrc/console/bb_bg.png materials/console bb_bg_preview.png
import struct, sys, os
import numpy as np
from PIL import Image, ImageFilter

src, matdir, preview = sys.argv[1], sys.argv[2], sys.argv[3]
im = np.asarray(Image.open(src).convert('RGB')).astype(np.float32)
H, W = im.shape[:2]
assert (W, H) == (1920, 1080), (W, H)

# Paint over the HUD (health bar + flashlight panel, bottom left) with the
# ground just above it, mirrored, feathered into the surroundings.
y0, x1 = 1008, 420
patch = im[2*y0-H:y0][::-1, :x1].copy()
mask = np.ones((H - y0, x1), np.float32)
fx = np.clip((x1 - np.arange(x1)) / 40.0, 0, 1)
fy = np.clip((np.arange(H - y0)) / 12.0, 0, 1)
mask *= fx[None, :] * fy[:, None]
im[y0:, :x1] = im[y0:, :x1] * (1 - mask[..., None]) + patch * mask[..., None]

img = Image.fromarray(np.clip(im, 0, 255).astype(np.uint8))
blurred = img.filter(ImageFilter.GaussianBlur(15))
blurred.save(preview)

def write_vtf(path, pil):
    w, h = pil.size
    rgb = np.asarray(pil.convert('RGB'))
    bgr = rgb[..., ::-1].tobytes()
    lin = (rgb.astype(np.float64) / 255.0) ** 2.2
    refl = lin.reshape(-1, 3).mean(0)
    # 16x16 DXT1 low-res thumbnail, solid colour per 4x4 block
    th = np.asarray(pil.convert('RGB').resize((16, 16), Image.BOX)).astype(np.int32)
    low = bytearray()
    for by in range(0, 16, 4):
        for bx in range(0, 16, 4):
            c = th[by:by+4, bx:bx+4].reshape(-1, 3).mean(0).astype(int)
            c565 = ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3)
            low += struct.pack('<HHI', c565, c565, 0)
    flags = 0x4 | 0x8 | 0x100 | 0x200   # CLAMPS | CLAMPT | NOMIP | NOLOD
    hdr = struct.pack('<4sIIIHHIHH4x3f4xfiBiBBH', b'VTF\0', 7, 2, 80, w, h, flags,
                      1, 0, *refl, 1.0, 3, 1, 13, 16, 16, 1)   # BGR888, lowres DXT1
    hdr = hdr.ljust(80, b'\0')
    with open(path, 'wb') as f:
        f.write(hdr + bytes(low) + bgr)
    print('wrote', path, w, h, os.path.getsize(path))

os.makedirs(matdir, exist_ok=True)
# Widescreen: whole 16:9 frame; GameUI stretches it over the screen.
write_vtf(os.path.join(matdir, 'bb_bg_widescreen.vtf'), blurred.resize((1024, 512), Image.LANCZOS))
# 4:3: centre crop.
cw = H * 4 // 3
write_vtf(os.path.join(matdir, 'bb_bg.vtf'), blurred.crop(((W - cw)//2, 0, (W + cw)//2, H)).resize((512, 512), Image.LANCZOS))

VMT = '''"UnlitGeneric"
{
	"$basetexture" "console/%s"
	"$vertexcolor" 1
	"$vertexalpha" 1
	"$ignorez" 1
	"$no_fullbright" "1"
	"$nolod" "1"
}
'''
# bb_bg is what ChapterBackgrounds picks; background01 is what GameUI shows
# before the background map is chosen, so it gets the same image.
for name, tex in [('bb_bg', 'bb_bg'), ('bb_bg_widescreen', 'bb_bg_widescreen'),
                  ('background01', 'bb_bg'), ('background01_widescreen', 'bb_bg_widescreen')]:
    with open(os.path.join(matdir, name + '.vmt'), 'w', newline='\r\n') as f:
        f.write(VMT % tex)
