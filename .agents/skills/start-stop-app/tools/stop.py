import subprocess
import sys
import os

def stop_server():
    """Stops the demo server running on port 2026."""
    print("Attempting to stop server on port 2026...")
    
    try:
        # Find process using lsof on port 2026
        # -t gives terse output (just PID)
        result = subprocess.run(
            ["lsof", "-t", "-i", ":2026"], 
            capture_output=True, 
            text=True
        )
        
        if result.returncode != 0 and result.returncode != 1:
             # lsof returns 1 if no process found, which is fine
             print(f"Error checking for process: {result.stderr}")
             sys.exit(1)
             
        pids = result.stdout.strip().split('\n')
        pids = [p for p in pids if p] # Filter empty strings
        
        if not pids:
            print("No server found running on port 2026.")
            return

        for pid in pids:
            print(f"Killing process {pid}...")
            subprocess.run(["kill", "-9", pid], check=True)
            print(f"Process {pid} killed.")
            
    except FileNotFoundError:
        print("Error: 'lsof' command not found. Please ensure it is installed.")
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"Error killing process: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        sys.exit(1)

if __name__ == "__main__":
    stop_server()
