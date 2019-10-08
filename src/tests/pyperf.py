import cProfile

def process_one():
  x = 0
  for i in range(18000000):
    x = x + 1

cProfile.run('process_one()')
cProfile.run('process_one()')
cProfile.run('process_one()')
cProfile.run('process_one()')
