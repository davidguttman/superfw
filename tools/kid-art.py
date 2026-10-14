#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["Pillow>=10,<13"]
# ///
"""Prepare the deliberately narrow SuperFW kid-mode BMP dialect (GPL-3.0+)."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile

from PIL import Image, ImageOps

WIDTH, HEIGHT, COLORS = 160, 104, 220


def convert(source: Path, destination: Path) -> None:
    with Image.open(source) as original:
        image = ImageOps.exif_transpose(original).convert("RGBA")
        background = Image.new("RGBA", image.size, "black")
        background.alpha_composite(image)
        image = ImageOps.contain(background.convert("RGB"), (WIDTH, HEIGHT), Image.Resampling.LANCZOS)
        canvas = Image.new("RGB", (WIDTH, HEIGHT), "black")
        canvas.paste(image, ((WIDTH - image.width) // 2, (HEIGHT - image.height) // 2))
        indexed = canvas.quantize(colors=COLORS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        # Write the BMP ourselves: Pillow normally emits 256 palette entries,
        # which would overlap the firmware's reserved UI palette indices.
        palette = indexed.getpalette()
        count = max(indexed.tobytes()) + 1
        offset = 54 + count * 4
        size = offset + WIDTH * HEIGHT
        header = struct.pack("<2sIHHI", b"BM", size, 0, 0, offset)
        header += struct.pack("<IiiHHIIiiII", 40, WIDTH, HEIGHT, 1, 8, 0, WIDTH * HEIGHT, 0, 0, count, 0)
        colors = b"".join(bytes((palette[i * 3 + 2], palette[i * 3 + 1], palette[i * 3], 0)) for i in range(count))
        data = indexed.tobytes()
        rows = b"".join(data[y * WIDTH:(y + 1) * WIDTH] for y in range(HEIGHT - 1, -1, -1))
        destination.write_bytes(header + colors + rows)


def self_test(reader: str) -> None:
    with tempfile.TemporaryDirectory(prefix="superfw-kid-art-") as directory:
        source = Path(directory) / "original.png"
        output = Path(directory) / "converted.bmp"
        Image.new("RGB", (320, 208), "red").save(source)
        convert(source, output)
        subprocess.run([str(Path(reader).resolve()), str(output)], check=True)
        # Ordinary JPEG and RGBA inputs exercise the same writer.
        for mode, suffix, color in (("RGB", "jpg", "red"), ("RGBA", "png", (255, 0, 0, 255))):
            source = Path(directory) / f"original.{suffix}"
            Image.new(mode, (160, 104), color).save(source)
            convert(source, output)
            subprocess.run([str(Path(reader).resolve()), str(output)], check=True)
        for image in (Image.effect_noise((300, 300), 90).convert("RGB"),
                      Image.new("RGBA", (200, 50), (255, 0, 0, 0))):
            image.save(source.with_suffix(".png"))
            convert(source.with_suffix(".png"), output)
            subprocess.run([str(Path(reader).resolve()), str(output), "--any"], check=True)
    print("PNG/JPEG/RGBA, quantization, letterbox and transparency round trips passed")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="?", type=Path)
    parser.add_argument("destination", nargs="?", type=Path)
    parser.add_argument("--self-test", metavar="C_READER")
    args = parser.parse_args()
    if args.self_test:
        self_test(args.self_test)
    elif args.source and args.destination:
        if args.source.resolve() == args.destination.resolve():
            parser.error("source and destination must differ")
        convert(args.source, args.destination)
        print(f"Wrote {args.destination}: {WIDTH}x{HEIGHT}, at most {COLORS} colors")
    else:
        parser.error("provide source and destination, or --self-test C_READER")


if __name__ == "__main__":
    main()
