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

WIDTH, HEIGHT, COLORS = 160, 160, 220


def convert(source: Path, destination: Path) -> None:
    with Image.open(source) as original:
        image = ImageOps.exif_transpose(original).convert("RGBA")
        background = Image.new("RGBA", image.size, "black")
        background.alpha_composite(image)
        # Cover the square completely; center-crop rather than add padding.
        image = ImageOps.fit(background.convert("RGB"), (WIDTH, HEIGHT), Image.Resampling.LANCZOS)
        indexed = image.quantize(colors=COLORS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
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
        with Image.open(output) as result:
            assert result.size == (160, 160)
            assert result.convert("RGB").getextrema() == ((255, 255), (0, 0), (0, 0))
        # Cover-crop must discard wide/tall colored borders, not add bars.
        for size in ((480, 160), (160, 480)):
            image = Image.new("RGB", size, "blue")
            left, top = (size[0]-160)//2, (size[1]-160)//2
            image.paste("red", (left, top, left+160, top+160))
            image.save(source)
            convert(source, output)
            with Image.open(output) as result:
                assert result.size == (160, 160)
                assert result.convert("RGB").getextrema() == ((255, 255), (0, 0), (0, 0))
            subprocess.run([str(Path(reader).resolve()), str(output)], check=True)
        # EXIF orientation is applied before cropping (PNG avoids JPEG ringing).
        image = Image.new("RGB", (160, 160), "red")
        image.paste("blue", (0, 0, 80, 160))
        exif = Image.Exif(); exif[274] = 6
        image.save(source, exif=exif)
        convert(source, output)
        with Image.open(output) as result:
            rgb = result.convert("RGB")
            assert rgb.getpixel((80, 10)) == (0, 0, 255)
            assert rgb.getpixel((80, 150)) == (255, 0, 0)
        subprocess.run([str(Path(reader).resolve()), str(output), "--any"], check=True)
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
            if image.mode == "RGBA":
                with Image.open(output) as result:
                    assert result.convert("RGB").getextrema() == ((0, 0), (0, 0), (0, 0))
    print("PNG/JPEG/RGBA, cover crop, EXIF, quantization and transparency round trips passed")


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
