import pandas as pd
import numpy as np
import matplotlib.pyplot as plt


data = pd.read_csv('factors.csv')

plt.hist(data[data.winner > 0]['endpoint_factor'], label=">0", color="red", alpha=0.5)
plt.hist(data[data.winner < 0]['endpoint_factor'], label="<0", color="blue", alpha=0.5)
plt.show()

plt.hist(data[data.winner > 0]['edge_factor'], label=">0", color="red", alpha=0.5)
plt.hist(data[data.winner < 0]['edge_factor'], label="<0", color="blue", alpha=0.5)
plt.show()

plt.scatter(data['endpoint_factor'], data['edge_factor'], s=20, c=data['winner'], cmap='Blues')
plt.show()
