def fib4(n):
    n = n - 3
    a = 1
    b = 2
    
    for _ in range(n):
        c = a + b
        a = b
        b = c
    
    return b

# Test with profiling
import time

def run():
    start = time.time()
    result = fib4(90)
    end = time.time()
    
    print(f"result: {result}")
    print(f"time: {end - start:.6f}s")

run()
