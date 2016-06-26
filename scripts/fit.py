import pandas as pd
import numpy as np
from sklearn import (svm, linear_model)
from sklearn.metrics import mean_squared_error
from sklearn.cross_validation import train_test_split
import matplotlib.pyplot as plt

fns = ['factors.csv', 'factors_max_edge.csv', 'factors_max_min.csv', 'factors_max_min_edge.csv', 'factors_split_average.csv', 'factors_split_average_max_min_edge.csv', 'gengame_factors_split_average.csv', 'extended_factors_split_average.csv']

for filename in fns:
    print "%s:" % filename
    data = pd.read_csv(filename)
    X = data[['endpoint_factor', 'edge_factor']]
    # X = data[['edge_factor']]
    # X = data[['endpoint_factor']]
    y = data['winner']

    match = 0
    nomatch = 0
    orig = []
    pred = []
    for _ in xrange(0, 30):
        X_train, X_test, y_train, y_test = train_test_split(X, y)

        model = linear_model.LinearRegression()
        # model = svm.SVR(kernel='rbf')
        model.fit(X_train, y_train)

        # print model.coef_

        origg = list(y_test)
        predd = list(model.predict(X_test))

        for (o, p) in zip(origg, predd):
            if o * p > 0:
                match += 1
            else:
                nomatch += 1
        orig.extend(origg)
        pred.extend(predd)

    print "mse: %f" % mean_squared_error(orig, pred)
    print "sign accuracy: %f (%d/%d)" % (float(match) / (match + nomatch), match, match + nomatch)
    print ""
