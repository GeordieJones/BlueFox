from PIL import Image, ImageEnhance
import binascii

def hex_to_bytes(hex_str):
    # Remove all non-hex characters
    import re
    clean_hex = re.sub(r'[^0-9a-fA-F]', '', hex_str)
    return binascii.unhexlify(clean_hex)

def save_jpeg(data_bytes, filename):
    with open(filename, 'wb') as f:
        f.write(data_bytes)

def brighten_image(input_path, output_path, factor=1.5):
    try:
        img = Image.open(input_path)
        enhancer = ImageEnhance.Brightness(img)
        bright_img = enhancer.enhance(factor)
        bright_img.save(output_path)
        bright_img.show()
        return True
    except Exception as e:
        print(f"Error processing image: {e}")
        return False

if __name__ == "__main__":
    # Example hex dump (paste your actual output here)
    hex_dump = """
    // dump goes here
    """

    try:
        # Convert hex to bytes
        data_bytes = hex_to_bytes(hex_dump)
        
        # Save and process image
        raw_jpeg_path = "captured_image.jpg"
        bright_jpeg_path = "captured_image_bright.jpg"
        
        save_jpeg(data_bytes, raw_jpeg_path)
        
        if brighten_image(raw_jpeg_path, bright_jpeg_path, factor=1.5):
            print("Image processed successfully!")
        else:
            print("Image processing failed")
    except Exception as e:
        print(f"Error: {e}")

