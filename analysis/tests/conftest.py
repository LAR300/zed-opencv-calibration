import os
import sys

# Tests import the package from analysis/ without installing it
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
