import numpy as np
import matplotlib.pyplot as plt

# 读取文件中的整数
with open('input.txt', 'r') as f:
    data = f.read().splitlines()
data = list(map(int, data))
#print(data)

# 计算CDF
counts, bin_edges = np.histogram(data, bins=100)
#print(counts)
#print(bin_edges)
cdf = np.cumsum(counts)

# 绘制CDF图
plt.plot(bin_edges[1:], cdf/cdf[-1])
plt.xlabel('Value')
plt.ylabel('CDF')
plt.title('CDF of Integers in input.txt')
plt.savefig('output.png')



