"""
validation script for hr candidate extraction
verifies if the primary spectral peak (bin x) from the python reference 
is present among the C++ candidates within a +/- 1 bin tolerance

args:
    reference (str): path to the python reference candidates csv
    test (str): path to the c++ candidates csv
"""

import argparse
import sys
import pandas as pd


MAX_BIN_DIFF = 1 # max allowed shift in frequency bins


def main(reference_path, test_path):
    reference = pd.read_csv(reference_path, sep=";")
    test = pd.read_csv(test_path, sep=";")

    reference = reference[reference["power"] > 0]
    test = test[test["power"] > 0]

    if len(reference) == 0:
        print("FAIL: no reference candidate")
        return 1

    if len(test) == 0:
        print("FAIL: no C++ candidates")
        return 1

    reference_bin = int(reference.iloc[0]["index"])
    reference_freq = int(reference.iloc[0]["frequency_q14"])

    closest = test.iloc[(test["index"] - reference_bin).abs().argmin()]

    test_bin = int(closest["index"])
    test_freq = int(closest["frequency_q14"])

    bin_diff = abs(reference_bin - test_bin)

    print(f"Reference bin:  {reference_bin}")
    print(f"C++ bin:        {test_bin}")
    print(f"Reference freq: {reference_freq}")
    print(f"C++ freq:       {test_freq}")
    print(f"Bin diff:       {bin_diff}")

    if bin_diff <= MAX_BIN_DIFF:
        print("RESULT: PASS")
        return 0

    print("RESULT: FAIL")
    return 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("reference")
    parser.add_argument("test")
    args = parser.parse_args()

    sys.exit(main(args.reference, args.test))