import json, sys

ref_file = sys.argv[1]
user_file = sys.argv[2]
cycle = int(sys.argv[3])

ref = json.load(open(ref_file))
usr = json.load(open(user_file))

r = ref[cycle]
u = usr[cycle]

print(f"=== Cycle {cycle} comparison ===")
for key in ["PC", "Exception", "ExceptionPC", "DecodedPCs"]:
    if r[key] != u[key]:
        print(f"  DIFF {key}: ref={r[key]} user={u[key]}")

rb = r["BusyBitTable"]
ub = u["BusyBitTable"]
for i in range(len(rb)):
    if rb[i] != ub[i]:
        print(f"  DIFF BusyBitTable[{i}]: ref={rb[i]} user={ub[i]}")

if len(r["ActiveList"]) != len(u["ActiveList"]):
    print(f"  DIFF ActiveList size: ref={len(r['ActiveList'])} user={len(u['ActiveList'])}")
for i in range(min(len(r["ActiveList"]), len(u["ActiveList"]))):
    if r["ActiveList"][i] != u["ActiveList"][i]:
        print(f"  DIFF ActiveList[{i}]: ref={r['ActiveList'][i]}")
        print(f"                        usr={u['ActiveList'][i]}")

if cycle > 0:
    print(f"\n=== Cycle {cycle-1} (previous) ===")
    rp = ref[cycle-1]
    up = usr[cycle-1]
    for key in ["PC", "Exception", "ExceptionPC"]:
        if rp[key] != up[key]:
            print(f"  DIFF {key}: ref={rp[key]} user={up[key]}")
    rpb = rp["BusyBitTable"]
    upb = up["BusyBitTable"]
    for i in range(len(rpb)):
        if rpb[i] != upb[i]:
            print(f"  DIFF BusyBitTable[{i}]: ref={rpb[i]} user={upb[i]}")
    print("  (no diffs)" if all(rpb[i] == upb[i] for i in range(len(rpb))) else "")
