import os
import sys
import struct

def convert_png_to_ico(png_path, ico_path):
    print(f"Reading PNG from: {png_path}")
    if not os.path.exists(png_path):
        print(f"Error: PNG path does not exist: {png_path}")
        sys.exit(1)
        
    try:
        from PIL import Image
        print("PIL (Pillow) is available. Generating multi-resolution ICO...")
        img = Image.open(png_path)
        # Standard ICO sizes
        sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
        img.save(ico_path, format="ICO", sizes=sizes)
        print(f"Successfully saved multi-resolution ICO to {ico_path}")
        return
    except ImportError:
        print("PIL (Pillow) is not available. Using fallback raw PNG-to-ICO wrapper...")
        
    # Fallback: Write a standard single-image ICO wrapper for the PNG file
    # ICO Header:
    # 2 bytes: Reserved (must be 0)
    # 2 bytes: Image type (1 for icon)
    # 2 bytes: Number of images in file (1)
    # Directory Entry:
    # 1 byte: Width (0 means 256)
    # 1 byte: Height (0 means 256)
    # 1 byte: Color count (0 if >= 256 colors)
    # 1 byte: Reserved (0)
    # 2 bytes: Color planes (1)
    # 2 bytes: Bits per pixel (32)
    # 4 bytes: Size of image data in bytes
    # 4 bytes: Offset of image data from beginning of file (22)
    
    with open(png_path, "rb") as f:
        png_data = f.read()
        
    png_size = len(png_data)
    
    # We will assume 256x256 for the entry.
    # Note: Width/Height of 0 represents 256.
    width = 0
    height = 0
    
    ico_header = struct.pack("<HHH", 0, 1, 1)
    dir_entry = struct.pack("<BBBBHHII", 
                            width,     # bWidth
                            height,    # bHeight
                            0,         # bColorCount
                            0,         # bReserved
                            1,         # wPlanes
                            32,        # wBitCount
                            png_size,  # dwBytesInRes
                            22         # dwImageOffset (6 bytes header + 16 bytes directory entry = 22)
                           )
    
    with open(ico_path, "wb") as f:
        f.write(ico_header)
        f.write(dir_entry)
        f.write(png_data)
        
    print(f"Successfully saved wrapped ICO to {ico_path}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python convert_icon.py <input.png> <output.ico>")
        sys.exit(1)
    convert_png_to_ico(sys.argv[1], sys.argv[2])
