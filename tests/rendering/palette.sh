for i in {0..255} ; do
    printf "\x1b[38;5;%sm %3d \e[0m" "$i" "$i"
    if (( (i + 1) % 16 == 0 )); then
      printf "\n"
    fi
done
printf "\n"
for i in {0..255} ; do
    printf "\x1b[48;5;%sm %3d \e[0m" "$i" "$i"
    if (( (i + 1) % 16 == 0 )); then
      printf "\n"
    fi
done
printf "\n"
