import subprocess, sys, os, json

extra_dir = "extra_tests"
ref_sim   = "reference_sim.py"
compare   = "compare.py"
simulator = "./simulator"   # C++ binary (Linux path, run from Docker)

passed = 0
failed = 0

for tnum in sorted(os.listdir(extra_dir)):
    tdir = os.path.join(extra_dir, tnum)
    if not os.path.isdir(tdir):
        continue
    inp      = os.path.join(tdir, "input.json")
    ref_out  = os.path.join(tdir, "output.json")
    user_out = os.path.join(tdir, "user_output.json")
    desc     = open(os.path.join(tdir, "desc.txt")).read().strip()

    print(f"[{tnum}] {desc}")

    # Step 1: generate reference output
    r = subprocess.run([sys.executable, ref_sim, inp, ref_out],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print(f"  REFERENCE SIM ERROR: {r.stderr.strip()}")
        failed += 1
        continue

    # Step 2: count reference cycles
    ref_cycles = len(json.load(open(ref_out)))
    print(f"  Reference: {ref_cycles} cycles")

    # Step 3: run C++ simulator (only works from Linux/Docker — skip gracefully)
    if not os.path.exists(simulator):
        print(f"  (C++ simulator not found at '{simulator}' — skipping comparison)")
        print()
        continue

    r2 = subprocess.run([simulator, inp, user_out],
                        capture_output=True, text=True)
    if r2.returncode != 0:
        print(f"  C++ SIMULATOR ERROR: {r2.stderr.strip()}")
        failed += 1
        print()
        continue

    # Step 4: compare
    r3 = subprocess.run([sys.executable, compare, user_out, "-r", ref_out],
                        capture_output=True, text=True)
    output = r3.stdout + r3.stderr
    ok = "PASSED" in output
    if ok:
        print(f"  PASSED")
        passed += 1
    else:
        print(f"  FAILED")
        for line in output.splitlines():
            if "Error" in line or "Cycle" in line:
                print(f"    {line}")
        failed += 1
    print()

if os.path.exists(simulator):
    print(f"=== {passed} passed, {failed} failed ===")
else:
    print("=== C++ simulator not found. Reference outputs generated. Run from Docker/WSL. ===")
