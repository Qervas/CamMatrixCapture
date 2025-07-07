#!/usr/bin/env python3
"""
Sapera RAW File Reader for Neural Rendering Dataset
Reads headerless binary RAW files from Nano-C4020 cameras and displays them
"""

import numpy as np
import cv2
import matplotlib.pyplot as plt
import os
import argparse
from pathlib import Path

class SaperaRawReader:
    def __init__(self, width=4112, height=3008, bit_depth=12):
        """
        Initialize RAW reader for Nano-C4020 cameras
        
        Args:
            width: Image width in pixels (4112 for Nano-C4020)
            height: Image height in pixels (3008 for Nano-C4020)
            bit_depth: Bits per pixel (typically 12 or 16)
        """
        self.width = width
        self.height = height
        self.bit_depth = bit_depth
        self.bytes_per_pixel = 2  # Usually 16-bit storage even for 12-bit data
        
    def read_raw_file(self, filepath):
        """
        Read Sapera RAW file and return as numpy array
        
        Args:
            filepath: Path to .raw file
            
        Returns:
            numpy array with raw Bayer data
        """
        try:
            # Read binary data
            with open(filepath, 'rb') as f:
                raw_data = f.read()
            
            actual_size = len(raw_data)
            print(f"File size: {actual_size} bytes")
            
            # Try different byte formats
            formats_to_try = [
                (2, np.uint16, "16-bit"),  # 2 bytes per pixel
                (1, np.uint8, "8-bit"),    # 1 byte per pixel
            ]
            
            for bytes_per_pixel, dtype, description in formats_to_try:
                expected_size = self.width * self.height * bytes_per_pixel
                print(f"Expected size for {description}: {expected_size} bytes")
                
                if actual_size == expected_size:
                    print(f"✅ Detected format: {description} ({self.width}x{self.height})")
                    self.bytes_per_pixel = bytes_per_pixel
                    
                    # Convert bytes to array
                    raw_array = np.frombuffer(raw_data, dtype=dtype)
                    image = raw_array.reshape((self.height, self.width))
                    
                    # Convert to uint16 for consistent processing
                    if dtype == np.uint8:
                        image = (image.astype(np.uint16)) << 8  # Scale 8-bit to 16-bit
                    elif self.bit_depth == 12 and dtype == np.uint16:
                        image = image << 4  # Scale 12-bit to 16-bit
                    
                    return image
            
            # If no exact match, try auto-detection with different resolutions
            print(f"⚠️  No exact match! Trying auto-detection...")
            
            # Try common resolutions with different bit depths
            common_resolutions = [
                (4112, 3008),  # Nano-C4020 full
                (2056, 1504),  # Half resolution
                (1028, 752),   # Quarter resolution
            ]
            
            for bytes_per_pixel, dtype, description in formats_to_try:
                total_pixels = actual_size // bytes_per_pixel
                for w, h in common_resolutions:
                    if w * h == total_pixels:
                        print(f"✅ Auto-detected: {w}x{h} {description}")
                        self.width, self.height = w, h
                        self.bytes_per_pixel = bytes_per_pixel
                        
                        raw_array = np.frombuffer(raw_data, dtype=dtype)
                        image = raw_array.reshape((self.height, self.width))
                        
                        # Convert to uint16
                        if dtype == np.uint8:
                            image = (image.astype(np.uint16)) << 8
                        elif self.bit_depth == 12 and dtype == np.uint16:
                            image = image << 4
                        
                        return image
            
            print("❌ Could not detect format automatically")
            return None
            
        except Exception as e:
            print(f"❌ Error reading RAW file: {e}")
            return None
    
    def demosaic_bayer(self, bayer_image, pattern='RGGB'):
        """
        Demosaic Bayer pattern to RGB
        
        Args:
            bayer_image: Raw Bayer data as numpy array
            pattern: Bayer pattern ('RGGB', 'BGGR', 'GRBG', 'GBRG')
            
        Returns:
            RGB image as numpy array
        """
        # Convert to 8-bit for OpenCV processing
        bayer_8bit = (bayer_image >> 8).astype(np.uint8)
        
        # Map pattern to OpenCV constants
        pattern_map = {
            'RGGB': cv2.COLOR_BAYER_BG2RGB,
            'BGGR': cv2.COLOR_BAYER_RG2RGB, 
            'GRBG': cv2.COLOR_BAYER_GB2RGB,
            'GBRG': cv2.COLOR_BAYER_GR2RGB
        }
        
        if pattern not in pattern_map:
            print(f"❌ Unknown Bayer pattern: {pattern}")
            return None
        
        # Demosaic using OpenCV
        rgb_image = cv2.cvtColor(bayer_8bit, pattern_map[pattern])
        
        return rgb_image
    
    def display_image(self, image, title="Sapera RAW Image"):
        """
        Display image using matplotlib
        
        Args:
            image: Image array to display
            title: Window title
        """
        plt.figure(figsize=(12, 8))
        
        if len(image.shape) == 3:
            # RGB image
            plt.imshow(image)
        else:
            # Grayscale RAW
            plt.imshow(image, cmap='gray')
        
        plt.title(title)
        plt.axis('off')
        plt.show()
    
    def process_raw_file(self, filepath, bayer_pattern='RGGB', display=True):
        """
        Complete pipeline: read, demosaic, and display RAW file
        
        Args:
            filepath: Path to .raw file
            bayer_pattern: Bayer pattern for demosaicing
            display: Whether to display the image
            
        Returns:
            tuple: (raw_image, rgb_image)
        """
        print(f"🔍 Processing: {filepath}")
        
        # Read RAW data
        raw_image = self.read_raw_file(filepath)
        if raw_image is None:
            return None, None
        
        print(f"✅ RAW image loaded: {raw_image.shape}")
        print(f"   Data range: {raw_image.min()} - {raw_image.max()}")
        
        # Demosaic to RGB
        rgb_image = self.demosaic_bayer(raw_image, bayer_pattern)
        if rgb_image is None:
            return raw_image, None
        
        print(f"✅ RGB image created: {rgb_image.shape}")
        
        if display:
            # Display both RAW and RGB
            fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
            
            # RAW image
            ax1.imshow(raw_image, cmap='gray')
            ax1.set_title(f'RAW Bayer Data\n{raw_image.shape}')
            ax1.axis('off')
            
            # RGB image  
            ax2.imshow(rgb_image)
            ax2.set_title(f'Demosaiced RGB\n{rgb_image.shape}')
            ax2.axis('off')
            
            plt.suptitle(f'Sapera RAW: {Path(filepath).name}')
            plt.tight_layout()
            plt.show()
        
        return raw_image, rgb_image

def main():
    parser = argparse.ArgumentParser(description='Read and display Sapera SDK RAW files')
    parser.add_argument('filepath', help='Path to .raw file')
    parser.add_argument('--width', type=int, default=4112, help='Image width (default: 4112)')
    parser.add_argument('--height', type=int, default=3008, help='Image height (default: 3008)') 
    parser.add_argument('--bits', type=int, default=12, help='Bit depth (default: 12)')
    parser.add_argument('--pattern', default='RGGB', help='Bayer pattern (default: RGGB)')
    parser.add_argument('--no-display', action='store_true', help='Don\'t display images')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.filepath):
        print(f"❌ File not found: {args.filepath}")
        return
    
    # Create reader
    reader = SaperaRawReader(args.width, args.height, args.bits)
    
    # Process file
    raw_img, rgb_img = reader.process_raw_file(
        args.filepath, 
        args.pattern, 
        display=not args.no_display
    )
    
    if raw_img is not None:
        print("✅ RAW file processed successfully!")
        
        # Save RGB version
        if rgb_img is not None:
            output_path = args.filepath.replace('.raw', '_rgb.png')
            cv2.imwrite(output_path, cv2.cvtColor(rgb_img, cv2.COLOR_RGB2BGR))
            print(f"💾 RGB image saved: {output_path}")

if __name__ == "__main__":
    # Example usage if run directly
    import sys
    
    if len(sys.argv) == 1:
        print("🔍 Looking for RAW files in neural_dataset...")
        
        # Find recent RAW files
        dataset_path = Path("neural_dataset/images")
        if dataset_path.exists():
            raw_files = list(dataset_path.glob("**/*.raw"))
            
            if raw_files:
                print(f"Found {len(raw_files)} RAW files")
                latest_file = max(raw_files, key=lambda x: x.stat().st_mtime)
                print(f"📂 Processing latest: {latest_file}")
                
                reader = SaperaRawReader()
                reader.process_raw_file(str(latest_file))
            else:
                print("❌ No RAW files found")
        else:
            print("❌ neural_dataset directory not found")
            print("\nUsage:")
            print("  python read_sapera_raw.py <file.raw>")
            print("  python read_sapera_raw.py cam_01_capture_001.raw --pattern RGGB")
    else:
        main() 