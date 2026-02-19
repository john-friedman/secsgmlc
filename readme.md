gcc -O2 -o uudecode uudecode.cgcc -O2 -o uudecode uudecode.c

# PowerShell timing
Measure-Command { ./uudecode input.txt output.jpg --mode load }
Measure-Command { ./uudecode input.txt output.jpg --mode decode }
Measure-Command { ./uudecode input.txt output.jpg --mode decode+write }


decode is in ms, so ignore for now.
# benchmarks
load - 560ms - Measure-Command { 1..100 | ForEach-Object { ./uudecode input.txt output.jpg --mode load } }
decode - 560ms - Measure-Command { 1..100 | ForEach-Object { ./uudecode input.txt output.jpg --mode decode } }
write - 560ms - Measure-Command { 1..100 | ForEach-Object { ./uudecode input.txt output.jpg --mode decode+write } }


for the parse sgml, we want
- sub metadata (ignore for now)
    - just do kv
- document metadata (type, sequence, filename, description) or just do kv, just do kv man
- documwnts

gcc -O2 -o parsesgml parsesgml.c secsgml.c uudecode.c

# other shit
- for documents struct, can just count <DOCUMENT> to allocate memory i think.
- optimization
- secsgml size bytes
- header standardization
- filtering for only certain documents