import subprocess
import sys
import os
import time

def start_server():
    """Starts the demo server on port 2026."""
    server_path = os.path.join(os.getcwd(), "apps", "server", "main.py")
    
    server_dir = os.path.dirname(server_path)
    log_dir = os.path.join(server_dir, "logs")
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, "server.log")

    print(f"Starting server from {server_path}...")
    
    # Using sys.executable to ensure we use the same python interpreter (within venv if active)
    # Running in background
    try:
        with open(log_path, "w") as log_file:
            process = subprocess.Popen(
                [sys.executable, server_path],
                stdout=log_file,
                stderr=log_file,
                cwd=os.getcwd()
            )
        print(f"Server started with PID: {process.pid}")
        print(f"Logs are being written to {log_path}")
        
        # Give it a moment to start
        time.sleep(2)
        
        # Check if it's still running
        if process.poll() is None:
             print("Server appears to be running successfully.")
        else:
             print(f"Server failed to start. Return code: {process.returncode}")
             print("Check server.log for details.")
             sys.exit(1)

    except Exception as e:
        print(f"Failed to start server: {e}")
        sys.exit(1)

if __name__ == "__main__":
    start_server()
