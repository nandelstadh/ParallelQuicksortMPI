#!/bin/bash

OUT="results/strong_scale.csv"
echo "n,p,h,time" > "$OUT"

P=(1 2 4 8 16 32)
H=(1 2 3)

for h in "${H[@]}"; do
        for p in "${P[@]}"; do
                result=$(mpirun --bind-to none -np "$p" ./quicksort "/proj/uppmax2026-1-92/A3/inputs/input2000000000.txt" "output.txt" "$h")
                echo "$result" >> "$OUT"
        done
done

OUT="results/weak_scale.csv"
echo "n,p,h,time" > "$OUT"

N=("/proj/uppmax2026-1-92/A3/inputs/input125000000.txt" "/proj/uppmax2026-1-92/A3/inputs/input250000000.txt" "/proj/uppmax2026-1-92/A3/inputs/input500000000.txt" "/proj/uppmax2026-1-92/A3/inputs/input1000000000.txt" "/proj/uppmax2026-1-92/A3/inputs/input2000000000.txt")

P=(1 2 4 8 16)

for i in "${!N[@]}"; do
		result=$(mpirun --bind-to none -n "${P[i]}" ./quicksort "${N[i]}" "output.txt" 3)
		echo "$result" >> "$OUT"
done

