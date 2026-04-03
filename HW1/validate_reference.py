import subprocess, sys, os

tests_dir = "given_tests"
ref_sim = "reference_sim.py"
compare = "compare.py"
tmp = "tmp_ref_check.json"

passed = 0
failed = 0

for tnum in sorted(os.listdir(tests_dir)):
    tdir = os.path.join(tests_dir, tnum)
    if not os.path.isdir(tdir):
        continue
    inp = os.path.join(tdir, "input.json")
    ref = os.path.join(tdir, "output.json")
    desc = open(os.path.join(tdir, "desc.txt")).read().strip()

    # Run reference sim
    r = subprocess.run([sys.executable, ref_sim, inp, tmp],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print(f"[{tnum}] {desc}")
        print(f"  REFERENCE SIM ERROR: {r.stderr.strip()}")
        failed += 1
        continue

    # Compare with ground truth
    r2 = subprocess.run([sys.executable, compare, tmp, "-r", ref],
                        capture_output=True, text=True)
    output = r2.stdout + r2.stderr
    ok = "PASSED" in output
    print(f"[{tnum}] {desc}")
    if ok:
        print(f"  PASSED")
        passed += 1
    else:
        print(f"  FAILED")
        for line in output.splitlines():
            if "Error" in line or "Cycle" in line:
                print(f"    {line}")
        failed += 1

print(f"\n=== {passed} passed, {failed} failed ===")
if os.path.exists(tmp):
    os.remove(tmp)
