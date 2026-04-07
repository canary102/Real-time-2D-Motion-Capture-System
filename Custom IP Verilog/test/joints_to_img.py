from PIL import Image, ImageDraw

def generate_skeleton_frame(joint_locations, output_hex, x_size=1280, y_size=720):
    # Create a pure black blackground, mode = "RGB"
    img = Image.new('RGB', (x_size, y_size), color=(0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Draw 5x5 white boxes at each joint location
    for x, y in joint_locations:
        # Ensure coordinates are within image boundaries
        if 0 <= x < x_size and 0 <= y < y_size:
            x1 = max(0, x - 2)
            y1 = max(0, y - 2)
            x2 = min(x_size - 1, x + 2)
            y2 = min(y_size - 1, y + 2)

            # Fill with white
            draw.rectangle([x1, y1, x2, y2], fill=(255, 255, 255))
        else:
            print(f"Warning: Joint location is out of frame bounds.")

    # Save the generated image for debug
    img.save("skeleton_frame_debug.png")

if __name__ == "__main__":
    # Change based on SystemVerilog Testbench waveforms
    joints = [
        (677, 202),  
        (702, 608),  
        (601, 606),   
        (718, 479),   
        (620, 465),   
        (680, 352),
        (805, 324),
        (542, 327),
        (754, 210),
        (691, 116),
        (791, 260),
        (565, 260),
        (579, 207)
    ]