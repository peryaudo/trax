import subprocess
import pandas as pd
import numpy as np
from sklearn import (svm, linear_model, decomposition)
from sklearn.metrics import mean_squared_error
from sklearn.cross_validation import train_test_split
from StringIO import StringIO
from sklearn.grid_search import GridSearchCV

def fieldtonp(s):
    return np.array([[int(c) for c in list(line)] for line in s.split('|')]).flatten()

def fieldtonplr(s):
    return np.fliplr(np.array([[int(c) for c in list(line)] for line in s.split('|')])).flatten()

def fieldtonpud(s):
    return np.flipud(np.array([[int(c) for c in list(line)] for line in s.split('|')])).flatten()

print('generating csv...')
p = subprocess.Popen("./trax --use_log --interpolate --searcher=simple-la --factors_csv", cwd="..", shell=True,
                     stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE)
stdout_data, stderr_data = p.communicate()

print('parsing csv...')
data = pd.read_csv(StringIO(stdout_data))
X = [fieldtonp(row) for row in list(data['field'])]
X += [fieldtonplr(row) for row in list(data['field'])]
X += [fieldtonpud(row) for row in list(data['field'])]

y = list(data['winner']) * 3

X = np.array(X)
y = np.array(y)

if True:
    print('grid searching...')
    C_range = np.logspace(-2, 10, 13)
    gamma_range = np.logspace(-9, 3, 13)
    param_grid = dict(gamma=gamma_range, C=C_range, kernel='rbf')

    grid = GridSearchCV(svm.SVR(), param_grid=param_grid, verbose=3)
    grid.fit(X, y)

    print("The best parameters are %s with a score of %0.2f"
          % (grid.best_params_, grid.best_score_))


    model = svm.SVR(kernel='rbf', C=grid.best_params_['C'], gamma=grid.best_params_['gamma'])
else:
    model = svm.SVR(kernel='rbf', verbose=3)

print('training...')

X_train, X_test, y_train, y_test = train_test_split(X, y)

model.fit(X_train, y_train)
y_pred = model.predict(X_test)

print "mse: %f" % mean_squared_error(y_test, y_pred)
print "corr: %f " % np.corrcoef(y_test, y_pred)[0][1]
