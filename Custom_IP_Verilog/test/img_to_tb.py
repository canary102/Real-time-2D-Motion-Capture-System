from PIL import Image

def convert_to_hex(image_path, output_hex):
    # Load image --> ensure RGB
    img = Image.open(image_path).convert('RGB')
    
    # resize the image to the correct resolution (1280 x 720)
    if img.size != (1280, 720):
        img = img.resize((1280, 720))

    pixels = img.load()
    
    with open(output_hex, 'w') as f:
        for y in range(720):
            for x in range(1280):
                r, g, b = pixels[x, y]
                # Combine into a 24-bit single hex value
                hex_val = f"{r:02x}{g:02x}{b:02x}"
                f.write(hex_val + "\n")

if __name__ == "__main__":
    convert_to_hex("input.png", "image_data.hex")