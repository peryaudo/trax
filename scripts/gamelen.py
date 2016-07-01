import subprocess
import pandas as pd
import matplotlib.pyplot as plt
from StringIO import StringIO


p = subprocess.Popen(
    "./trax --use_log --stats_csv --interpolate --searcher=negamax1-la",
    # "./trax --use_log --stats_csv",
    cwd="..", shell=True,
    stdout=subprocess.PIPE)
stdout_data, stderr_data = p.communicate()

df = pd.read_csv(StringIO(stdout_data))

plt.hist(df[df.winning_reason == 'LINE']['total_step'], label="line", color="red", alpha=0.5, bins=25, range=(0, 50))
plt.hist(df[df.winning_reason == 'LOOP']['total_step'], label="loop", color="blue", alpha=0.5, bins=25, range=(0, 50))
plt.legend()
plt.show()
