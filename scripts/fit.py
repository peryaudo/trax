import subprocess
import pandas as pd
import numpy as np
from sklearn import (svm, linear_model)
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
    'max_edge_factor_average']

# factors = ['endpoint_factor','sum_edge_factor']

print "Correlation to all factors:"
for factor in factors:
    print "  %s: %f" % (factor,
                        np.corrcoef(data['winner'], data[factor])[0][1])
X = data[factors]

# X = data[['edge_factor']]
# X = data[['endpoint_factor']]
y = data['winner']

X_train, X_test, y_train, y_test = train_test_split(X, y)

model = linear_model.LinearRegression()
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
