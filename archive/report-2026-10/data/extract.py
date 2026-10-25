import csv
import pathlib
import re
import sys

REPORT_DIR = pathlib.Path(__file__).resolve().parent
REPO_DIR = REPORT_DIR.parents[2]
LOG_DIR = REPO_DIR / "bench" / "log"
MACHINES = ("debian", "hppavillion")
STEPS = (
    "00-baseline",
    "01-block-io",
    "02-inline-bitio",
    "03-no-memmove",
    "04-compare-8-bytes",
    "05-avx512bw",
    "06-avx2",
    "07-lto-pgo",
)
STEP_LABELS = {
    "00-baseline": "00",
    "01-block-io": "01",
    "02-inline-bitio": "02",
    "03-no-memmove": "03",
    "04-compare-8-bytes": "04",
    "05-avx512bw": "05",
    "06-avx2": "06",
    "07-lto": "07a",
    "07-lto-pgo": "07b",
}
FILE_ORDER = (
    "mixed.dat", "random.dat", "text.dat", "zeros.dat", "dickens", "mozilla",
    "mr", "nci", "ooffice", "osdb", "reymont", "samba", "sao", "webster",
    "x-ray", "xml",
)


def write_table(name, header, rows):
    path = REPORT_DIR / name
    with path.open("w") as output:
        output.write(" ".join(header) + "\n")
        for row in rows:
            output.write(" ".join(str(value) for value in row) + "\n")


def steps_table(machine):
    text = (LOG_DIR / machine / "steps" / "steps.txt").read_text()
    rows = []
    for line in text.splitlines():
        fields = line.split()
        if not fields or fields[0] not in STEP_LABELS:
            continue
        compress, silesia, decompress = fields[1], fields[4], fields[6]
        rows.append((STEP_LABELS[fields[0]], compress, silesia, decompress,
                     fields[2].rstrip("x"), fields[3].rstrip("x"),
                     fields[7].rstrip("x"), fields[8].rstrip("x")))
    write_table(f"steps-{machine}.dat",
                ("step", "compress", "silesia", "decompress", "cvsprev",
                 "cvsbase", "dvsprev", "dvsbase"), rows)


def counter(summary, event, occurrence):
    matches = re.findall(r"^\s*([\d,]+)\s+" + re.escape(event), summary,
                         re.MULTILINE)
    return int(matches[occurrence].replace(",", ""))


def profile_table(machine):
    rows = []
    for step in STEPS:
        summary = (LOG_DIR / machine / step / "raw" / "profile"
                   / "summary.txt").read_text()
        compress_cycles = counter(summary, "cycles:u", 0)
        compress_instructions = counter(summary, "instructions:u", 0)
        compress_branches = counter(summary, "branches:u", 0)
        compress_misses = counter(summary, "branch-misses:u", 0)
        decompress_cycles = counter(summary, "cycles:u", 1)
        decompress_instructions = counter(summary, "instructions:u", 1)
        rows.append((
            STEP_LABELS[step],
            f"{compress_cycles / 1e6:.0f}",
            f"{compress_instructions / 1e6:.0f}",
            f"{compress_instructions / compress_cycles:.2f}",
            f"{compress_branches / 1e6:.0f}",
            f"{compress_misses / 1e6:.2f}",
            f"{100 * compress_misses / compress_branches:.2f}",
            f"{decompress_cycles / 1e6:.0f}",
            f"{decompress_instructions / 1e6:.0f}",
            f"{decompress_instructions / decompress_cycles:.2f}",
        ))
    write_table(f"profile-{machine}.dat",
                ("step", "ccycles", "cinstr", "cipc", "cbranches", "cmisses",
                 "cmissrate", "dcycles", "dinstr", "dipc"), rows)


def per_file_times(machine, step):
    times = {}
    path = LOG_DIR / machine / step / "raw" / "bench.csv"
    with path.open() as source:
        for record in csv.DictReader(source):
            times[record["file"]] = (float(record["compress_s"]),
                                     float(record["decompress_s"]),
                                     int(record["size"]))
    return times


def per_file_table(machine):
    by_step = {step: per_file_times(machine, step) for step in STEPS}
    rows = []
    for index, file_name in enumerate(FILE_ORDER):
        baseline = by_step["00-baseline"][file_name]
        row = [index, file_name, f"{baseline[2] / 1e6:.1f}"]
        for step in ("00-baseline", "02-inline-bitio", "03-no-memmove",
                     "04-compare-8-bytes", "07-lto-pgo"):
            row.append(f"{by_step[step][file_name][0]:.4f}")
        before = by_step["02-inline-bitio"][file_name][0]
        after = by_step["03-no-memmove"][file_name][0]
        row.append(f"{before / after:.2f}")
        before = by_step["03-no-memmove"][file_name][0]
        after = by_step["04-compare-8-bytes"][file_name][0]
        row.append(f"{before / after:.2f}")
        row.append(f"{baseline[0] / by_step['07-lto-pgo'][file_name][0]:.2f}")
        row.append(f"{baseline[1] / by_step['07-lto-pgo'][file_name][1]:.2f}")
        rows.append(row)
    write_table(f"perfile-{machine}.dat",
                ("index", "file", "mb", "t00", "t02", "t03", "t04", "t07",
                 "s14", "s15", "stotal", "dtotal"), rows)


def throughput_table(machine):
    rows = []
    for step in STEPS:
        times = per_file_times(machine, step)
        size = sum(entry[2] for entry in times.values())
        compress = sum(entry[0] for entry in times.values())
        decompress = sum(entry[1] for entry in times.values())
        rows.append((STEP_LABELS[step], f"{compress:.3f}",
                     f"{decompress:.3f}", f"{size / 1e6 / compress:.1f}",
                     f"{size / 1e6 / decompress:.1f}"))
    write_table(f"wall-{machine}.dat",
                ("step", "compress", "decompress", "cmbs", "dmbs"), rows)


def paths_table(machine, columns):
    text = (LOG_DIR / machine / "06-avx2" / "raw" / "followup"
            / "ab-cycles.txt").read_text()
    rows = []
    for line in text.splitlines():
        fields = line.split()
        if fields and fields[0] in FILE_ORDER:
            values = [fields[1 + column] for column in columns]
            rows.append([FILE_ORDER.index(fields[0]), fields[0], *values])
    return rows


def scaling_tables():
    instructions = {}
    for line in (REPORT_DIR / "scaling-raw.txt").read_text().splitlines():
        fields = line.split()
        if len(fields) != 6 or not fields[1].isdigit():
            continue
        file_name, size_mb, binary = fields[0], int(fields[1]), fields[2]
        instructions.setdefault((file_name, size_mb, binary), []).append(
            int(fields[5]))
    for file_name in ("random", "text", "zeros"):
        rows = []
        sizes = sorted({key[1] for key in instructions if key[0] == file_name})
        for size_mb in sizes:
            base = min(instructions[(file_name, size_mb, "base")])
            final = min(instructions[(file_name, size_mb, "final")])
            rows.append((size_mb, f"{base / 1e9:.3f}", f"{final / 1e9:.3f}",
                         f"{base / (size_mb * 1e6):.1f}",
                         f"{final / (size_mb * 1e6):.1f}"))
        write_table(f"scaling-{file_name}.dat",
                    ("mb", "base_ginstr", "final_ginstr", "base_ipb",
                     "final_ipb"), rows)


def main():
    scaling_tables()
    for machine in MACHINES:
        steps_table(machine)
        profile_table(machine)
        per_file_table(machine)
        throughput_table(machine)
    write_table("paths-hppavillion.dat",
                ("index", "file", "avx512", "avx2", "word"),
                paths_table("hppavillion", (1, 2, 3)))
    write_table("paths-debian.dat", ("index", "file", "word", "avx2"),
                paths_table("debian", (1, 2)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
