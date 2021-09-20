from statistics import geometric_mean
import csv

raw_data = {}

columns = ("Hypermap", "Bitcode", "Peer")
#columns = ("Hypermap", "Lookup", "Bitcode", "Peer")

rwidth = 0.75

# I/O

def parse_file():
    fin = open("perf.csv")
    data_csv = csv.reader(fin, delimiter=";")
    for row in data_csv:
        test = row[0]
        if not test in raw_data:
            raw_data[test] = []
        if row[-1] == "0":
            prow = [bool(int(row[1])),
                    bool(int(row[2])),
                    bool(int(row[3])),
                    float(row[4])]
            raw_data[test].append(prow)

# Utils

def getmax(data):
    vals = [i[-1] for i in data if not isinstance(i[-1], str)]
    return max(vals)

def getkey(row):
    if isinstance(row[-1], str):
        return 0
    return row[-1]

# TeX Utils

def checkmark(val):
    if val:
        return "\\checkmark"
    return "$\\cdot$"

def rule(val, maxv):
    assert(val <= maxv)
    width = rwidth * (val / maxv)
    return "\\textcolor{{{0}}}{{\\rule{{{1:.3f}in}}{{6pt}}}}".format("red", width)

def columnformat(ncols):
    # start = "@{}"
    start = "@{\\extracolsep{\\fill}}"
    # sep = "@{{\\hspace*{{{}em}}}}".format(0.5)
    sep = ""
    return "{}|{}|D{{.}}{{.}}{{2.3}}l|".format(start, sep.join(['c'] * ncols))

def tablebegin():
    return "\\begin{{tabular*}}{{\\linewidth}}[t]{{{}}}".format(columnformat(len(columns)))

def tableheader(hasTime, name=None):
    head = ["\\textit{{{}}}".format(i) for i in columns]
    if hasTime:
        head.append("\\multicolumn{{1}}{{c}}{{\\textit{{{}}}}}".format("Time"))
        head.append("\\multicolumn{{1}}{{c|}}{{\\textit{{{}}}}}".format("Speedup"))
    else:
        head.append("\\multicolumn{{2}}{{c|}}{{\\textit{{{}}}}}".format("Speedup"))
    if name == None:
        return "\\hline\n{}\\\\\\hline".format(" & ".join(head))
    title = "\\multicolumn{{{}}}{{c}}{{\\textsc{{{}}}}}".format(len(columns) + 2, name)
    return "\\hline\n{}\\\\\\hline\n{}\\\\\\hline".format(title, " & ".join(head))

def getperf(row, maxv):
    if isinstance(row[-1], str):
        return ["\\multicolumn{{2}}{{c|}}{{{}}}".format("???")]
    return ["{:.3f}".format(row[-2]), rule(row[-1], maxv)]

def getrow(row, maxv):
    r = []
    for i in range(0,len(row) - 2):
        r.append(checkmark(row[i]))
    r.extend(getperf(row, maxv))
    return " & ".join(r) + "\\\\"

# Table prep

def addspeedup(data, overwrite=False):
    maxv = getmax(data)
    for row in data:
        if not isinstance(row[-1], str):
            ratio = maxv / row[-1]
            if overwrite:
                row[-1] = ratio
            row.append(ratio)
        else:
            if overwrite:
                row[-1] = "???"
            row[-1] = row.append("???")

def sorttable(raw_data):
    data = sorted(raw_data, key=getkey, reverse=False)
    return data

def preptable(raw_data, overwrite):
    addspeedup(data, overwrite)
    raw_data = sorted(raw_data, key=getkey, reverse=True)
    return raw_data
    
def printtable(perftable, hasTime):
    maxv = getmax(perftable)
    print(tablebegin())
    print(tableheader(hasTime))
    for row in perftable:
        print(getrow(row, maxv))
    print("\\hline\n\\end{tabular*}")

def optdata(raw_data):
    opt = []
    for test in raw_data:
        noopt = filter(lambda row: (not row[0]) and (not row[1]) and (not row[2]), raw_data[test])
        allopt = filter(lambda row: row[0] and row[1] and row[2], raw_data[test])
        opt.append([test, next(noopt)[-1] / next(allopt)[-1]])
    return opt

def mergetable(raw_data):
    for test in raw_data:
        addspeedup(raw_data[test], True)
    template = next(iter(raw_data.values()))
    ret = [row.copy() for row in template]
    for i in range(len(ret)):
        vals = [raw_data[test][i][-1] for test in raw_data]
        ret[i][-2] = geometric_mean(vals)
        ret[i][-1] = geometric_mean(vals)
    return sorted(ret, key=getkey, reverse=False)

"""
for data in raw_data:
    printtable(data, False)
   """

parse_file()
opt = optdata(raw_data)
data = mergetable(raw_data)
maxv = getmax(data)

#print(tablebegin())
#print(tableheader(False))
#for row in data:
#    print(getrow(row, maxv))
#print("\\hline\n\\end{tabular*}\n")
printtable(data, False)

for test in raw_data:
    tdata = sorttable(raw_data[test])
    maxv = getmax(tdata)
    print(tablebegin())
    print(tableheader(False, test))
    for row in tdata:
        print(getrow(row, maxv))
    print("\\hline\n\\end{tabular*}\n")

for row in opt:
    print("{} {:.3f}".format(row[0], row[-1]))