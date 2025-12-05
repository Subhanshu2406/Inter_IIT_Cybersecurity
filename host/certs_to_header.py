#!/usr/bin/env python3
"""
Convert certificate/key files to C header arrays for embedding in firmware
"""
import sys
import os

def file_to_c_array(filename, var_name):
    """Convert a binary file to a C array declaration"""
    with open(filename, 'rb') as f:
        data = f.read()
    
    # Generate C array
    c_array = f"static const unsigned char {var_name}[] = {{\n"
    for i in range(0, len(data), 12):
        chunk = data[i:i+12]
        hex_values = ', '.join(f'0x{b:02x}' for b in chunk)
        c_array += f"    {hex_values},\n"
    c_array = c_array.rstrip(',\n') + '\n'
    c_array += "};\n"
    c_array += f"static const unsigned int {var_name}_len = {len(data)};\n\n"
    
    return c_array

def main():
    cert_dir = "host/certs"
    output_file = "boot/wolfssl/certs_data.h"
    
    print(f"Converting certificates to C arrays...")
    
    # Files to convert
    files = [
        ("ca-cert.der", "ca_cert_der"),
        ("client-cert.der", "client_cert_der"),
        ("client-key.der", "client_key_der"),
    ]
    
    header_content = """/* Auto-generated certificate data for embedded DTLS client */
#ifndef CERTS_DATA_H
#define CERTS_DATA_H

"""
    
    for filename, var_name in files:
        filepath = os.path.join(cert_dir, filename)
        if not os.path.exists(filepath):
            print(f"Error: {filepath} not found!")
            sys.exit(1)
        
        print(f"  {filename} -> {var_name}")
        header_content += file_to_c_array(filepath, var_name)
    
    header_content += "#endif /* CERTS_DATA_H */\n"
    
    # Write output
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    with open(output_file, 'w') as f:
        f.write(header_content)
    
    print(f"\nGenerated: {output_file}")
    print("Certificate data ready for firmware embedding!")

if __name__ == "__main__":
    main()
