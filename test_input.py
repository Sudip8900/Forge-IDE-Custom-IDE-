import time
import sys

print("Python script started.")
sys.stdout.flush()

name = input("Enter your name: ")
print(f"Hello, {name}!")
sys.stdout.flush()

for i in range(3):
    print(f"Count: {i}")
    sys.stdout.flush()
    time.sleep(0.5)

print("Done!")
sys.stdout.flush()
