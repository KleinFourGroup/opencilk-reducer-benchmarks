import os
import subprocess
import statistics


vector_lens = [2, 8, 16, 32, 64, 128]
worker_nums = [1, 2, 4, 8]
methods = {
    0: "serial",
    1: "associative",
    2: "commutative",
    3: "commutative_builtin",
}

reps = 3

INF = 10000


class Result(object):
    def __init__(self):
        self.times = []
        self.value = None
        self.extras = []

    def insert(self, input_str):
        time = input_str.split("\t")[0]
        value = input_str.split("\t")[1]
        extra = " ".join(input_str.split("\t")[2:])

        if self.value is None:
            self.value = value
        else:
            if self.value != value:
                raise AssertionError("Wrong answer!")
        self.times.append(float(time))
        self.extras.append(extra)

    @property
    def min(self):
        return min(self.times)

    @property
    def stdev(self):
        return statistics.stdev(self.times)

    def __str__(self):
        if not all(item == '' for item in self.extras):
            return "{:.6f}\t{:.5f}  \t{}".format(self.min, self.stdev, self.extras)
        else:
            return "{:.6f}\t{:.5f}".format(self.min, self.stdev)


def run_benchmark(worker_num, vector_len):
    print("Vector len {}, worker number {}".format(vector_len, worker_num))

    for method in methods:
        process = subprocess.Popen(
                "make clean".split(' '),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.STDOUT)
        process.wait()

        process = subprocess.Popen(
                "make VECTOR_LEN={} METHOD={}".format(vector_len, method).split(' '),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.STDOUT)
        process.wait()

        result = Result()

        for i in range(reps):
            command = "CILK_NWORKERS={} taskset -c 1-{} ./main".format(
                worker_num, worker_num)
            output = os.popen(command).read()
            output = output.strip()
            result.insert(output)

        print("{}\t{}".format(methods[method], result))


for worker_num in worker_nums:
    for vector_len in vector_lens:
        run_benchmark(worker_num, vector_len)
