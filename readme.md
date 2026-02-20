so we at 4ms to parse 15mb file. thread safe so yeah. 5ms ish to read
gcc -O3 -march=native -o parsesgml parsesgml.c secsgml.c uudecode.c


# User
- parse_sgml: parses the file and document metadata. takes bytews
- parse_submission_metadata: parses the submission metadata. takes bytews

probbaly ui names
parse_sgml_documents_with_metadata
parse_sgml_submission_metadata
standardize_submission_metadata (implemented)

so now its just python shit