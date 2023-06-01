import string
import random

n = 100000
length = 64
random_strings = set()
random_integers = set()
while len(random_strings) < n:
    s = ''.join(random.choices(string.ascii_letters + string.digits, k=length))
    random_strings.add(s)
    i = int.from_bytes(s.encode(), byteorder='big')
    random_integers.add(i)
    #print(s+'\n')
    #print(str(i)+'\n')

#random_strings = [''.join(random.choices(string.ascii_letters + string.digits, k=8)) for i in range(n)]


with open('input.txt', 'w') as f:
    for integer in random_integers:
        f.write(str(integer) + '\n')

with open('output_strings.txt','w')as f:
    for string in random_strings:
        f.write(string+'\n')
