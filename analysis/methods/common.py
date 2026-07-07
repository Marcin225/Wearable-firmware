import numpy as np

HISTORY_SIZE = 8
BUFFER_SIZE = 200


def medianFilter(current, buffer):
    values = [int(current), int(buffer[0]), int(buffer[1])]
    values.sort()

    return values[1]


def lowPassFilter(sample, prevOutput, shift=2):
    output = ((int(sample) - int(prevOutput)) >> shift) + int(prevOutput)

    return output


def highPassFilter(sample, prevFilterState, alpha=31739):
    filterState = (int(sample) << 15) + ((int(alpha) * int(prevFilterState)) >> 15)
    output = (filterState - int(prevFilterState)) >> 15

    return output, filterState


def medianResult(arr):
    arr = [int(x) for x in arr]
    arr.sort()
    n = len(arr)

    if n == 0:
        return 0

    if n & 1:
        return arr[n // 2]

    return (arr[n // 2 - 1] + arr[n // 2]) >> 1


class OpticalChannel:
    def __init__(self):
        self.reset()

    def reset(self):
        self.hpState = 0
        self.lpOut = 0
        self.medBuffer = [0, 0]
        self.sum = 0

    def resetSum(self):
        self.sum = 0

    def process(self, rawSample):
        rawSample = int(rawSample)

        clean = medianFilter(rawSample, self.medBuffer)

        self.medBuffer[1] = self.medBuffer[0]
        self.medBuffer[0] = rawSample

        self.sum += clean

        ac, self.hpState = highPassFilter(clean, self.hpState, 31739)
        self.lpOut = lowPassFilter(ac, self.lpOut, 2)

        return self.lpOut

    def getSum(self):
        return self.sum


class MedianOnlyChannel:
    def __init__(self):
        self.medBuffer = [0, 0]
        self.sum = 0

    def resetSum(self):
        self.sum = 0

    def process(self, rawSample):
        rawSample = int(rawSample)

        clean = medianFilter(rawSample, self.medBuffer)

        self.medBuffer[1] = self.medBuffer[0]
        self.medBuffer[0] = rawSample

        self.sum += clean

        return clean

    def getSum(self):
        return self.sum


class AxisFilter:
    def __init__(self):
        self.reset()

    def reset(self):
        self.lpOut = 0

    def process(self, rawSample):
        rawSample = int(rawSample)
        self.lpOut = lowPassFilter(rawSample, self.lpOut)

        return rawSample - self.lpOut


def estimateHR_peak(peaks, fs, MAX_BEATS=12):
    hr = 0
    confidence_score = 0

    HR_MIN = 40
    HR_MAX = 160

    I_MIN = (fs * 60) // HR_MAX
    I_MAX = (fs * 60) // HR_MIN

    intervals = []

    for i in range(1, len(peaks)):
        d = peaks[i] - peaks[i - 1]

        if d >= I_MIN and d <= I_MAX and len(intervals) < MAX_BEATS:
            intervals.append(d)

    if len(intervals) == 0:
        return int(hr), int(confidence_score)

    med = medianResult(intervals)

    if len(intervals) >= 2:
        dev = [abs(x - med) for x in intervals]

        medianDeviation = medianResult(dev)

        perfectDeviation = (med * 5) // 100
        badDeviation = (med * 30) // 100

        if perfectDeviation >= medianDeviation:
            confidence_score = 100
        elif badDeviation <= medianDeviation:
            confidence_score = 0
        else:
            confidence_score = 100 - (((medianDeviation - perfectDeviation) * 100) // (badDeviation - perfectDeviation))

    elif len(intervals) == 1:
        confidence_score = 35

    if med > 0:
        hr = fs * 60 // med

    return int(hr), int(confidence_score)


def estimateHR_autocorr(sig, fs):
    hr = 0
    confidence_score = 0

    sig = np.asarray(sig, dtype=np.int64)
    n = len(sig)

    HR_MIN = 40
    HR_MAX = 160

    lagMin = (fs * 60) // HR_MAX
    lagMax = (fs * 60) // HR_MIN

    sumAbs = 0
    sumMean = 0

    for i in range(n):
        a = abs(int(sig[i]))

        if a > 600:
            a = 600

        sumAbs += a
        sumMean += int(sig[i])

    meanAbs = sumAbs // n
    meanSig = sumMean // n

    if meanAbs < 4:
        return int(hr), int(confidence_score)

    elif meanAbs >= 40:
        confidence_score = 100

    else:
        confidence_score = ((meanAbs - 4) * 100) // (40 - 4)

    bestLag = 0
    bestScore = 0
    secondBestScore = 0

    if lagMax - lagMin + 1 > 128 or lagMax - lagMin + 1 <= 0:
        return int(hr), int(confidence_score)

    scoreTab = [0] * (lagMax - lagMin + 1)

    for lag in range(lagMin, lagMax + 1):
        score = 0

        for i in range(n - lag):
            x1 = int(sig[i]) - meanSig
            x2 = int(sig[i + lag]) - meanSig

            score += x1 * x2

        if score > bestScore:
            bestScore = score
            bestLag = lag

        scoreTab[lag - lagMin] = score

    for lag in range(lagMin, lagMax + 1):
        if lag >= bestLag - 15 and lag <= bestLag + 15:
            continue

        index = lag - lagMin

        if scoreTab[index] > secondBestScore:
            secondBestScore = scoreTab[index]

    scoreRatio = 0

    if bestScore > 0:
        scoreRatio = (bestScore - secondBestScore) * 100 // bestScore

    if scoreRatio >= 40:
        confidence_score += 30
    elif scoreRatio >= 30:
        confidence_score += 20
    elif scoreRatio >= 15:
        confidence_score += 10
    else:
        confidence_score -= 10

    bestLag_x2 = bestLag * 2

    if bestLag_x2 <= lagMax:
        idx = bestLag_x2 - lagMin
        score_2 = scoreTab[idx]

        if score_2 * 100 > bestScore * 80:
            confidence_score -= 40

    if confidence_score > 100:
        confidence_score = 100

    if confidence_score < 0:
        confidence_score = 0

    if bestLag == 0:
        return int(hr), int(confidence_score)

    hr = (fs * 60) // bestLag

    if hr < HR_MIN or hr > HR_MAX:
        confidence_score = 0
        hr = 0
        return int(hr), int(confidence_score)

    return int(hr), int(confidence_score)


class PeakAutocorrHR:
    def __init__(self, fs=100):
        self.fs = fs

        self.hr_history = [0] * HISTORY_SIZE
        self.hr_idx = 0
        self.hr_count = 0
        self.reject_counter_hr = 0

    def calculate(self, data, samplingRate=None):
        if samplingRate is None:
            samplingRate = self.fs

        peakConfScore = 0
        autoConfScore = 0
        hr = 0

        acIr = data["acIr"]

        if data["dcIr"] <= 0 or data["dcRed"] <= 0:
            return int(hr), int(peakConfScore), int(autoConfScore)

        MIN_PEAK_DIST = samplingRate * 450 // 1000

        if self.hr_count == HISTORY_SIZE:
            last_hr = medianResult(self.hr_history)

            if last_hr > 0:
                MIN_PEAK_DIST = samplingRate * 60 // last_hr * 60 // 100

        if MIN_PEAK_DIST < 30:
            MIN_PEAK_DIST = 30

        if MIN_PEAK_DIST > 150:
            MIN_PEAK_DIST = 150

        MAX_BEATS = 12
        sumAbs = 0

        for i in range(BUFFER_SIZE):
            v = abs(int(acIr[i]))

            if v > 600:
                v = 600

            sumAbs += v

        meanAbs = sumAbs // BUFFER_SIZE

        if meanAbs < 6:
            return int(hr), int(peakConfScore), int(autoConfScore)

        thr = meanAbs * 3 // 4

        if thr < 8:
            thr = 8

        if thr > 250:
            thr = 250

        peaks = []
        lastPeakIndex = -MIN_PEAK_DIST

        for i in range(1, BUFFER_SIZE - 1):
            v0 = int(acIr[i])

            if v0 < -thr and v0 < acIr[i - 1] and v0 < acIr[i + 1]:
                if i - lastPeakIndex >= MIN_PEAK_DIST:
                    peaks.append(i)
                    lastPeakIndex = i

                    if len(peaks) >= MAX_BEATS:
                        break

        if len(peaks) < 2:
            return int(hr), int(peakConfScore), int(autoConfScore)

        hr_peak, peakConfScore = estimateHR_peak(peaks, fs=samplingRate, MAX_BEATS=MAX_BEATS)
        hr_autocorr, autoConfScore = estimateHR_autocorr(acIr, fs=samplingRate)

        if autoConfScore > 65 and peakConfScore > 65 and abs(hr_peak - hr_autocorr) <= 8:
            hr = (hr_peak + hr_autocorr) // 2
        elif autoConfScore >= 70 and autoConfScore > peakConfScore:
            hr = hr_autocorr
        elif peakConfScore >= 75 and autoConfScore < peakConfScore:
            hr = hr_peak
        else:
            hr = 0

        if hr == 0:
            self.reject_counter_hr = 0
            return int(hr), int(peakConfScore), int(autoConfScore)

        if hr > 0 and self.hr_count == HISTORY_SIZE:
            last_hr = medianResult(self.hr_history)

            if abs(hr - last_hr) > 10:
                self.reject_counter_hr += 1

                if self.reject_counter_hr < 4:
                    hr = 0
                else:
                    self.hr_count = 0
                    self.hr_idx = 0
                    self.reject_counter_hr = 0
            else:
                self.reject_counter_hr = 0

        if hr > 0:
            self.hr_history[self.hr_idx] = int(hr)
            self.hr_idx = (self.hr_idx + 1) & (HISTORY_SIZE - 1)

            if self.hr_count < HISTORY_SIZE:
                self.hr_count += 1

        return int(hr), int(peakConfScore), int(autoConfScore)