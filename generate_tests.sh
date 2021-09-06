awk -F'[	 ,"{]+' '
FNR==1{f++}
BEGIN {
  file_paths = "test_finnish_512 test_random test_zero"
  split(file_paths, files, " ")
}
/list_start/,/list_end/{
  if (!$3) next;
  if (f==1)mem[$3] = $2;
  if (f==2)transform[$3] = $2;
  if (f==3)compress[$3] = $2;
}
END {
  for (f in files)
  for (m in mem)
  for (c in compress) {
    if (compress[c] == "POINTER") {
      for (t in transform)
        if (transform[t] == mem[m])
          print "./test.sh " m " " c " " t " " files[f];
    } else {
      if (compress[c] == mem[m])
        print "./test.sh " m " " c " - " files[f];
    }
  } 
}' mem.c transform.c compress.c | shuf > tests.sh
chmod +x tests.sh
