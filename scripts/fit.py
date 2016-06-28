import subprocess
import pandas as pd
import numpy as np
from sklearn import (svm, linear_model, decomposition)
from sklearn.metrics import mean_squared_error
from sklearn.cross_validation import train_test_split
import matplotlib.pyplot as plt
from StringIO import StringIO

p = subprocess.Popen("./trax --dump_factors", cwd="..", shell=True,
                     stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE)
stdout_data, stderr_data = p.communicate()

data = pd.read_csv(StringIO(stdout_data))
factors = [
    'leaf_average','longest_line','edge_color',
    'endpoint_factor','sum_edge_factor','max_edge_factor',
    'endpoint_factor_max_min','sum_edge_factor_max_min',
    'max_edge_factor_max_min',
    'endpoint_factor_average','sum_edge_factor_average',
    'max_edge_factor_average',
    'shortcut',
    #'min_edge_size', 'max_edge_size',
]

# factors = ['endpoint_factor','sum_edge_factor']
# factors = ['sum_edge_factor_max_min','endpoint_factor_average']
factors = ['leaf_average', 'sum_edge_factor_max_min','endpoint_factor_average']

# plt.hist(data[data.winner > 0]['sum_edge_factor_max_min'], label=">0", color="red", alpha=0.5)
# plt.hist(data[data.winner < 0]['sum_edge_factor_max_min'], label="<0", color="blue", alpha=0.5)
# plt.show()
# plt.hist(data[data.winner > 0]['endpoint_factor_average'], label=">0", color="red", alpha=0.5)
# plt.hist(data[data.winner < 0]['endpoint_factor_average'], label="<0", color="blue", alpha=0.5)
# plt.show()

print "Correlation to all factors:"
for factor in factors:
    print "  %s: %f" % (factor,
                        np.corrcoef(data['winner'], data[factor])[0][1])

X = data[factors]
y = data['winner']

# pca = decomposition.PCA(n_components=2)
# pca.fit(X)
# X = pca.transform(X)

X_train, X_test, y_train, y_test = train_test_split(X, y)

model = linear_model.LinearRegression()
# model = linear_model.LogisticRegression()
# model = svm.SVR(kernel='rbf')
model.fit(X_train, y_train)

print model.coef_

orig = list(y_test)
pred = list(model.predict(X_test))

match = 0
nomatch = 0
for (o, p) in zip(orig, pred):
    if o * p > 0:
        match += 1
    else:
        nomatch += 1

print "mse: %f" % mean_squared_error(orig, pred)
print "sign accuracy: %f (%d/%d)" % (float(match) / (match + nomatch),
                                     match, match + nomatch)
print "corr: %f " % np.corrcoef(orig, pred)[0][1]
