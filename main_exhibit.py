from statistics import geometric_mean
import csv

from matplotlib import pyplot as plt

class dbtable:
    def __init__(self, fields):
        self.fields = fields
        self.rows = []
    
    def addRow(self, row):
        self.rows.append(row)
    
    def getTuples(self, fields):
        indices = [self.fields.index(f) for f in fields]
        res = []
        for row in self.rows:
            tup = tuple([row[i] for i in indices])
            if not tup in res:
                res.append(tup)
        return res
    
    def expunge(self, predFields, predVals):
        res = dbtable(self.fields)
        pIndices = [self.fields.index(f) for f in predFields]
        for row in self.rows:
            toComp = tuple([row[i] for i in pIndices])
            if not predVals == toComp:
                res.addRow(row.copy())
        return res
    
    def filter(self, predFields, predVals):
        res = dbtable(self.fields)
        pIndices = [self.fields.index(f) for f in predFields]
        for row in self.rows:
            toComp = tuple([row[i] for i in pIndices])
            if predVals == toComp:
                res.addRow(row.copy())
        return res
    
    def getVals(self, targFields, predFields = None, predVals = None):
        if not predFields == None:
            pIndices = [self.fields.index(f) for f in predFields]
        else:
            pIndices = [i for i in range(len(self.fields))]
        tIndices = [self.fields.index(f) for f in targFields]
        res = []
        for row in self.rows:
            toComp = tuple([row[i] for i in pIndices])
            if predFields == None or predVals == toComp:
                vals = tuple([row[i] for i in tIndices])
                res.append(vals)
        return res
    
    def update(self, fields, fn):
        indices = [self.fields.index(f) for f in fields]
        for row in self.rows:
            args = tuple([row[i] for i in indices])
            res = fn(args)
            for i in range(len(fields)):
                row[indices[i]] = res[i]
    
    def sort(self, fields, fn, flip = False):
        indices = [self.fields.index(f) for f in fields]
        self.rows.sort(reverse = flip,
                        key = lambda row: fn(tuple([row[i] for i in indices])))

    def merge(self, db):
        for row in db.rows:
            self.addRow(row)

    def prettyprint(self):
        print(self.fields)
        for row in self.rows:
            print(row)



def parse_opt_file():
    fin = open("../archive/perf_opt.csv")
    data_csv = csv.reader(fin, delimiter="\t")
    opts = dbtable(("test", "workers", "runtime", "integration", "compiler", "performance"))
    for row in data_csv:
        if row[-1] == "0":
            prow = [row[0],
                    int(row[1]),
                    bool(int(row[2])),
                    bool(int(row[3])),
                    bool(int(row[4])),
                    float(row[5])]
            opts.addRow(prow)
    fin.close()
    return opts

def parse_comm_file():
    fin = open("../archive/perf_comm.csv")
    data_csv = csv.reader(fin, delimiter="\t")
    comm = dbtable(("test", "workers", "reducer", "runtime", "integration", "compiler", "performance"))
    for row in data_csv:
        if row[-1] == "0":
            prow = [row[0],
                    int(row[1]),
                    int(row[2]),
                    bool(int(row[3])),
                    bool(int(row[4])),
                    bool(int(row[5])),
                    float(row[6])]
            comm.addRow(prow)
    fin.close()
    return comm

def parse_scale_file():
    fin = open("../archive/perf_scale.csv")
    data_csv = csv.reader(fin, delimiter="\t")
    scale = dbtable(("test", "workers", "bins",
                        "merges", "steals",
                        "runtime", "integration", "compiler",
                        "performance", "aux"))
    for row in data_csv:
        if row[-1] == "0":
            prow = [row[0],
                    int(row[1]),
                    int(row[2]),
                    int(row[3]),
                    int(row[4]),
                    bool(int(row[5])),
                    bool(int(row[6])),
                    bool(int(row[7])),
                    float(row[8]),
                    float(row[9])]
            scale.addRow(prow)
    fin.close()
    return scale

def normalize_opt_db(db):
    tests = db.getTuples(("test", "workers"))
    res = dbtable(db.fields)
    for test in tests:
        test_db = db.filter(("test", "workers"), test)
        times = test_db.getVals(("performance",))
        maxv = max([t[0] for t in times])
        test_db.update(("performance",), lambda perf: (maxv / perf[0],))
        res.merge(test_db)
    return res

def mean_opt_db(db):
    configs = db.getTuples(("workers", "runtime", "integration", "compiler"))
    res = dbtable(("workers", "runtime", "integration", "compiler", "performance"))
    for confs in configs:
        confs_db = db.filter(("workers", "runtime", "integration", "compiler"), confs)
        perfs = confs_db.getVals(("performance",))
        gm = geometric_mean([perf[0] for perf in perfs])
        row = list(confs)
        row.append((1 - 1 / gm) * 100)
        res.addRow(row)
    return res



def print_opt_table(db):
    db = normalize_opt_db(db)
    db = mean_opt_db(db)
    db.sort(("performance",), lambda perf: perf[0])
    rows = db.getVals(("runtime", "integration", "compiler", "performance"))
    maxv = rows[-1][3]
    rwidth = 0.75
    print("\\begin{tabular*}{\\linewidth}[t]{@{\\extracolsep{\\fill}}|ccc|D{.}{.}{2.3}l|}")
    print("\\hline")
    print("\\textit{Runtime} & \\textit{Integration} & \\textit{Compiler} & \\multicolumn{2}{c|}{\\textit{Speedup}}\\\\\\hline")
    for row in rows:
        spa = "\\checkmark" if row[0] else "$\\cdot$"
        bitcode = "\\checkmark" if row[1] else "$\\cdot$"
        peer = "\\checkmark" if row[2] else "$\\cdot$"
        width = (row[3] / maxv) * rwidth
        rule = "\\textcolor{{red}}{{\\rule{{{0:.3f}in}}{{6pt}}}}".format(width)
        print("{} & {} & {} & {:.2f}\\% & {} \\\\".format(spa, bitcode, peer, row[3], rule))
    print("\\hline\n\\end{tabular*}\n")

def per_opt_db(db, opt):
    opts = ("runtime", "integration", "compiler")
    res = dbtable(("test", "workers", "performance"))
    tests = db.getTuples(("test", "workers"))
    for test in tests:
        test_db = db.filter(("test", "workers"), test)
        target = [False, False, False]
        unoptperf = test_db.getVals(("performance",),
                                    ("runtime", "integration", "compiler"),
                                    tuple(target))
        target[opt] = True
        optperf = test_db.getVals(("performance",),
                                    ("runtime", "integration", "compiler"),
                                    tuple(target))
        row = list(test)
        row.append((1 - optperf[0][0] / unoptperf[0][0]) * 100)
        res.addRow(row)
    return res

def print_per_opt_table(db, opt):
    db = per_opt_db(db, opt)
    rows = db.getVals(("test", "performance"))
    db.sort(("performance",), lambda perf: abs(perf[0]))
    sorted_rows = db.getVals(("performance",))
    maxv = sorted_rows[-1][0]
    rwidth = 1.5
    print("\\begin{tabular*}{\\linewidth}[t]{@{\\extracolsep{\\fill}}|c|D{.}{.}{2.3}l|}")
    print("\\hline")
    print("\\textit{Benchmark} & \\multicolumn{2}{c|}{\\textit{Speedup}}\\\\\\hline")
    for row in rows:
        bench = "\\texttt{{{}}}".format(row[0].replace("_", "\\_"))
        width = (abs(row[1]) / maxv) * rwidth
        rule = "\\textcolor{{{0}}}{{\\rule{{{1:.3f}in}}{{6pt}}}}".format("red" if row[1] > 0 else "blue", width)
        print("{} & {:.2f}\\% & {} \\\\".format(bench, row[1], rule))
    print("\\hline\n\\end{tabular*}\n")

def speedup_comm_db(db):
    confs = db.getTuples(("test", "workers", "runtime", "integration", "compiler"))
    res = dbtable(("test", "workers", "runtime", "integration", "compiler", "performance"))
    for conf in confs:
        target = list(conf)
        target.append(None)
        target[-1] = 1
        unoptperf = db.getVals(("performance",),
                                ("test", "workers", "runtime", "integration", "compiler", "reducer"),
                                tuple(target))
        target[-1] = 2
        optperf = db.getVals(("performance",),
                                ("test", "workers", "runtime", "integration", "compiler", "reducer"),
                                tuple(target))
        row = list(conf)
        row.append((1 - optperf[0][0] / unoptperf[0][0]) * 100)
        res.addRow(row)
    return res

def print_comm_table(db):
    db = speedup_comm_db(db)
    tests = db.getTuples(("test", "workers"))
    rwidth = 0.7
    print("\\begin{tabular*}{\\linewidth}[t]{@{\\extracolsep{\\fill}}|c|cc|D{.}{.}{2.3}l|}")
    print("\\hline")
    print("\\textit{Benchmark} & \\textit{Integration} & \\textit{Compiler} & \\multicolumn{2}{c|}{\\textit{Speedup}}\\\\\\hline")
    for test in tests:
        test_db = dbtable(("test", "runtime", "integration", "compiler", "performance"))
        rows = db.getVals(("runtime", "integration", "compiler", "performance"),
                            ("test", "workers"),
                            test)
        for row in rows:
            mrow = [test[0]]
            mrow.extend(row)
            test_db.addRow(mrow)
        test_db.sort(("performance",), lambda perf: abs(perf[0]))
        rows = test_db.getVals(("test", "runtime", "integration", "compiler", "performance"))
        maxv = abs(rows[-1][4])
        test_db.sort(("performance",), lambda perf: perf[0])
        rows = test_db.getVals(("test", "runtime", "integration", "compiler", "performance"))
        for row in rows:
            bench = "\\texttt{{{}}}".format(row[0].replace("_", "\\_"))
            bitcode = "\\checkmark" if row[2] else "$\\cdot$"
            peer = "\\checkmark" if row[3] else "$\\cdot$"
            width = (abs(row[4]) / maxv) * rwidth
            rule = "\\textcolor{{{0}}}{{\\rule{{{1:.3f}in}}{{6pt}}}}".format("red" if row[4] > 0 else "blue", width)
            print("{} & {} & {} & {:.2f}\\% & {} \\\\".format(bench, bitcode, peer, row[4], rule))
        print("\\hline")
    print("\\end{tabular*}\n")

def process_scale_db(db):
    db = db.filter(("runtime", "integration", "compiler"), (True, False, False))
    res = dbtable(("test", "bins", "tau", "steals", "performance"))
    rows = db.getVals(("test", "bins", "merges", "steals", "performance"))
    for row in rows:
        tau = row[2] / row[3]
        res.addRow([row[0], row[1], tau, row[3], row[4]])
    return res

def make_fig(x_vals, y_vals, x_label, y_label, file_name):
    ymin = min(y_vals)
    ymax = max(y_vals)
    plt.plot(x_vals, y_vals)
    plt.xlabel(x_label, fontsize=24)
    plt.ylabel(y_label, fontsize=24)
    ax = plt.gca()
    ax.set_ylim([0, ymax + ymin])
    ax.tick_params(axis='both', which='major', labelsize=18)
    #ax.locator_params(nbins=6, axis='x')
    plt.savefig(file_name, bbox_inches='tight')
    plt.clf()

def print_scale_db(db):
    db = process_scale_db(db)
    tests = db.getTuples(("test",))
    fout = open("scale.csv", "w", newline='')
    csvout = csv.writer(fout, delimiter='\t')
    for test in tests:
        ind = tests.index(test)
        bin_plt = []
        tau_plt = []
        steal_plt = []
        perf_plt = []
        test_db = db.filter(("test",), test)
        test_db.sort(("bins",), lambda tup: abs(tup[0]))
        bin_rows = test_db.getVals(("bins", "performance", "steals"))
        for row in bin_rows:
            bin_plt.append(row[0])
            perf_plt.append(row[1])
            steal_plt.append(row[2])
        make_fig(bin_plt, steal_plt,
                 "Histogram bins",
                 "Steals",
                 '{}_bin_steal.png'.format(ind))
        make_fig(bin_plt, perf_plt,
                 "Histogram bins",
                 "Performance",
                 '{}_bin_perf.png'.format(ind))
        #
        test_db.sort(("tau",), lambda tup: abs(tup[0]))
        tau_rows = test_db.getVals(("tau", "performance", "steals"))
        steal_plt = []
        perf_plt = []
        for row in tau_rows:
            tau_plt.append(row[0])
            perf_plt.append(row[1])
            steal_plt.append(row[2])
        make_fig(tau_plt, steal_plt,
                 "Average merges per steal",
                 "Steals",
                 '{}_tau_steal.png'.format(ind))
        make_fig(tau_plt, perf_plt,
                 "Average merges per steal",
                 "Performance",
                 '{}_tau_perf.png'.format(ind))
        #
        csvout.writerow(["test", "bins", "performance",
                                "average merges per steal", "performance", "steals"])
        for i in range(len(bin_rows)):
            csvout.writerow([test[0], bin_rows[i][0], bin_rows[i][1],
                                        tau_rows[i][0], tau_rows[i][1], tau_rows[i][2]])
    fout.close()

opt_db = parse_opt_file()
opt_db = opt_db.filter(("workers",), (1,))
print_opt_table(opt_db)
for opt in range(3):
    print_per_opt_table(opt_db, opt)

comm_db = parse_comm_file()
comm_db = comm_db.expunge(("reducer",), (0,))
comm_db = comm_db.filter(("workers",), (8,))
print_comm_table(comm_db)

scale_db = parse_scale_file()
scale_db = scale_db.filter(("workers",), (8,))
tests = scale_db.getTuples(("test",))
aux_db = scale_db.filter(("test",), tests[-1],)
scale_db = scale_db.expunge(("test",), tests[-1],)
print_scale_db(scale_db)