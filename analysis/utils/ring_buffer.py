import numpy as np


class RingBuffer:
    def __init__(self, size, dtype=float):
        self.size = size
        self.data = np.zeros(size, dtype=dtype)
        self.write_index = 0
        self.count = 0

    def append(self, values):
        values = np.asarray(values, dtype=self.data.dtype)
        n = len(values)

        if n >= self.size:
            self.data[:] = values[-self.size:]
            self.write_index = 0
            self.count = self.size
            return

        end_index = self.write_index + n

        if end_index <= self.size:
            self.data[self.write_index:end_index] = values
        else:
            first_part = self.size - self.write_index
            second_part = n - first_part

            self.data[self.write_index:] = values[:first_part]
            self.data[:second_part] = values[first_part:]

        self.write_index = (self.write_index + n) % self.size
        self.count = min(self.size, self.count + n)

    def ready(self):
        return self.count == self.size

    def get_ordered(self):
        if self.count < self.size:
            return self.data[:self.count].copy()

        return np.concatenate((
            self.data[self.write_index:],
            self.data[:self.write_index]
        )).copy()