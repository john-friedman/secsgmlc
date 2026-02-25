## SEC SGML C

Parsing SEC Standardized Generalized Markup Language in C. See [secsgml](https://github.com/john-friedman/secsgml) for a python interface.

Not tested at scale yet. Will test with datamule experimental branch later.

## Performance

- Parses samples/10k.txt in 4 ms. Extrapolates to 3GBps throughput on a single thread
- I/O is the slow part.

## Functions

- parse_sgml: parses the file and document metadata. takes bytes
- parse_submission_metadata: parses the submission metadata. takes bytes
- standardize_submission_metadata: standardizes the submission metadata
- uudecode: decodes SEC uuencoding
## Creating the executable

```gcc -O3 -march=native -o parsesgml.exe src/parsesgml.c src/secsgml.c src/uudecode.c src/standardize_submission_metadata.c```

## SEC Specific Quirks

SEC uuencoding has variable length lines. Needs special handling.

Normal uudecode:

![](uudecode_quirks/bad.jpg)

Corrected for SEC uudecode:

![](uudecode_quirks/good.jpg)
