"""
Validation script for HR candidate extraction.

Compares HR candidates produced by the Python reference and the C++ implementation.

Because both implementations use different peak-detection methods, the complete
candidate sets are not expected to match exactly.

The test passes if the strongest candidate from either implementation has
a corresponding candidate in the other implementation within +/- 1 FFT bin.

Args:
    reference (str): Path to the Python reference candidates CSV
    test (str): Path to the C++ candidates CSV
"""

import argparse
import sys
import pandas as pd


MAX_BIN_DIFF = 1


def find_closest(candidate_bin, candidates):
    differences = (candidates["index"] - candidate_bin).abs()
    closest_idx = differences.idxmin()

    closest_bin = int(candidates.loc[closest_idx, "index"])
    bin_diff = abs(candidate_bin - closest_bin)

    return closest_bin, bin_diff


def main(reference_path, test_path):
    reference = pd.read_csv(reference_path, sep=";")
    test = pd.read_csv(test_path, sep=";")

    reference = reference[reference["power"] > 0]
    test = test[test["power"] > 0]

    if len(reference) == 0:
        print("FAIL: no Python reference candidates")
        return 1

    if len(test) == 0:
        print("FAIL: no C++ candidates")
        return 1

    # strongest candidate from each implementation
    reference_primary = reference.loc[reference["power"].idxmax()]
    test_primary = test.loc[test["power"].idxmax()]

    reference_bin = int(reference_primary["index"])
    test_bin = int(test_primary["index"])

    # check strongest Python candidate against all C++ candidates
    closest_cpp_bin, reference_diff = find_closest(reference_bin, test)

    # check strongest C++ candidate against all Python candidates
    closest_reference_bin, test_diff = find_closest(test_bin, reference)

    reference_matched = reference_diff <= MAX_BIN_DIFF
    test_matched = test_diff <= MAX_BIN_DIFF

    print()
    print("HR CANDIDATE COMPARISON")
    print()

    print(f"Python strongest bin: {reference_bin}")
    print(f"Closest C++ bin:      {closest_cpp_bin}")
    print(f"Bin difference:       {reference_diff}")
    print()

    print(f"C++ strongest bin:    {test_bin}")
    print(f"Closest Python bin:   {closest_reference_bin}")
    print(f"Bin difference:       {test_diff}")
    print()

    if reference_matched or test_matched:
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