import json, sys

fname = sys.argv[1]
data = json.load(open(fname))

for i, c in enumerate(data):
    al_size = len(c["ActiveList"])
    iq_size = len(c["IntegerQueue"])
    exc = c["Exception"]
    excpc = c["ExceptionPC"]
    pc = c["PC"]
    dpcs = c["DecodedPCs"]
    print(f"Cycle {i}: PC={pc} Exception={exc} ExcPC={excpc} AL={al_size} IQ={iq_size} DPCs={dpcs}")
