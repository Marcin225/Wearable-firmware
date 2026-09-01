# Parameter Tuning

Heart-rate algorithm parameters are tuned using recorded sensor data with HR measurements from the ECG chest strap as the reference.

The tuning tools repeatedly run the production C++ processing pipeline with different parameter values and export accuracy metrics for each tested configuration.

Two parameter sweeps are currently used:

- crest-factor threshold tuning,
- candidate-scoring weight tuning.

## TH_CF Tuning

`TH_CF_Q12` defines the minimum peak-to-mean spectral power ratio used by the HR state machine.

The sweep tests threshold values from:

| Parameter | Range | Step |
| :--- | :--- | :--- |
| `TH_CF` | 3.0 – 7.9 | 0.1 |

Each value is converted to Q12 before being passed to the production HR algorithm.

During this sweep, the candidate-scoring parameters remain fixed at their current configuration values:

```c
BONUS_Q12
MAIN_PENALTY_Q12
```

This isolates the effect of the CF threshold on HR stability, validity and accuracy.

## Scoring Weight Tuning

The second sweep tunes the two parameters used during HR candidate scoring:

```text
score =
    normalized PPG power
    - motion penalty
    + history bonus
```

The tested ranges are:

| Parameter | Range | Step |
| :--- | :--- | :--- |
| Motion penalty weight | 0.4 – 1.6 | 0.1 |
| History bonus weight | 0.005 – 0.030 | 0.005 |

The corresponding values are converted to Q12 before the production algorithm is executed.

During this sweep, `TH_CF_Q12` remains fixed at the value defined in `config.h`. Every combination of penalty and bonus weights is evaluated independently.

## Reference HR

For every 1024-sample processing window, the reference HR is calculated from valid ECG belt samples within the same window.

Only positive reference values are included in the average. The processing window is shifted by 256 samples after each calculation, matching the update behavior used by the firmware.

## Evaluation Metrics

Each tested parameter configuration produces the following metrics:

| Metric | Description |
| :--- | :--- |
| Reference windows | Windows containing a valid reference HR |
| Valid windows | Windows where the algorithm produced a valid HR |
| Valid ratio | Valid HR windows / reference windows |
| MAE | Mean absolute HR error |
| RMSE | Root mean square HR error |
| Bias | Mean signed HR error |
| Max error | Largest absolute HR error |
| Within 5 BPM | Fraction of valid estimates within ±5 BPM |
| Within 10% or 5 BPM | Fraction of valid estimates within the larger of ±10% or ±5 BPM |
| Effective 5 BPM | Fraction of all reference windows producing a valid estimate within ±5 BPM |
| Effective 10% or 5 BPM | Fraction of all reference windows producing a valid estimate within the 10% or 5 BPM tolerance |

The distinction between within and effective metrics is important. 

Within 5 BPM, for example, measures accuracy only among windows where the algorithm returned a valid HR:

$$Within_{5} = \frac{\text{valid estimates within 5 BPM}}{\text{valid estimates}}$$

Effective 5 BPM also accounts for windows where no HR result was produced:

$$Effective_{5} = \frac{\text{valid estimates within 5 BPM}}{\text{reference windows}}$$

This prevents a configuration with very high accuracy but very few valid results from appearing better than a configuration that provides reliable estimates more consistently.

## Parameter Selection

The sweep results are exported to CSV so that different configurations can be compared using both accuracy and result availability.

Parameter selection should therefore consider several metrics together rather than minimizing a single error value.