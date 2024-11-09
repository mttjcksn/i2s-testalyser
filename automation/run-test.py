from saleae import automation
import os
import argparse
import socket
import struct
from datetime import datetime
import threading

def main():
    # Set up argument parser
    parser = argparse.ArgumentParser(description="Device serial")
    parser.add_argument("--device", required=True, help="Device serial to use for the script")
    args = parser.parse_args()
    logic_device_id = args.device
    print(f"Using device ID: {logic_device_id}")

    # Create a stop event to signal the server thread to shut down
    stop_event = threading.Event()

    # Connect to Saleae Logic application
    with automation.Manager.connect(port=10430) as manager:
        device_configuration = automation.LogicDeviceConfiguration(
            enabled_digital_channels=[0, 1, 2, 3],
            digital_sample_rate=500_000_000,
            digital_threshold_volts=3.3,
        )

        capture_configuration = automation.CaptureConfiguration(
            buffer_size_megabytes=128,
            capture_mode=automation.ManualCaptureMode(trim_data_seconds=1.0)
        )

        with manager.start_capture(
                device_id=logic_device_id,
                device_configuration=device_configuration,
                capture_configuration=capture_configuration) as capture:
            
            # Start the server in a separate thread, passing the stop event
            server_thread = threading.Thread(target=start_server, args=('127.0.0.1', 65432, capture, stop_event))
            server_thread.start()

            # Add analyzer
            test_analyzer = capture.add_analyzer('I2S / PCM Test', label='Test Analyzer', settings={
                'CLOCK channel': 1,
                'FRAME': 2,
                'DATA': 3,
                'DATA Significant Bit': 'Most Significant Bit Sent First',
                'CLOCK State': 'Rising edge',
                'Audio Bit Depth (bits/sample)': '32 Bits/Word',
                'FRAME Signal Transitions': 'Once each word (I2S, PCM standard)',
                'DATA Bits Alignment': 'Left aligned',
                'DATA Bits Shift': 'Right-shifted by one (I2S typical)',
                'Signed/Unsigned': 'Unsigned',
                'Word Select High': 'Channel 2 (right - I2S typical)',
                'Test mode': 'Contiguous',
                'Use test server': True,
            })

            # Wait until the capture finishes or is stopped by the server
            capture.wait()

            # Set up output directory
            output_dir = os.path.join(os.getcwd(), f'output-{datetime.now().strftime("%Y-%m-%d_%H-%M-%S")}')
            os.makedirs(output_dir)

            # Export analyzer data
            # analyzer_export_filepath = os.path.join(output_dir, 'spi_export.csv')
            # capture.export_data_table(
            #     filepath=analyzer_export_filepath,
            #     analyzers=[test_analyzer]
            # )

            print("Exporting data... (This might take a few minutes)")

            # Export raw digital data
            capture.export_raw_data_csv(directory=output_dir, digital_channels=[0, 1, 2, 3])

            # Save the capture to a file
            capture_filepath = os.path.join(output_dir, 'i2s_test.sal')
            capture.save_capture(filepath=capture_filepath)

            print("Capture and export completed.")

    # Signal the server thread to stop and wait for it to finish
    stop_event.set()
    server_thread.join()  # Ensure the server thread exits before the script ends

    print("Script completed and exited gracefully.")

def start_server(host, port, capture, stop_event):
    # Create a TCP/IP socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind((host, port))
        server_socket.listen()
        server_socket.settimeout(1.0)  # Set a 1-second timeout for accept()

        print(f"Logging server listening on {host}:{port}")

        while not stop_event.is_set():
            try:
                conn, addr = server_socket.accept()
                with conn:
                    print(f"Connected by {addr}")
                    data = b''  # Initialize an empty bytes object to accumulate received data

                    while True:
                        chunk = conn.recv(16 - len(data))
                        if not chunk:
                            print(f"Connection closed by {addr}")
                            break
                        data += chunk

                        if len(data) == 16:
                            try:
                                value1, value2 = deserialize(data)
                                if value1 == 1111 and value2 == 9999:
                                    capture.stop()
                                    print("Capture stopped by external command.")
                                    break
                                elif value1 == 0 and value2 == 1234:
                                    print("Connected!")
                                else:
                                    ppmmin, ppmmax = calculate_ppm(value1*2, value2*2, 500000000, 3072000)
                                    print(f"Log received ppm: min: {ppmmin}, max: {ppmmax}")
                                    
                            except ValueError as e:
                                print(f"Deserialization error: {e}")
                            finally:
                                data = b''
            except socket.timeout:
                # Continue loop to check stop_event after timeout
                continue

def deserialize(data):
    if len(data) != 16:
        raise ValueError("Data must be exactly 16 bytes long.")
    return struct.unpack('<QQ', data)  # '<QQ' specifies little-endian, two uint64s

def calculate_ppm(min_samples, max_samples, sample_rate, clock_frequency):
    # Convert min and max intervals to time (in seconds)
    min_time = min_samples / sample_rate
    max_time = max_samples / sample_rate
    
    # Calculate the nominal interval based on the clock frequency
    nominal_interval = 1 / clock_frequency
    
    # Calculate the PPM deviation for min and max intervals
    min_ppm = ((min_time - nominal_interval) / nominal_interval) * 1_000_000
    max_ppm = ((max_time - nominal_interval) / nominal_interval) * 1_000_000
    
    return min_ppm, max_ppm

if __name__ == "__main__":
    main()
